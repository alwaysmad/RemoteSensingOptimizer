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
    float pointSize : SV_PointSize;
};

[shader("vertex")]
VertexOutput vertMain(VertexInput inVert)
{
    VertexOutput output;
    // The Warlord's Cull: Direction from Center to Point in View Space
    // By setting w=0.0, we ignore translation. We just rotate the local coordinate.
    float viewDirectionZ = mul(g_ubo.view, mul(g_ubo.model, float4(inVert.coords.xyz, 0.0))).z;
    // In standard GLM lookAt, the camera looks down the -Z axis.
    // This means anything pointing away from the camera has a negative Z.
    // (If it culls the wrong half, just flip it to > 0.0 depending on your exact projection setup).
    if (viewDirectionZ < 0.0)
    {
        output.color = float3(0.0, 0.0, 0.0);
        output.sv_position = float4(-10.0, -10.0, -10.0, 1.0); // banish point off-screen
        output.pointSize = 0.0;
        return output;
    }

    output.sv_position = mul(g_ubo.proj, mul(g_ubo.view, mul(g_ubo.model, float4(inVert.coords.xyz, 1.0))));
    output.pointSize = 1.0;

    // Linear Coloring (alpha 0.0 = White, alpha 1.0 = Black)
    const float alpha = saturate(inVert.coords.w);
    output.color = float3(1.0 - alpha);

    return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput inVert) : SV_Target
{
    return float4(inVert.color, 1.0);
}