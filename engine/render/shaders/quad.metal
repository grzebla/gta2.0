// Metal version of quad.vert.glsl / quad.frag.glsl (compiled by the driver at run time).
#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float2 corner [[attribute(0)]];
    float2 center [[attribute(1)]];
    float2 size [[attribute(2)]];
    float rotation [[attribute(3)]];
    float4 color [[attribute(4)]];
};

struct VertexOut {
    float4 position [[position]];
    float4 color;
};

struct Camera {
    float4x4 view_projection;
};

vertex VertexOut vs_main(VertexIn in [[stage_in]], constant Camera& camera [[buffer(0)]]) {
    float2 c = in.corner * in.size;
    float s = sin(in.rotation);
    float k = cos(in.rotation);
    float2 world = in.center + float2(c.x * k - c.y * s, c.x * s + c.y * k);
    VertexOut out;
    out.position = camera.view_projection * float4(world, 0.0, 1.0);
    out.color = in.color;
    return out;
}

fragment float4 fs_main(VertexOut in [[stage_in]]) {
    return in.color;
}
