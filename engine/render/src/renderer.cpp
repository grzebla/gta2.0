#include "gta20/render/renderer.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <cstring>
#include <optional>
#include <string_view>
#include <utility>

#include "gta20/render/transfer_size.hpp"

extern const unsigned char gta20_quad_vert_spv[];
extern const std::size_t gta20_quad_vert_spv_size;
extern const unsigned char gta20_quad_frag_spv[];
extern const std::size_t gta20_quad_frag_spv_size;
extern const unsigned char gta20_quad_msl[];
extern const std::size_t gta20_quad_msl_size;
#if defined(GTA20_D3D_SHADER_FORMAT_DXIL) || defined(GTA20_D3D_SHADER_FORMAT_DXBC)
extern const unsigned char gta20_quad_vs_d3d[];
extern const std::size_t gta20_quad_vs_d3d_size;
extern const unsigned char gta20_quad_ps_d3d[];
extern const std::size_t gta20_quad_ps_d3d_size;
#endif

namespace gta20::render {
namespace {

std::string sdl_error(std::string_view what) {
    return std::string(what) + ": " + SDL_GetError();
}

constexpr SDL_GPUShaderFormat kD3DFormat =
#if defined(GTA20_D3D_SHADER_FORMAT_DXIL)
    SDL_GPU_SHADERFORMAT_DXIL;
#elif defined(GTA20_D3D_SHADER_FORMAT_DXBC)
    SDL_GPU_SHADERFORMAT_DXBC;
#else
    SDL_GPU_SHADERFORMAT_INVALID;
#endif

const char* default_driver() {
#if defined(__APPLE__)
    return "metal";
#elif defined(_WIN32)
    return kD3DFormat != SDL_GPU_SHADERFORMAT_INVALID ? "direct3d12" : "vulkan";
#else
    return "vulkan";
#endif
}

// Two triangles covering the unit quad [-0.5, 0.5]^2.
constexpr std::array<float, 12> kQuadCorners = {-0.5f, -0.5f, 0.5f, -0.5f, 0.5f,  0.5f,
                                                -0.5f, -0.5f, 0.5f, 0.5f,  -0.5f, 0.5f};

void release(SDL_GPUDevice* device, SDL_GPUTexture* texture) {
    SDL_ReleaseGPUTexture(device, texture);
}
void release(SDL_GPUDevice* device, SDL_GPUBuffer* buffer) {
    SDL_ReleaseGPUBuffer(device, buffer);
}
void release(SDL_GPUDevice* device, SDL_GPUTransferBuffer* transfer) {
    SDL_ReleaseGPUTransferBuffer(device, transfer);
}

/// One GPU object released on scope exit unless take() hands it over. SDL defers the release
/// until command buffers that still use the object have completed.
template <class T>
class Owned {
public:
    Owned(SDL_GPUDevice* device, T* object) : device_(device), object_(object) {}
    ~Owned() {
        if (object_ != nullptr) {
            release(device_, object_);
        }
    }
    Owned(const Owned&) = delete;
    Owned& operator=(const Owned&) = delete;
    Owned(Owned&&) = delete;
    Owned& operator=(Owned&&) = delete;

    explicit operator bool() const { return object_ != nullptr; }
    [[nodiscard]] T* get() const { return object_; }
    [[nodiscard]] T* take() { return std::exchange(object_, nullptr); }

private:
    SDL_GPUDevice* device_;
    T* object_;
};

/// A command buffer cancelled on scope exit unless it was submitted. Once a swapchain texture has
/// been acquired SDL forbids cancelling, so submit_on_exit() makes the exit submit it instead.
class PendingCommands {
public:
    explicit PendingCommands(SDL_GPUCommandBuffer* cmd) : cmd_(cmd) {}
    ~PendingCommands() {
        if (cmd_ == nullptr) {
            return;
        }
        if (submit_on_exit_) {
            SDL_SubmitGPUCommandBuffer(cmd_);
        } else {
            SDL_CancelGPUCommandBuffer(cmd_);
        }
    }
    PendingCommands(const PendingCommands&) = delete;
    PendingCommands& operator=(const PendingCommands&) = delete;
    PendingCommands(PendingCommands&&) = delete;
    PendingCommands& operator=(PendingCommands&&) = delete;

