#include "shared.slang"

static const uint kWorkgroupSize = 256u;

[[vk::binding(0, 0)]] ConstantBuffer<UBO> g_ubo;
[[vk::binding(1, 0)]] StructuredBuffer<float> g_partials;
[[vk::binding(2, 0)]] RWStructuredBuffer<float> g_result;

groupshared float s_threadSums[kWorkgroupSize];
groupshared float s_threadComps[kWorkgroupSize];

[shader("compute")]
[numthreads(256, 1, 1)]
void reduceFinal(uint groupIndex: SV_GroupIndex)
{
    uint totalGroups = (g_ubo.vertexCount + (kWorkgroupSize - 1u)) / kWorkgroupSize;

    // Kahan compensated summation in portable IEEE-754 float32 (no float64 / float atomics).
    float sum = 0.0f;
    float c = 0.0f;
    for (uint g = groupIndex; g < totalGroups; g += kWorkgroupSize)
    {
        float y = g_partials[g] - c;
        float t = sum + y;
        c = (t - sum) - y;
        sum = t;
    }

    // Track true value as s_threadSums[i] + s_threadComps[i] through the 8-step tree reduction.
    s_threadSums[groupIndex] = sum;
    s_threadComps[groupIndex] = -c;
    GroupMemoryBarrierWithGroupSync();

    for (uint stride = kWorkgroupSize >> 1u; stride > 0u; stride >>= 1u)
    {
        if (groupIndex < stride)
        {
            float a = s_threadSums[groupIndex];
            float b = s_threadSums[groupIndex + stride];
            float t = a + b;
            float err = (abs(a) >= abs(b)) ? ((a - t) + b) : ((b - t) + a);
            s_threadSums[groupIndex] = t;
            s_threadComps[groupIndex] += s_threadComps[groupIndex + stride] + err;
        }
        GroupMemoryBarrierWithGroupSync();
    }

    if (groupIndex == 0u)
    {
        g_result[0] = s_threadSums[0] + s_threadComps[0];
    }
}
