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

    // 1. Out of bounds check
    if (instanceID >= g_ubo.agentCount)
    {
        // banish to shadow realm
        out.pos = float4(-10.0, -10.0, -10.0, 1.0);
        out.color = float4(0.0, 0.0, 0.0, 0.0);
        return out;
    }

    // 2. Extract satellite transform contract
    float4x4 satInvViewProj = getInvViewProj(g_ubo.agents[instanceID].camera);

    // 3. Extract the Satellite's World Space Position
    // The tip of the satellite camera frustum (Ndc corner index 0) is the camera position itself!
    float4 satWorldPosTmp = mul(satInvViewProj, kNdcCorners[0]);
    float3 satWorldPos = satWorldPosTmp.xyz / satWorldPosTmp.w;

    // 4. Calculate Main Observer View Space vectors
    // Main observer camera is at (0,0,0) in its own view space.
    float4 satViewPos = mul(g_ubo.view, float4(satWorldPos, 1.0));

    // Earth's center in World Space is (0,0,0). Find it in View Space:
    float3 earthCenterView = mul(g_ubo.view, float4(0.0, 0.0, 0.0, 1.0)).xyz;

    // Vector from the Earth's center to the Satellite (This is also the horizon normal direction)
    float3 earthToSatWorld = satWorldPos - float3(0.0, 0.0, 0.0);
    float3 horizonNormalView = mul(g_ubo.view, float4(earthToSatWorld, 0.0)).xyz;

    // Vector from the main observer camera to the Earth's horizon point facing the satellite
    // Earth radius is normalized to 1.0f in your cubesphere mesh population
    float3 earthHorizonPointView = earthCenterView + horizonNormalView;
    float3 viewRayToHorizon = earthHorizonPointView; // Camera is at (0,0,0) in view space, so vector is just the point

    // ==========================================
    // 5. The True Perspective Horizon Cull
    // ==========================================
    // If the dot product of the horizon normal and the camera view ray to that horizon is > 0.0,
    // the horizon point is facing away from the camera, putting the satellite behind the Earth's curvature.
    if (dot(horizonNormalView, viewRayToHorizon) > 0.0)
    {
        // banish to shadow realm
        out.pos = float4(-10.0, -10.0, -10.0, 1.0);
        out.color = float4(0.0, 0.0, 0.0, 0.0);
        return out;
    }

    // 6. Generate vertex position normally if visible
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