    explicit operator bool() const { return cmd_ != nullptr; }
    [[nodiscard]] SDL_GPUCommandBuffer* get() const { return cmd_; }
    void submit_on_exit() { submit_on_exit_ = true; }
    bool submit() { return SDL_SubmitGPUCommandBuffer(std::exchange(cmd_, nullptr)); }
    SDL_GPUFence* submit_and_acquire_fence() {
        return SDL_SubmitGPUCommandBufferAndAcquireFence(std::exchange(cmd_, nullptr));
    }

private:
    SDL_GPUCommandBuffer* cmd_;
    bool submit_on_exit_ = false;
};

} // namespace

struct Renderer::Impl {
    SDL_GPUDevice* device = nullptr;
    SDL_Window* window = nullptr;
    SDL_GPUShader* vertex_shader = nullptr;
    SDL_GPUShader* fragment_shader = nullptr;
    SDL_GPUBuffer* corner_buffer = nullptr;
    SDL_GPUTexture* atlas = nullptr;
    SDL_GPUSampler* sampler = nullptr;
    SDL_GPUBuffer* instance_buffer = nullptr;
    SDL_GPUTransferBuffer* instance_transfer = nullptr;
    u32 instance_capacity = 0; // in instances
    // One pipeline per colour-target format (swapchain format vs offscreen RGBA8).
    std::array<std::pair<SDL_GPUTextureFormat, SDL_GPUGraphicsPipeline*>, 2> pipelines{};

    struct ArmedFault {
        GpuFault fault = GpuFault::wait;
        u32 skip = 0;
    };
    std::optional<ArmedFault> armed_fault; // test seam, see Renderer::inject_fault

    Impl() = default;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    ~Impl() {
        if (device == nullptr) {
            return;
        }
        SDL_WaitForGPUIdle(device);
        for (auto& [format, pipeline] : pipelines) {
            if (pipeline != nullptr) {
                SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
            }
        }
        if (instance_transfer != nullptr)
            SDL_ReleaseGPUTransferBuffer(device, instance_transfer);
        if (instance_buffer != nullptr)
            SDL_ReleaseGPUBuffer(device, instance_buffer);
        if (corner_buffer != nullptr)
            SDL_ReleaseGPUBuffer(device, corner_buffer);
        if (sampler != nullptr)
            SDL_ReleaseGPUSampler(device, sampler);
        if (atlas != nullptr)
            SDL_ReleaseGPUTexture(device, atlas);
        if (fragment_shader != nullptr)
            SDL_ReleaseGPUShader(device, fragment_shader);
        if (vertex_shader != nullptr)
            SDL_ReleaseGPUShader(device, vertex_shader);
        if (window != nullptr)
            SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyGPUDevice(device);
    }

    // SDL_GPU calls that can fail, routed through the fault-injection seam.

    bool injected(GpuFault fault) {
        if (!armed_fault || armed_fault->fault != fault) {
            return false;
        }
        if (armed_fault->skip > 0) {
            --armed_fault->skip;
            return false;
        }
        armed_fault.reset();
        SDL_SetError("injected failure");
        return true;
    }

    SDL_GPUTexture* create_texture(const SDL_GPUTextureCreateInfo& info) {
        return injected(GpuFault::create_texture) ? nullptr : SDL_CreateGPUTexture(device, &info);
    }

    SDL_GPUBuffer* create_buffer(SDL_GPUBufferUsageFlags usage, u32 bytes) {
        SDL_GPUBufferCreateInfo info{};
        info.usage = usage;
        info.size = bytes;
        return injected(GpuFault::create_buffer) ? nullptr : SDL_CreateGPUBuffer(device, &info);
    }

    SDL_GPUTransferBuffer* create_transfer_buffer(SDL_GPUTransferBufferUsage usage, u32 bytes) {
        SDL_GPUTransferBufferCreateInfo info{};
        info.usage = usage;
        info.size = bytes;
        return injected(GpuFault::create_transfer_buffer) ? nullptr : SDL_CreateGPUTransferBuffer(device, &info);
    }

    void* map_transfer_buffer(SDL_GPUTransferBuffer* transfer, bool cycle) {
        return injected(GpuFault::map_transfer_buffer) ? nullptr : SDL_MapGPUTransferBuffer(device, transfer, cycle);
    }

