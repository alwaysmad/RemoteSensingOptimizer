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
    // 1. Calculate View Space Position
    const float4 worldPos = mul(g_ubo.model, float4(inVert.coords.xyz, 1.0));
    const float4 viewPos = mul(g_ubo.view, worldPos);

    // 2. Calculate Final Clip Space Position
    output.sv_position = mul(g_ubo.proj, viewPos);

    // ==========================================
    // 3. The True Perspective Horizon Cull
    // ==========================================
    // Rotate the local coordinate (which is also the normal) into View Space
    float3 viewNormal = mul(g_ubo.view, mul(g_ubo.model, float4(inVert.coords.xyz, 0.0))).xyz;

    // 'viewPos.xyz' is the vector FROM the camera TO the vertex (since camera is at 0,0,0).
    // If the dot product is > 0.0, the normal and the view ray are facing the same general direction,
    // meaning the vertex is physically pointing away from the camera.
    if (dot(viewNormal, viewPos.xyz) > 0.0)
    {
        output.color = float3(0.0, 0.0, 0.0);
        output.sv_position = float4(-10.0, -10.0, -10.0, 1.0);
        output.pointSize = 0.0;
        return output;
    }

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