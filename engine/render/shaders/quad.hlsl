// Direct3D 12 version of quad.vert.glsl / quad.frag.glsl (compiled at build time on Windows).
// SDL_GPU binding rules: vertex uniforms in (b0, space1); inputs use TEXCOORD<location>.
cbuffer Camera : register(b0, space1)
{
    float4x4 view_projection; // column-major, same bytes as the GLSL/MSL versions
};

struct VSInput
{
    float2 corner : TEXCOORD0;
    float2 center : TEXCOORD1;
    float2 size : TEXCOORD2;
    float rotation : TEXCOORD3;
    float4 color : TEXCOORD4;
};

struct VSOutput
{
    float4 color : TEXCOORD0;
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
    return output;
}

float4 PSMain(VSOutput input) : SV_Target0
{
    return input.color;
}
