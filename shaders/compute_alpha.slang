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

    // 3. Read vertex payload directly as 32-bit floats
    float3 localPos = g_coords[idx].values.xyz;
    float alpha = g_coords[idx].values.w;

    float epsilon = g_data[idx].values.x;
    float v = g_data[idx].values.y;
    float s = g_data[idx].values.z;
    float w_val = g_data[idx].values.w;

    // 4. Transform to World Space (Do this ONCE per vertex, not inside the loop)
    float4 worldPos = mul(g_ubo.model, float4(localPos, 1.0));
    float3 worldNormal = mul(g_ubo.model, float4(localPos, 0.0)).xyz;

    // 5. Compute Omega (Total Survey Quality)
    float Omega = 0.0;
    // iterate over ALL agents
    for (uint i = 0; i < agentCount; ++i)
    {
        float4x4 view, proj;
        unpackCamera(s_agentCamera[i], view, proj);

        // Transform to Satellite's View Space
        float4 viewPos = mul(view, worldPos);
        float3 viewNormal = mul(view, float4(worldNormal, 0.0)).xyz;

        // 1. Raw dot product
        float nDotV = dot(viewNormal, viewPos.xyz);

        // 2. Horizon Cull (If dot > 0, it's facing away)
        if (nDotV > 0.0)
            continue;

        // 3. Viewport Check
        float4 clipPos = mul(proj, viewPos);
        
        // Ensure the point is actually in front of the camera (w > 0)
        if (clipPos.w > 0.0)
        {
            float3 ndc = clipPos.xyz / clipPos.w;
            
            // Is it on screen?
            if (ndc.x > -1.0 && ndc.x < 1.0 && ndc.y > -1.0 && ndc.y < 1.0)
            {
                // ==========================================
                // 4. The Jacobian Determinant (Flat CMOS Physics)
                // ==========================================
                // Extract projection scaling factors from the matrix
                float P00 = proj[0][0]; 
                float P11 = proj[1][1]; 

                // Vulkan's P11 is usually negative (to flip Y), so we take abs()
                float areaScale = abs(P00 * P11);

                // w is the depth along the optical axis. Cubing it replaces the 
                // inverse-square law and accounts for perspective stretching!
                float w3 = clipPos.w * clipPos.w * clipPos.w;

                // The differential area in NDC space.
                // -nDotV flips the negative dot product to positive.
                float ndcArea = areaScale * (-nDotV) / w3;

                // 5. Normalized Sensor Coverage
                // Divide by 4.0 because the total NDC screen area is 2x2
                Omega += ndcArea / 4.0;
            }
        }
    }

    // 6. Backwards Euler Integration
    //Omega *= s * g_ubo.timeStep;
    float f = 1.0 / g_ubo.timeStep;
    //float alpha_new = (f * alpha + epsilon) / (f + epsilon + Omega * v);
    float alpha_new = Omega * s;

    // 7. Write updated alpha directly to VRAM
    g_coords[idx].values.w = clamp(alpha_new, -1.0, 1.0);
    //g_coords[idx].values.w = alpha_new;
}