    SDL_GPUCommandBuffer* acquire_command_buffer() {
        return injected(GpuFault::acquire_command_buffer) ? nullptr : SDL_AcquireGPUCommandBuffer(device);
    }

    SDL_GPUCopyPass* begin_copy_pass(SDL_GPUCommandBuffer* cmd) {
        return injected(GpuFault::begin_copy_pass) ? nullptr : SDL_BeginGPUCopyPass(cmd);
    }

    SDL_GPURenderPass* begin_render_pass(SDL_GPUCommandBuffer* cmd, const SDL_GPUColorTargetInfo& color) {
        return injected(GpuFault::begin_render_pass) ? nullptr : SDL_BeginGPURenderPass(cmd, &color, 1, nullptr);
    }

    bool wait_for_fence(SDL_GPUFence* fence) {
        return !injected(GpuFault::wait) && SDL_WaitForGPUFences(device, true, &fence, 1);
    }

    bool wait_for_idle() { return !injected(GpuFault::wait) && SDL_WaitForGPUIdle(device); }

    /// Copies `bytes` from `data` into a new upload transfer buffer.
    Result<SDL_GPUTransferBuffer*> staged_upload(const void* data, u32 bytes, const char* what) {
        Owned transfer(device, create_transfer_buffer(SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, bytes));
        if (!transfer) {
            return make_error(sdl_error(std::string(what) + ": SDL_CreateGPUTransferBuffer"));
        }
        void* mapped = map_transfer_buffer(transfer.get(), false);
        if (mapped == nullptr) {
            return make_error(sdl_error(std::string(what) + ": SDL_MapGPUTransferBuffer"));
        }
        std::memcpy(mapped, data, bytes);
        SDL_UnmapGPUTransferBuffer(device, transfer.get());
        return transfer.take();
    }

    Result<bool> create_shaders() {
        const SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);
        SDL_GPUShaderCreateInfo vs{};
        SDL_GPUShaderCreateInfo fs{};
        vs.stage = SDL_GPU_SHADERSTAGE_VERTEX;
        vs.num_uniform_buffers = 1;
        fs.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        fs.num_samplers = 1;
        if ((formats & SDL_GPU_SHADERFORMAT_MSL) != 0) {
            vs.format = fs.format = SDL_GPU_SHADERFORMAT_MSL;
            vs.code = fs.code = gta20_quad_msl;
            vs.code_size = fs.code_size = gta20_quad_msl_size;
            vs.entrypoint = "vs_main";
            fs.entrypoint = "fs_main";
#if defined(GTA20_D3D_SHADER_FORMAT_DXIL) || defined(GTA20_D3D_SHADER_FORMAT_DXBC)
        } else if ((formats & kD3DFormat) != 0) {
            vs.format = fs.format = kD3DFormat;
            vs.code = gta20_quad_vs_d3d;
            vs.code_size = gta20_quad_vs_d3d_size;
            fs.code = gta20_quad_ps_d3d;
            fs.code_size = gta20_quad_ps_d3d_size;
            vs.entrypoint = "VSMain";
            fs.entrypoint = "PSMain";
#endif
        } else if ((formats & SDL_GPU_SHADERFORMAT_SPIRV) != 0) {
            vs.format = fs.format = SDL_GPU_SHADERFORMAT_SPIRV;
            vs.code = gta20_quad_vert_spv;
            vs.code_size = gta20_quad_vert_spv_size;
            fs.code = gta20_quad_frag_spv;
            fs.code_size = gta20_quad_frag_spv_size;
            vs.entrypoint = fs.entrypoint = "main";
        } else {
            return make_error("GPU backend accepts none of the embedded shader formats");
        }
        vertex_shader = SDL_CreateGPUShader(device, &vs);
        fragment_shader = SDL_CreateGPUShader(device, &fs);
        if (vertex_shader == nullptr || fragment_shader == nullptr) {
            return make_error(sdl_error("SDL_CreateGPUShader"));
        }
        return true;
    }

