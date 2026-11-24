#include "shared.slang"

static const uint kWorkgroupSize = 256u;
static const uint kMaxGroupsPerDimension = 65535u;

[[vk::binding(0, 0)]] ConstantBuffer<UBO> g_ubo;
[[vk::binding(1, 0)]] StructuredBuffer<VertexCoords> g_coords;
[[vk::binding(2, 0)]] StructuredBuffer<VertexData> g_data;
[[vk::binding(3, 0)]] RWStructuredBuffer<float> g_partials;

groupshared float s_threadSums[kWorkgroupSize];

[shader("compute")]
[numthreads(256, 1, 1)]
void reducePartials(uint3 groupID: SV_GroupID, uint3 groupThreadID: SV_GroupThreadID, uint groupIndex: SV_GroupIndex)
{
    uint totalGroups = (g_ubo.vertexCount + (kWorkgroupSize - 1u)) / kWorkgroupSize;
    uint flatGroupID = groupID.y * kMaxGroupsPerDimension + groupID.x;

    // 2D dispatch guard: padded workgroups when groupCountY > 1 must exit before writing g_partials.
    if (flatGroupID >= totalGroups)
        return;

    uint idx = flatGroupID * kWorkgroupSize + groupThreadID.x;
    float myValue = 0.0f;

    if (idx < g_ubo.vertexCount)
    {
        float alpha = g_coords[idx].values.w;
        float s = g_data[idx].values.z;
        float w = g_data[idx].values.w;

        myValue = alpha * w * s;
    }

    s_threadSums[groupIndex] = myValue;
    GroupMemoryBarrierWithGroupSync();

    for (uint stride = kWorkgroupSize >> 1u; stride > 0u; stride >>= 1u)
    {
        if (groupIndex < stride)
        {
            s_threadSums[groupIndex] += s_threadSums[groupIndex + stride];
        }
        GroupMemoryBarrierWithGroupSync();
    }

    if (groupIndex == 0u)
    {
        g_partials[flatGroupID] = s_threadSums[0];
    }
}
