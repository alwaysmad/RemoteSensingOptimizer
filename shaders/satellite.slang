#include "shared.slang"

[[vk::binding(0, 0)]]
ConstantBuffer<UBO> g_ubo;

struct VertexOutput
{
    float4 pos : SV_Position;
    float4 color : COLOR;
};

// 9 points: 1 tip + 8 corners
static const float4 kNdcCorners[9] = {
    float4(0, 0, -1, 0),
    float4(-1, -1, 0, 1),
    float4(1, -1, 0, 1),
    float4(1, 1, 0, 1),
    float4(-1, 1, 0, 1),
    float4(-1, -1, 1, 1),
    float4(1, -1, 1, 1),
    float4(1, 1, 1, 1),
    float4(-1, 1, 1, 1)
};

// 16 line segments -> 32 vertices
static const uint kIndices[32] = {
    0, 1, 0, 2, 0, 3, 0, 4,
    1, 2, 2, 3, 3, 4, 4, 1,
    1, 5, 2, 6, 3, 7, 4, 8,
    5, 6, 6, 7, 7, 8, 8, 5
};

[shader("vertex")]
VertexOutput vertMain(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    VertexOutput out;

    if (instanceID >= g_ubo.agentCount)
    {
        out.pos = float4(2.0, 2.0, 2.0, 1.0);
        out.color = float4(0.0, 0.0, 0.0, 0.0);
        return out;
    }

    float4x4 invViewProj = getInvViewProj(g_ubo.agents[instanceID].camera);

    uint idx = kIndices[vertexID];
    float4 ndcPos = kNdcCorners[idx];
    float4 worldPos = mul(invViewProj, ndcPos);

    out.pos = mul(g_ubo.proj, mul(g_ubo.view, worldPos));
    out.color = float4(g_ubo.agents[instanceID].data.x, g_ubo.agents[instanceID].data.y, g_ubo.agents[instanceID].data.z, g_ubo.agents[instanceID].data.w);
    return out;
}

[shader("fragment")]
float4 fragMain(VertexOutput inVert) : SV_Target
{
    return inVert.color;
}