    SDL_GPUGraphicsPipeline* pipeline_for(SDL_GPUTextureFormat format) {
        for (auto& [f, p] : pipelines) {
            if (p != nullptr && f == format) {
                return p;
            }
        }
        const std::array<SDL_GPUVertexBufferDescription, 2> buffers = {{
            {0, sizeof(float) * 2, SDL_GPU_VERTEXINPUTRATE_VERTEX, 0},
            {1, sizeof(scene::QuadInstance), SDL_GPU_VERTEXINPUTRATE_INSTANCE, 0},
        }};
        const std::array<SDL_GPUVertexAttribute, 6> attributes = {{
            {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 0},
            {1, 1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(scene::QuadInstance, center_x)},
            {2, 1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(scene::QuadInstance, width)},
            {3, 1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT, offsetof(scene::QuadInstance, rotation)},
            {4, 1, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, offsetof(scene::QuadInstance, r)},
            {5, 1, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(scene::QuadInstance, uv)},
        }};
        SDL_GPUColorTargetDescription target{};
        target.format = format;
        target.blend_state.enable_blend = true;
        target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        target.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        target.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

        SDL_GPUGraphicsPipelineCreateInfo info{};
        info.vertex_shader = vertex_shader;
        info.fragment_shader = fragment_shader;
        info.vertex_input_state.vertex_buffer_descriptions = buffers.data();
        info.vertex_input_state.num_vertex_buffers = static_cast<Uint32>(buffers.size());
        info.vertex_input_state.vertex_attributes = attributes.data();
        info.vertex_input_state.num_vertex_attributes = static_cast<Uint32>(attributes.size());
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        info.target_info.color_target_descriptions = &target;
        info.target_info.num_color_targets = 1;

        SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
        if (pipeline == nullptr) {
            return nullptr;
        }
        for (auto& slot : pipelines) {
            if (slot.second == nullptr) {
                slot = {format, pipeline};
                return pipeline;
            }
        }
        SDL_ReleaseGPUGraphicsPipeline(device, pipelines[1].second); // evict
        pipelines[1] = {format, pipeline};
        return pipeline;
    }

    /// Replaces the atlas only after the new texture has been fully uploaded; on any failure the
    /// previous atlas stays bound and everything created here is released.
    Result<bool> upload_atlas(u32 width, u32 height, std::span<const u8> rgba) {
        const auto bytes = rgba8_byte_size(width, height, "atlas");
        if (!bytes) {
            return bytes.error();
        }
        if (rgba.size() != bytes.value()) {
            return make_error("atlas: " + std::to_string(rgba.size()) + " bytes of pixel data for " +
                              std::to_string(width) + "x" + std::to_string(height) + " RGBA8 (expected " +
                              std::to_string(bytes.value()) + ")");
        }
        SDL_GPUTextureCreateInfo info{};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        info.width = width;
        info.height = height;
        info.layer_count_or_depth = 1;
        info.num_levels = 1;
        Owned texture(device, create_texture(info));
        if (!texture) {
            return make_error(sdl_error("atlas: SDL_CreateGPUTexture"));
        }
        auto staged = staged_upload(rgba.data(), bytes.value(), "atlas");
        if (!staged) {
            return staged.error();
        }
        Owned transfer(device, staged.value());
        PendingCommands cmd(acquire_command_buffer());
        if (!cmd) {
            return make_error(sdl_error("atlas: SDL_AcquireGPUCommandBuffer"));
        }
        SDL_GPUCopyPass* copy = begin_copy_pass(cmd.get());
        if (copy == nullptr) {
            return make_error(sdl_error("atlas: SDL_BeginGPUCopyPass"));
        }
        SDL_GPUTextureTransferInfo src{};
        src.transfer_buffer = transfer.get();
        SDL_GPUTextureRegion dst{};
        dst.texture = texture.get();
        dst.w = width;
        dst.h = height;
        dst.d = 1;
        SDL_UploadToGPUTexture(copy, &src, &dst, false);
        SDL_EndGPUCopyPass(copy);
        if (!cmd.submit()) {
            return make_error(sdl_error("atlas: SDL_SubmitGPUCommandBuffer"));
        }
        if (atlas != nullptr) {
            if (!wait_for_idle()) { // the previous atlas may still be in use
                return make_error(sdl_error("atlas: SDL_WaitForGPUIdle"));
            }
            SDL_ReleaseGPUTexture(device, atlas);
        }
        atlas = texture.take();
        return true;
    }

