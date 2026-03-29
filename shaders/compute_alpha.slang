#include "shared.slang"

// ==========================================
// The Memory Contracts
// ==========================================
[[vk::binding(0, 0)]] ConstantBuffer<UBO> g_ubo;
[[vk::binding(1, 0)]] RWStructuredBuffer<VertexCoords> g_coords; // Read-Write for Alpha
[[vk::binding(2, 0)]] StructuredBuffer<VertexData> g_data;       // Read-Only for Payload

// 32KB L1 Shared Memory Cache for the Workgroup.
// Fits perfectly within the Vulkan guaranteed hardware limit.
groupshared float4x4 s_agentCamera[MAX_AGENTS];

// ==========================================
// Helper: Unpack View & Proj Matrices
// ==========================================
void unpackCamera(float4x4 c, out float4x4 view, out float4x4 proj)
{
    float t = c[3].x;
    float a = c[3].y;
    float n = c[3].z;
    float f = c[3].w;

    view = c;
    view[3].x = 0.0;
    view[3].y = 0.0;
    view[3].z = 0.0;
    view[3].w = 1.0;

    proj = float4x4(
        1.0 / (a * t), 0.0, 0.0, 0.0,
        0.0, -1.0 / t, 0.0, 0.0,
        0.0, 0.0, -f / (f - n), -f * n / (f - n),
        0.0, 0.0, -1.0, 0.0
    );
}

// ==========================================
// The Grand Equation
// ==========================================
// Change the signature to grab SV_GroupID:
[shader("compute")]
[numthreads(256, 1, 1)]
void computeMain(uint3 groupID: SV_GroupID, uint3 groupThreadID: SV_GroupThreadID)
{
    uint agentCount = g_ubo.agentCount;

    // 1. Collaborative Cache Load
    // The 256 threads split the work of loading the agent matrices into L1 Cache
    // Cache load remains perfectly identical (it relies on groupThreadID)
    for (uint i = groupThreadID.x; i < agentCount; i += 256)
    {
        s_agentCamera[i] = g_ubo.agents[i].camera;
    }
    // Halt all threads until the cache is fully loaded
    GroupMemoryBarrierWithGroupSync();

    // === THE 2D UNPACK MATH ===
    // Multiply Y by the MAX_GROUPS width (65535) we defined in C++
    uint flatGroupID = groupID.y * 65535 + groupID.x;
    uint idx = flatGroupID * 256 + groupThreadID.x;

    // 2. Bounds Check (Don't execute on padded threads)
    if (idx >= g_ubo.vertexCount)
        return;

    // 3. Unpack 16-bit Memory
    VertexCoords coords = g_coords[idx];
    VertexData data = g_data[idx];

    // Convert SNORM16 to standard floats
    float3 localPos = float3(coords.values.xyz) / 32767.0;
    float alpha = float(coords.values.w) / 32767.0;

    // Convert SFLOAT16 to standard floats using the hardware intrinsic
    float epsilon = f16tof32(uint(data.values.x));
    float v = f16tof32(uint(data.values.y));

    // 4. Transform to World Space (Do this ONCE per vertex, not inside the loop)
    float4 worldPos = mul(g_ubo.model, float4(localPos, 1.0));
    float3 worldNormal = normalize(mul(g_ubo.model, float4(localPos, 0.0)).xyz);

    // 5. Compute Omega (Total Survey Quality)
    float Omega = 0.0;

    for (uint i = 0; i < agentCount; ++i)
    {
        float4x4 view, proj;
        unpackCamera(s_agentCamera[i], view, proj);

        // Transform to Satellite's View Space
        float4 viewPos = mul(view, worldPos);
        float3 viewNormal = mul(view, float4(worldNormal, 0.0)).xyz;

        // 5.1 Horizon Cull: If dot product > 0, it's facing away from the satellite
        if (dot(viewNormal, viewPos.xyz) > 0.0)
            continue;

        // 5.2 Viewport Check
        float4 clipPos = mul(proj, viewPos);

        // Check if the point is actually in front of the camera lens (w > 0)
        if (clipPos.w > 0.0)
        {
            // Convert to Normalized Device Coordinates (NDC)
            float3 ndc = clipPos.xyz / clipPos.w;

            // Is the point inside the (-1.0, 1.0) boundaries of the camera screen?
            if (ndc.x > -1.0 && ndc.x < 1.0 && ndc.y > -1.0 && ndc.y < 1.0)
            {
                Omega += 1.0;
            }
        }
    }

    // 6. Backwards Euler Integration
    //Omega = 100.0;
    float f = 1.0 / g_ubo.timeStep;
    float alpha_new = (f * alpha + epsilon) / (f + epsilon + Omega * v);

    // 7. Repack and Write to VRAM
    g_coords[idx].values.w = int16_t(clamp(alpha_new, -1.0, 1.0) * 32767.0);
}