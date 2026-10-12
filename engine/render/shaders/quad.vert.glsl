#version 450
// Instanced, rotated, coloured quad. Vulkan source; compiled to SPIR-V by scripts/shaders/compile-spirv.sh.
layout(location = 0) in vec2 a_corner;      // per vertex: unit quad corner in [-0.5, 0.5]
layout(location = 1) in vec2 i_center;      // per instance (world units)
layout(location = 2) in vec2 i_size;
layout(location = 3) in float i_rotation;   // radians, clockwise on screen (world y points down)
layout(location = 4) in vec4 i_color;       // UBYTE4_NORM

layout(location = 0) out vec4 v_color;

layout(set = 1, binding = 0) uniform Camera {
    mat4 view_projection;
};

void main() {
    vec2 c = a_corner * i_size;
    float s = sin(i_rotation);
    float k = cos(i_rotation);
    vec2 world = i_center + vec2(c.x * k - c.y * s, c.x * s + c.y * k);
    gl_Position = view_projection * vec4(world, 0.0, 1.0);
    v_color = i_color;
}
