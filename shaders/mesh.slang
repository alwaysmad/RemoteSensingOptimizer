#include "shared.slang"

struct VertexInput
{
    [[vk::location(0)]]
    float4 coords;
};

[[vk::binding(0, 0)]]
ConstantBuffer<UBO> g_ubo;

struct VertexOutput
{
    float3 color;
    float4 sv_position : SV_Position;
};

[shader("vertex")]
VertexOutput vertMain(VertexInput inVert)
{
    VertexOutput output;
    const float4 worldPos = mul(g_ubo.model, float4(inVert.coords.xyz, 1.0));
    output.sv_position = mul(g_ubo.viewProj, worldPos);

    const float gray = saturate(inVert.coords.w);
    output.color = float3(gray, gray, gray);
    return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput inVert) : SV_Target
{
    return float4(inVert.color, 1.0);
}
