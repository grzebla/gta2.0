#include "gta20/render/renderer.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <cstring>

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

std::string sdl_error(const char* what) {
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

    Result<bool> upload_atlas(u32 width, u32 height, std::span<const u8> rgba) {
        if (rgba.size() != static_cast<std::size_t>(width) * height * 4 || width == 0 || height == 0) {
            return make_error("atlas pixel data does not match its size");
        }
        SDL_GPUTextureCreateInfo info{};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        info.width = width;
        info.height = height;
        info.layer_count_or_depth = 1;
        info.num_levels = 1;
        SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &info);
        SDL_GPUTransferBufferCreateInfo transfer_info{};
        transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transfer_info.size = static_cast<Uint32>(rgba.size());
        SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
        if (texture == nullptr || transfer == nullptr) {
            if (texture != nullptr) {
                SDL_ReleaseGPUTexture(device, texture);
            }
            if (transfer != nullptr) {
                SDL_ReleaseGPUTransferBuffer(device, transfer);
            }
            return make_error(sdl_error("creating the atlas texture"));
        }
        std::memcpy(SDL_MapGPUTransferBuffer(device, transfer, false), rgba.data(), rgba.size());
        SDL_UnmapGPUTransferBuffer(device, transfer);
        SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
        SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
        SDL_GPUTextureTransferInfo src{};
        src.transfer_buffer = transfer;
        SDL_GPUTextureRegion dst{};
        dst.texture = texture;
        dst.w = width;
        dst.h = height;
        dst.d = 1;
        SDL_UploadToGPUTexture(copy, &src, &dst, false);
        SDL_EndGPUCopyPass(copy);
        const bool submitted = SDL_SubmitGPUCommandBuffer(cmd);
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        if (!submitted) {
            SDL_ReleaseGPUTexture(device, texture);
            return make_error(sdl_error("SDL_SubmitGPUCommandBuffer"));
        }
        if (atlas != nullptr) {
            SDL_WaitForGPUIdle(device); // the previous atlas may still be in use
            SDL_ReleaseGPUTexture(device, atlas);
        }
        atlas = texture;
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
        SDL_GPUBufferCreateInfo buffer_info{};
        buffer_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        buffer_info.size = sizeof(kQuadCorners);
        corner_buffer = SDL_CreateGPUBuffer(device, &buffer_info);
        SDL_GPUTransferBufferCreateInfo transfer_info{};
        transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transfer_info.size = sizeof(kQuadCorners);
        SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
        if (corner_buffer == nullptr || transfer == nullptr) {
            if (transfer != nullptr)
                SDL_ReleaseGPUTransferBuffer(device, transfer);
            return make_error(sdl_error("creating the quad vertex buffer"));
        }
        std::memcpy(SDL_MapGPUTransferBuffer(device, transfer, false), kQuadCorners.data(), sizeof(kQuadCorners));
        SDL_UnmapGPUTransferBuffer(device, transfer);
        SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
        SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
        const SDL_GPUTransferBufferLocation src{transfer, 0};
        const SDL_GPUBufferRegion dst{corner_buffer, 0, sizeof(kQuadCorners)};
        SDL_UploadToGPUBuffer(copy, &src, &dst, false);
        SDL_EndGPUCopyPass(copy);
        const bool submitted = SDL_SubmitGPUCommandBuffer(cmd);
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        return submitted ? Result<bool>(true) : Result<bool>(make_error(sdl_error("SDL_SubmitGPUCommandBuffer")));
    }

    /// Grows (x2) the instance buffers when needed and records the upload.
    Result<bool> upload_instances(SDL_GPUCommandBuffer* cmd, const std::vector<scene::QuadInstance>& quads) {
        if (quads.empty()) {
            return true;
        }
        const auto count = static_cast<u32>(quads.size());
        if (count > instance_capacity) {
            u32 capacity = instance_capacity == 0 ? 1024 : instance_capacity;
            while (capacity < count)
                capacity *= 2;
            if (instance_transfer != nullptr)
                SDL_ReleaseGPUTransferBuffer(device, instance_transfer);
            if (instance_buffer != nullptr)
                SDL_ReleaseGPUBuffer(device, instance_buffer);
            const auto bytes = static_cast<Uint32>(capacity * sizeof(scene::QuadInstance));
            SDL_GPUBufferCreateInfo buffer_info{};
            buffer_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
            buffer_info.size = bytes;
            instance_buffer = SDL_CreateGPUBuffer(device, &buffer_info);
            SDL_GPUTransferBufferCreateInfo transfer_info{};
            transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            transfer_info.size = bytes;
            instance_transfer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
            if (instance_buffer == nullptr || instance_transfer == nullptr) {
                instance_capacity = 0;
                return make_error(sdl_error("growing the instance buffer"));
            }
            instance_capacity = capacity;
        }
        const auto bytes = static_cast<Uint32>(count * sizeof(scene::QuadInstance));
        void* mapped = SDL_MapGPUTransferBuffer(device, instance_transfer, true); // cycle: never stall on the GPU
        std::memcpy(mapped, quads.data(), bytes);
        SDL_UnmapGPUTransferBuffer(device, instance_transfer);
        SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
        const SDL_GPUTransferBufferLocation src{instance_transfer, 0};
        const SDL_GPUBufferRegion dst{instance_buffer, 0, bytes};
        SDL_UploadToGPUBuffer(copy, &src, &dst, true);
        SDL_EndGPUCopyPass(copy);
        return true;
    }

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
        SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmd, &color, 1, nullptr);
        if (!list.quads().empty()) {
            SDL_BindGPUGraphicsPipeline(pass, pipeline);
            const std::array<SDL_GPUBufferBinding, 2> bindings = {{{corner_buffer, 0}, {instance_buffer, 0}}};
            SDL_BindGPUVertexBuffers(pass, 0, bindings.data(), static_cast<Uint32>(bindings.size()));
            const auto matrix = camera.view_projection(size);
            SDL_PushGPUVertexUniformData(cmd, 0, matrix.data(), static_cast<Uint32>(sizeof(float) * matrix.size()));
            const SDL_GPUTextureSamplerBinding binding{atlas, sampler};
            SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
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

std::string Renderer::backend() const {
    const char* name = SDL_GetGPUDeviceDriver(impl_->device);
    return name != nullptr ? name : "unknown";
}

Result<bool> Renderer::render_to_window(const scene::DrawList& list, const Camera2D& camera, ClearColor clear) {
    if (impl_->window == nullptr) {
        return make_error("renderer was created without a window");
    }
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(impl_->device);
    if (cmd == nullptr) {
        return make_error(sdl_error("SDL_AcquireGPUCommandBuffer"));
    }
    SDL_GPUTexture* swapchain = nullptr;
    Uint32 w = 0;
    Uint32 h = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, impl_->window, &swapchain, &w, &h)) {
        SDL_CancelGPUCommandBuffer(cmd);
        return make_error(sdl_error("SDL_WaitAndAcquireGPUSwapchainTexture"));
    }
    if (swapchain == nullptr) {
        SDL_SubmitGPUCommandBuffer(cmd);
        return false; // minimised / occluded
    }
    const auto format = SDL_GetGPUSwapchainTextureFormat(impl_->device, impl_->window);
    if (auto ok =
            impl_->encode(cmd, swapchain, format, {static_cast<float>(w), static_cast<float>(h)}, list, camera, clear);
        !ok) {
        SDL_SubmitGPUCommandBuffer(cmd);
        return ok.error();
    }
    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        return make_error(sdl_error("SDL_SubmitGPUCommandBuffer"));
    }
    return true;
}

