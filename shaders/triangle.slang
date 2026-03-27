struct VertexInput
{
    [[vk::location(0)]]
    float4 coords;
};

struct VertexOutput
{
    float3 color;
    float4 sv_position : SV_Position;
};

[shader("vertex")]
VertexOutput vertMain(VertexInput inVert)
{
    VertexOutput output;
    output.sv_position = float4(inVert.coords.xyz, 1.0);

    float gray = saturate(inVert.coords.w);
    output.color = float3(gray, gray, gray);
    return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput inVert) : SV_Target
{
    float3 color = inVert.color;
    return float4(color, 1.0);
}