    Result<bool> create_sampler() {
        SDL_GPUSamplerCreateInfo info{};
        info.min_filter = SDL_GPU_FILTER_NEAREST; // pixel-art tiles: no blurring between texels
        info.mag_filter = SDL_GPU_FILTER_NEAREST;
        info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sampler = SDL_CreateGPUSampler(device, &info);
        if (sampler == nullptr) {
            return make_error(sdl_error("SDL_CreateGPUSampler"));
        }
        constexpr std::array<u8, 4> white = {255, 255, 255, 255};
        return upload_atlas(1, 1, white);
    }

    Result<bool> upload_corners() {
        constexpr u32 bytes = sizeof(kQuadCorners);
        corner_buffer = create_buffer(SDL_GPU_BUFFERUSAGE_VERTEX, bytes);
        if (corner_buffer == nullptr) {
            return make_error(sdl_error("quad corners: SDL_CreateGPUBuffer"));
        }
        auto staged = staged_upload(kQuadCorners.data(), bytes, "quad corners");
        if (!staged) {
            return staged.error();
        }
        Owned transfer(device, staged.value());
        PendingCommands cmd(acquire_command_buffer());
        if (!cmd) {
            return make_error(sdl_error("quad corners: SDL_AcquireGPUCommandBuffer"));
        }
        SDL_GPUCopyPass* copy = begin_copy_pass(cmd.get());
        if (copy == nullptr) {
            return make_error(sdl_error("quad corners: SDL_BeginGPUCopyPass"));
        }
        const SDL_GPUTransferBufferLocation src{transfer.get(), 0};
        const SDL_GPUBufferRegion dst{corner_buffer, 0, bytes};
        SDL_UploadToGPUBuffer(copy, &src, &dst, false);
        SDL_EndGPUCopyPass(copy);
        if (!cmd.submit()) {
            return make_error(sdl_error("quad corners: SDL_SubmitGPUCommandBuffer"));
        }
        return true;
    }

    /// Replaces the instance buffers with larger ones. On failure the previous buffers and
    /// capacity stay, so later frames that fit them still render.
    Result<bool> grow_instances(std::size_t count) {
        const auto capacity = grow_instance_capacity(instance_capacity, count);
        if (!capacity) {
            return capacity.error();
        }
        const auto bytes = instance_byte_size(capacity.value());
        if (!bytes) {
            return bytes.error();
        }
        Owned buffer(device, create_buffer(SDL_GPU_BUFFERUSAGE_VERTEX, bytes.value()));
        if (!buffer) {
            return make_error(sdl_error("growing the instance buffer: SDL_CreateGPUBuffer"));
        }
        Owned transfer(device, create_transfer_buffer(SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, bytes.value()));
        if (!transfer) {
            return make_error(sdl_error("growing the instance buffer: SDL_CreateGPUTransferBuffer"));
        }
        if (instance_transfer != nullptr)
            SDL_ReleaseGPUTransferBuffer(device, instance_transfer);
        if (instance_buffer != nullptr)
            SDL_ReleaseGPUBuffer(device, instance_buffer);
        instance_buffer = buffer.take();
        instance_transfer = transfer.take();
        instance_capacity = capacity.value();
        return true;
    }

    /// Grows (x2) the instance buffers when needed and records the upload.
    Result<bool> upload_instances(SDL_GPUCommandBuffer* cmd, const std::vector<scene::QuadInstance>& quads) {
        if (quads.empty()) {
            return true;
        }
        const auto bytes = instance_byte_size(quads.size());
        if (!bytes) {
            return bytes.error();
        }
        if (quads.size() > instance_capacity) {
            if (auto grown = grow_instances(quads.size()); !grown) {
                return grown.error();
            }
        }
        void* mapped = map_transfer_buffer(instance_transfer, true); // cycle: never stall on the GPU
        if (mapped == nullptr) {
            return make_error(sdl_error("instance upload: SDL_MapGPUTransferBuffer"));
        }
        std::memcpy(mapped, quads.data(), bytes.value());
        SDL_UnmapGPUTransferBuffer(device, instance_transfer);
        SDL_GPUCopyPass* copy = begin_copy_pass(cmd);
        if (copy == nullptr) {
            return make_error(sdl_error("instance upload: SDL_BeginGPUCopyPass"));
        }
        const SDL_GPUTransferBufferLocation src{instance_transfer, 0};
        const SDL_GPUBufferRegion dst{instance_buffer, 0, bytes.value()};
        SDL_UploadToGPUBuffer(copy, &src, &dst, true);
        SDL_EndGPUCopyPass(copy);
        return true;
    }