Result<Image> Renderer::render_offscreen(const scene::DrawList& list, const Camera2D& camera, u32 width, u32 height,
                                         ClearColor clear) {
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
    SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &tex_info);
    SDL_GPUTransferBufferCreateInfo download_info{};
    download_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    download_info.size = width * height * 4;
    SDL_GPUTransferBuffer* download = SDL_CreateGPUTransferBuffer(device, &download_info);
    struct Cleanup {
        SDL_GPUDevice* device;
        SDL_GPUTexture* texture;
        SDL_GPUTransferBuffer* download;
        Cleanup(SDL_GPUDevice* d, SDL_GPUTexture* t, SDL_GPUTransferBuffer* b) : device(d), texture(t), download(b) {}
        Cleanup(const Cleanup&) = delete;
        Cleanup& operator=(const Cleanup&) = delete;
        Cleanup(Cleanup&&) = delete;
        Cleanup& operator=(Cleanup&&) = delete;
        ~Cleanup() {
            if (download != nullptr)
                SDL_ReleaseGPUTransferBuffer(device, download);
            if (texture != nullptr)
                SDL_ReleaseGPUTexture(device, texture);
        }
    } cleanup{device, texture, download};
    if (texture == nullptr || download == nullptr) {
        return make_error(sdl_error("creating the offscreen target"));
    }

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
    if (auto ok = impl_->encode(cmd, texture, format, {static_cast<float>(width), static_cast<float>(height)}, list,
                                camera, clear);
        !ok) {
        SDL_SubmitGPUCommandBuffer(cmd);
        return ok.error();
    }
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureRegion region{};
    region.texture = texture;
    region.w = width;
    region.h = height;
    region.d = 1;
    SDL_GPUTextureTransferInfo destination{};
    destination.transfer_buffer = download;
    SDL_DownloadFromGPUTexture(copy, &region, &destination);
    SDL_EndGPUCopyPass(copy);
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (fence == nullptr) {
        return make_error(sdl_error("SDL_SubmitGPUCommandBufferAndAcquireFence"));
    }
    SDL_WaitForGPUFences(device, true, &fence, 1);
    SDL_ReleaseGPUFence(device, fence);

    Image image;
    image.width = width;
    image.height = height;
    image.rgba.resize(static_cast<std::size_t>(width) * height * 4);
    const void* pixels = SDL_MapGPUTransferBuffer(device, download, false);
    std::memcpy(image.rgba.data(), pixels, image.rgba.size());
    SDL_UnmapGPUTransferBuffer(device, download);
    return image;
}

} // namespace gta20::render
