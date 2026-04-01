#include "shared.slang"

[[vk::binding(0, 0)]] ConstantBuffer<UBO> g_ubo;
[[vk::binding(1, 0)]] StructuredBuffer<uint4> g_coords;
[[vk::binding(2, 0)]] StructuredBuffer<uint4> g_data;
[[vk::binding(3, 0)]] RWByteAddressBuffer g_result;

groupshared float s_subgroupSums[32];

[shader("compute")]
[numthreads(256, 1, 1)]
void reduceMain(uint3 groupID: SV_GroupID, uint3 groupThreadID: SV_GroupThreadID, uint groupIndex: SV_GroupIndex)
{
    // === THE 2D UNPACK MATH ===
    uint flatGroupID = groupID.y * 65535 + groupID.x;
    uint idx = flatGroupID * 256 + groupThreadID.x;

    float myValue = 0.0;

    // Divide vertexCount by 2 because each thread now processes TWO vertices
    if (idx < g_ubo.vertexCount)
    {
        uint4 c = g_coords[idx];
        uint4 d = g_data[idx];

        // Process Cell A (lower 8 bytes)
        float3 posA; float alphaA;
        unpackCoords(c.xy, posA, alphaA);
        float epsA, vA, sA, wA;
        unpackData(d.xy, epsA, vA, sA, wA);

        // Process Cell B (upper 8 bytes)
        float3 posB; float alphaB;
        unpackCoords(c.zw, posB, alphaB);
        float epsB, vB, sB, wB;
        unpackData(d.zw, epsB, vB, sB, wB);

        // Sum both cells for this thread (Remove 's' if computing pure Omega coverage!)
        myValue = wA + wB;
    }

    float waveSum = WaveActiveSum(myValue);

    if (WaveIsFirstLane())
    {
        uint waveID = groupIndex / WaveGetLaneCount();
        s_subgroupSums[waveID] = waveSum;
    }
    GroupMemoryBarrierWithGroupSync();

    if (groupIndex == 0)
    {
        float groupSum = 0.0;
        uint numWaves = (256 + WaveGetLaneCount() - 1) / WaveGetLaneCount();
        for (uint i = 0; i < numWaves; ++i) { groupSum += s_subgroupSums[i]; }

        uint prev;
        g_result.InterlockedCompareExchange(0, 0, 0, prev);
        uint cmp;
        do {
            cmp = prev;
            uint newVal = asuint(asfloat(cmp) + groupSum);
            g_result.InterlockedCompareExchange(0, cmp, newVal, prev);
        } while (prev != cmp);
    }
}