    /// Records the instance upload and the draw. On failure no pass is left open, so the caller
    /// can cancel (or, after a swapchain acquisition, submit) the command buffer.
    Result<bool> encode(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* target, SDL_GPUTextureFormat format, Vec2f size,
                        const scene::DrawList& list, const Camera2D& camera, ClearColor clear) {
        if (auto uploaded = upload_instances(cmd, list.quads()); !uploaded) {
            return uploaded.error();
        }
        SDL_GPUGraphicsPipeline* pipeline = pipeline_for(format);
        if (pipeline == nullptr) {
            return make_error(sdl_error("SDL_CreateGPUGraphicsPipeline"));
        }
        SDL_GPUColorTargetInfo color{};
        color.texture = target;
        color.clear_color = {clear.r, clear.g, clear.b, clear.a};
        color.load_op = SDL_GPU_LOADOP_CLEAR;
        color.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass* pass = begin_render_pass(cmd, color);
        if (pass == nullptr) {
            return make_error(sdl_error("SDL_BeginGPURenderPass"));
        }
        if (!list.quads().empty()) {
            SDL_BindGPUGraphicsPipeline(pass, pipeline);
            const std::array<SDL_GPUBufferBinding, 2> bindings = {{{corner_buffer, 0}, {instance_buffer, 0}}};
            SDL_BindGPUVertexBuffers(pass, 0, bindings.data(), static_cast<Uint32>(bindings.size()));
            const auto matrix = camera.view_projection(size);
            SDL_PushGPUVertexUniformData(cmd, 0, matrix.data(), static_cast<Uint32>(sizeof(float) * matrix.size()));
            const SDL_GPUTextureSamplerBinding binding{atlas, sampler};
            SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
            // upload_instances checked the count against kMaxInstances.
            SDL_DrawGPUPrimitives(pass, 6, static_cast<Uint32>(list.quads().size()), 0, 0);
        }
        SDL_EndGPURenderPass(pass);
        return true;
    }
};

Renderer::Renderer(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {
}
Renderer::~Renderer() = default;

Result<std::unique_ptr<Renderer>> Renderer::create(SDL_Window* window, const RendererOptions& options) {
    auto impl = std::make_unique<Impl>();
    const SDL_GPUShaderFormat formats = SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL | kD3DFormat;
    const char* driver = options.preferred_driver != nullptr ? options.preferred_driver : default_driver();
    impl->device = SDL_CreateGPUDevice(formats, options.gpu_debug, driver);
    if (impl->device == nullptr && options.preferred_driver == nullptr) {
        impl->device = SDL_CreateGPUDevice(formats, options.gpu_debug, nullptr); // any backend SDL can find
    }
    if (impl->device == nullptr) {
        return make_error(sdl_error("SDL_CreateGPUDevice"));
    }
    if (window != nullptr) {
        if (!SDL_ClaimWindowForGPUDevice(impl->device, window)) {
            return make_error(sdl_error("SDL_ClaimWindowForGPUDevice"));
        }
        impl->window = window;
    }
    if (auto shaders = impl->create_shaders(); !shaders) {
        return shaders.error();
    }
    if (auto corners = impl->upload_corners(); !corners) {
        return corners.error();
    }
    if (auto sampler = impl->create_sampler(); !sampler) {
        return sampler.error();
    }
    return std::unique_ptr<Renderer>(new Renderer(std::move(impl)));
}

Result<bool> Renderer::set_atlas(u32 width, u32 height, std::span<const u8> rgba) {
    return impl_->upload_atlas(width, height, rgba);
}

void Renderer::inject_fault(GpuFault fault, u32 skip) {
    impl_->armed_fault = Impl::ArmedFault{fault, skip};
}

std::string Renderer::backend() const {
    const char* name = SDL_GetGPUDeviceDriver(impl_->device);
    return name != nullptr ? name : "unknown";
}

Result<bool> Renderer::render_to_window(const scene::DrawList& list, const Camera2D& camera, ClearColor clear) {
    if (impl_->window == nullptr) {
        return make_error("renderer was created without a window");
    }
    PendingCommands cmd(impl_->acquire_command_buffer());
    if (!cmd) {
        return make_error(sdl_error("window frame: SDL_AcquireGPUCommandBuffer"));
    }
    SDL_GPUTexture* swapchain = nullptr;
    Uint32 w = 0;
    Uint32 h = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd.get(), impl_->window, &swapchain, &w, &h)) {
        return make_error(sdl_error("window frame: SDL_WaitAndAcquireGPUSwapchainTexture"));
    }
    cmd.submit_on_exit(); // a command buffer that acquired a swapchain texture must not be cancelled
    if (swapchain == nullptr) {
        return false; // minimised / occluded
    }
    const auto format = SDL_GetGPUSwapchainTextureFormat(impl_->device, impl_->window);
    if (auto ok = impl_->encode(cmd.get(), swapchain, format, {static_cast<float>(w), static_cast<float>(h)}, list,
                                camera, clear);
        !ok) {
        return make_error("window frame: " + ok.error().message);
    }
    if (!cmd.submit()) {
        return make_error(sdl_error("window frame: SDL_SubmitGPUCommandBuffer"));
    }
    return true;
}

