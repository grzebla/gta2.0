// Direct3D 12 version of quad.vert.glsl / quad.frag.glsl (compiled at build time on Windows).
// SDL_GPU binding rules: vertex uniforms (b0, space1); fragment textures/samplers (t0/s0, space2);
// vertex inputs use TEXCOORD<location>.
cbuffer Camera : register(b0, space1)
{
    float4x4 view_projection; // column-major, same bytes as the GLSL/MSL versions
};

Texture2D<float4> Atlas : register(t0, space2);
SamplerState AtlasSampler : register(s0, space2);

struct VSInput
{
    float2 corner : TEXCOORD0;
    float2 center : TEXCOORD1;
    float2 size : TEXCOORD2;
    float rotation : TEXCOORD3;
    float4 color : TEXCOORD4;
    float4 uv : TEXCOORD5;
};

struct VSOutput
{
    float4 color : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float4 position : SV_Position;
};

VSOutput VSMain(VSInput input)
{
    float2 c = input.corner * input.size;
    float s = sin(input.rotation);
    float k = cos(input.rotation);
    float2 world = input.center + float2(c.x * k - c.y * s, c.x * s + c.y * k);
    VSOutput output;
    output.position = mul(view_projection, float4(world, 0.0, 1.0));
    output.color = input.color;
    output.uv = lerp(input.uv.xy, input.uv.zw, input.corner + 0.5);
    return output;
}

float4 PSMain(VSOutput input) : SV_Target0
{
    return Atlas.Sample(AtlasSampler, input.uv) * input.color;
}
