#include "shared.slang"

[[vk::binding(0, 0)]]
ConstantBuffer<UBO> g_ubo;

[[vk::binding(1, 0)]]
StructuredBuffer<VertexCoords> g_vertexCoords;

[[vk::binding(2, 0)]]
StructuredBuffer<VertexData> g_vertexData;

[shader("compute")]
[numthreads(1, 1, 1)]
void compMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (g_ubo.vertexCount == 0)
        { return; }

    // Placeholder only: touch inputs so the interface is validated end-to-end.
    const uint idx = dispatchThreadID.x % g_ubo.vertexCount;
    const int16_t4 coords = g_vertexCoords[idx].values;
    const uint16_t4 data = g_vertexData[idx].values;
    const uint sink =
        uint(coords.x + coords.y + coords.z + coords.w) +
        uint(data.x + data.y + data.z + data.w);
    if (sink == 0xFFFFFFFFu)
        { return; }
}