Result<Image> Renderer::render_offscreen(const scene::DrawList& list, const Camera2D& camera, u32 width, u32 height,
                                         ClearColor clear) {
    const auto bytes = rgba8_byte_size(width, height, "offscreen frame");
    if (!bytes) {
        return bytes.error();
    }
    SDL_GPUDevice* device = impl_->device;
    constexpr SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    SDL_GPUTextureCreateInfo tex_info{};
    tex_info.type = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format = format;
    tex_info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    tex_info.width = width;
    tex_info.height = height;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels = 1;
    Owned texture(device, impl_->create_texture(tex_info));
    if (!texture) {
        return make_error(sdl_error("offscreen frame: SDL_CreateGPUTexture"));
    }
    Owned download(device, impl_->create_transfer_buffer(SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, bytes.value()));
    if (!download) {
        return make_error(sdl_error("offscreen frame: SDL_CreateGPUTransferBuffer"));
    }

    PendingCommands cmd(impl_->acquire_command_buffer());
    if (!cmd) {
        return make_error(sdl_error("offscreen frame: SDL_AcquireGPUCommandBuffer"));
    }
    if (auto ok = impl_->encode(cmd.get(), texture.get(), format,
                                {static_cast<float>(width), static_cast<float>(height)}, list, camera, clear);
        !ok) {
        return make_error("offscreen frame: " + ok.error().message);
    }
    SDL_GPUCopyPass* copy = impl_->begin_copy_pass(cmd.get());
    if (copy == nullptr) {
        return make_error(sdl_error("offscreen readback: SDL_BeginGPUCopyPass"));
    }
    SDL_GPUTextureRegion region{};
    region.texture = texture.get();
    region.w = width;
    region.h = height;
    region.d = 1;
    SDL_GPUTextureTransferInfo destination{};
    destination.transfer_buffer = download.get();
    SDL_DownloadFromGPUTexture(copy, &region, &destination);
    SDL_EndGPUCopyPass(copy);
    SDL_GPUFence* fence = cmd.submit_and_acquire_fence();
    if (fence == nullptr) {
        return make_error(sdl_error("offscreen frame: SDL_SubmitGPUCommandBufferAndAcquireFence"));
    }
    const bool finished = impl_->wait_for_fence(fence);
    SDL_ReleaseGPUFence(device, fence);
    if (!finished) {
        return make_error(sdl_error("offscreen readback: SDL_WaitForGPUFences"));
    }

    const void* pixels = impl_->map_transfer_buffer(download.get(), false);
    if (pixels == nullptr) {
        return make_error(sdl_error("offscreen readback: SDL_MapGPUTransferBuffer"));
    }
    Image image;
    image.width = width;
    image.height = height;
    image.rgba.resize(bytes.value());
    std::memcpy(image.rgba.data(), pixels, image.rgba.size());
    SDL_UnmapGPUTransferBuffer(device, download.get());
    return image;
}

} // namespace gta20::render
