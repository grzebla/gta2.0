add_library(g2n_render
    src/renderer.cpp)
add_library(g2n::render ALIAS g2n_render)
target_include_directories(g2n_render PUBLIC include)
target_link_libraries(g2n_render PUBLIC g2n::core g2n::scene ${G2N_SDL_TARGET})
g2n_target_defaults(g2n_render)

# Shader binaries for every backend SDL_GPU may pick on this platform.
set(SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
g2n_embed_binary(g2n_render g2n_quad_vert_spv ${SHADERS}/spirv/quad.vert.spv)
g2n_embed_binary(g2n_render g2n_quad_frag_spv ${SHADERS}/spirv/quad.frag.spv)
g2n_embed_binary(g2n_render g2n_quad_msl ${SHADERS}/quad.metal)

# Direct3D 12: compile HLSL at build time with the Windows SDK compiler (dxc -> DXIL, else fxc -> DXBC).
if(WIN32)
    file(GLOB G2N_DXC_CANDIDATES "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/bin/*/x64/dxc.exe")
    file(GLOB G2N_FXC_CANDIDATES "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/bin/*/x64/fxc.exe")
    list(SORT G2N_DXC_CANDIDATES COMPARE NATURAL ORDER DESCENDING)
    list(SORT G2N_FXC_CANDIDATES COMPARE NATURAL ORDER DESCENDING)
    find_program(G2N_DXC NAMES dxc HINTS ${G2N_DXC_CANDIDATES})
    if(NOT G2N_DXC AND G2N_DXC_CANDIDATES)
        list(GET G2N_DXC_CANDIDATES 0 G2N_DXC)
    endif()
    if(NOT G2N_FXC AND G2N_FXC_CANDIDATES)
        list(GET G2N_FXC_CANDIDATES 0 G2N_FXC)
    endif()

    set(out ${CMAKE_CURRENT_BINARY_DIR}/shaders)
    file(MAKE_DIRECTORY ${out})
    if(G2N_DXC)
        message(STATUS "D3D12 shaders: DXIL via ${G2N_DXC}")
        add_custom_command(OUTPUT ${out}/quad.vs.d3d ${out}/quad.ps.d3d
            COMMAND ${G2N_DXC} -nologo -T vs_6_0 -E VSMain -Fo ${out}/quad.vs.d3d ${SHADERS}/quad.hlsl
            COMMAND ${G2N_DXC} -nologo -T ps_6_0 -E PSMain -Fo ${out}/quad.ps.d3d ${SHADERS}/quad.hlsl
            DEPENDS ${SHADERS}/quad.hlsl VERBATIM)
        target_compile_definitions(g2n_render PRIVATE G2N_D3D_SHADER_FORMAT_DXIL=1)
    elseif(G2N_FXC)
        message(STATUS "D3D12 shaders: DXBC (SM 5.1) via ${G2N_FXC}")
        add_custom_command(OUTPUT ${out}/quad.vs.d3d ${out}/quad.ps.d3d
            COMMAND ${G2N_FXC} /nologo /T vs_5_1 /E VSMain /Fo ${out}/quad.vs.d3d ${SHADERS}/quad.hlsl
            COMMAND ${G2N_FXC} /nologo /T ps_5_1 /E PSMain /Fo ${out}/quad.ps.d3d ${SHADERS}/quad.hlsl
            DEPENDS ${SHADERS}/quad.hlsl VERBATIM)
        target_compile_definitions(g2n_render PRIVATE G2N_D3D_SHADER_FORMAT_DXBC=1)
    else()
        message(WARNING "No dxc/fxc found: Direct3D 12 unavailable, SDL_GPU will try Vulkan.")
    endif()
    if(G2N_DXC OR G2N_FXC)
        g2n_embed_binary(g2n_render g2n_quad_vs_d3d ${out}/quad.vs.d3d)
        g2n_embed_binary(g2n_render g2n_quad_ps_d3d ${out}/quad.ps.d3d)
    endif()
endif()
