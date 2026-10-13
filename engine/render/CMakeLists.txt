add_library(gta20_render
    src/renderer.cpp
    src/transfer_size.cpp)
add_library(gta20::render ALIAS gta20_render)
target_include_directories(gta20_render PUBLIC include)
target_link_libraries(gta20_render PUBLIC gta20::core gta20::scene ${GTA20_SDL_TARGET})
gta20_target_defaults(gta20_render)

# Shader binaries for every backend SDL_GPU may pick on this platform.
set(SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
gta20_embed_binary(gta20_render gta20_quad_vert_spv ${SHADERS}/spirv/quad.vert.spv)
gta20_embed_binary(gta20_render gta20_quad_frag_spv ${SHADERS}/spirv/quad.frag.spv)
gta20_embed_binary(gta20_render gta20_quad_msl ${SHADERS}/quad.metal)

# Direct3D 12: compile HLSL at build time with the Windows SDK compiler (dxc -> DXIL, else fxc -> DXBC).
if(WIN32)
    file(GLOB GTA20_DXC_CANDIDATES "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/bin/*/x64/dxc.exe")
    file(GLOB GTA20_FXC_CANDIDATES "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/bin/*/x64/fxc.exe")
    list(SORT GTA20_DXC_CANDIDATES COMPARE NATURAL ORDER DESCENDING)
    list(SORT GTA20_FXC_CANDIDATES COMPARE NATURAL ORDER DESCENDING)
    find_program(GTA20_DXC NAMES dxc HINTS ${GTA20_DXC_CANDIDATES})
    if(NOT GTA20_DXC AND GTA20_DXC_CANDIDATES)
        list(GET GTA20_DXC_CANDIDATES 0 GTA20_DXC)
    endif()
    if(NOT GTA20_FXC AND GTA20_FXC_CANDIDATES)
        list(GET GTA20_FXC_CANDIDATES 0 GTA20_FXC)
    endif()

    set(out ${CMAKE_CURRENT_BINARY_DIR}/shaders)
    file(MAKE_DIRECTORY ${out})
    if(GTA20_DXC)
        message(STATUS "D3D12 shaders: DXIL via ${GTA20_DXC}")
        add_custom_command(OUTPUT ${out}/quad.vs.d3d ${out}/quad.ps.d3d
            COMMAND ${GTA20_DXC} -nologo -T vs_6_0 -E VSMain -Fo ${out}/quad.vs.d3d ${SHADERS}/quad.hlsl
            COMMAND ${GTA20_DXC} -nologo -T ps_6_0 -E PSMain -Fo ${out}/quad.ps.d3d ${SHADERS}/quad.hlsl
            DEPENDS ${SHADERS}/quad.hlsl VERBATIM)
        target_compile_definitions(gta20_render PRIVATE GTA20_D3D_SHADER_FORMAT_DXIL=1)
    elseif(GTA20_FXC)
        message(STATUS "D3D12 shaders: DXBC (SM 5.1) via ${GTA20_FXC}")
        add_custom_command(OUTPUT ${out}/quad.vs.d3d ${out}/quad.ps.d3d
            COMMAND ${GTA20_FXC} /nologo /T vs_5_1 /E VSMain /Fo ${out}/quad.vs.d3d ${SHADERS}/quad.hlsl
            COMMAND ${GTA20_FXC} /nologo /T ps_5_1 /E PSMain /Fo ${out}/quad.ps.d3d ${SHADERS}/quad.hlsl
            DEPENDS ${SHADERS}/quad.hlsl VERBATIM)
        target_compile_definitions(gta20_render PRIVATE GTA20_D3D_SHADER_FORMAT_DXBC=1)
    else()
        message(WARNING "No dxc/fxc found: Direct3D 12 unavailable, SDL_GPU will try Vulkan.")
    endif()
    if(GTA20_DXC OR GTA20_FXC)
        gta20_embed_binary(gta20_render gta20_quad_vs_d3d ${out}/quad.vs.d3d)
        gta20_embed_binary(gta20_render gta20_quad_ps_d3d ${out}/quad.ps.d3d)
    endif()
endif()
