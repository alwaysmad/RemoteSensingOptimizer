#include "shared.slang"

[[vk::binding(0, 0)]] ConstantBuffer<UBO> g_ubo;
[[vk::binding(1, 0)]] StructuredBuffer<VertexCoords> g_coords;
[[vk::binding(2, 0)]] StructuredBuffer<VertexData> g_data;
[[vk::binding(3, 0)]] RWByteAddressBuffer g_result;

groupshared float s_threadSums[256];

[shader("compute")]
[numthreads(256, 1, 1)]
void reduceMain(uint3 groupID: SV_GroupID, uint3 groupThreadID: SV_GroupThreadID, uint groupIndex: SV_GroupIndex)
{
    // === THE 2D UNPACK MATH ===
    uint flatGroupID = groupID.y * 65535 + groupID.x;
    uint idx = flatGroupID * 256 + groupThreadID.x;

    float myValue = 0.0;

    if (idx < g_ubo.vertexCount)
    {
        float alpha = g_coords[idx].values.w;
        float s = g_data[idx].values.z;
        float w = g_data[idx].values.w;

        myValue = alpha * w;
    }

    s_threadSums[groupIndex] = myValue;
    GroupMemoryBarrierWithGroupSync();

    for (uint stride = 128; stride > 0; stride >>= 1)
    {
        if (groupIndex < stride)
        {
            s_threadSums[groupIndex] += s_threadSums[groupIndex + stride];
        }
        GroupMemoryBarrierWithGroupSync();
    }

    if (groupIndex == 0)
    {
        float groupSum = s_threadSums[0];

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
