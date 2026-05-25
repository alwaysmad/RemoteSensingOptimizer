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
VertexOutput vertMain(uint vertexID: SV_VertexID, uint instanceID: SV_InstanceID)
{
    VertexOutput out;

    if (instanceID >= g_ubo.agentCount)
    {
        // if outside array bounds
        // banish to shadow realm
        out.pos = float4(-10.0, -10.0, -10.0, 1.0);
        out.color = float4(0.0, 0.0, 0.0, 0.0);
        return out;
    }
    float4x4 satInvViewProj = getInvViewProj(g_ubo.agents[instanceID].camera);

    // ==========================================
    // 2.1 Find Earth (E) & Satellite (S) in Observer's View
    // ==========================================
    float4 satWorldPosTmp = mul(satInvViewProj, kNdcCorners[0]);
    float3 S = satWorldPosTmp.xyz / satWorldPosTmp.w;

    // Observer is at (0,0,0), so vector CE is exactly Earth's position in View Space
    float3 E = mul(g_ubo.view, float4(0.0, 0.0, 0.0, 1.0)).xyz;
    S = mul(g_ubo.view, float4(S, 1.0)).xyz;

    // ==========================================
    // 2.2 Calculate Triangle Properties
    // ==========================================
    // Squared distance from E to line segment CS using projection (avoids cross product and sqrt)
    float e_dot_s = dot(E, S);
    float s_sq = dot(S, S);
    float d2 = dot(E, E) - (e_dot_s * e_dot_s) / s_sq;

    // Vector from Earth to Satellite
    float3 ES = S - E;

    // ==========================================
    // 2.3 Cull if behind
    // ==========================================
    // If ray intersects Earth (d2 < 1.0) AND satellite is further away than Earth (dot(E, ES) > 0)
    if (d2 < 1.0 && dot(E, ES) > 0.0)
    {
        // banish to shadow realm
        out.pos = float4(-10.0, -10.0, -10.0, 1.0);
        out.color = float4(0.0, 0.0, 0.0, 0.0);
        return out;
    }

    // ==========================================
    // Render Visible Satellite
    // ==========================================
    uint idx = kIndices[vertexID];
    float4 ndcPos = kNdcCorners[idx];
    float4 worldPos = mul(satInvViewProj, ndcPos);

    out.pos = mul(g_ubo.proj, mul(g_ubo.view, worldPos));
    out.color = float4(g_ubo.agents[instanceID].data.xyzw);
    return out;
}

[shader("fragment")]
float4 fragMain(VertexOutput inVert) : SV_Target
{
    return inVert.color;
}