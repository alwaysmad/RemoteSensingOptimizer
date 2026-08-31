// Simple triangle shader with hardcoded vertices

struct VertexOutput
{
    [[vk::location(0)]]
    float3 color;
    float4 sv_position : SV_Position;
};

struct FragmentInput
{
    [[vk::location(0)]]
    float3 color;
};

[shader("vertex")]
VertexOutput vertMain(uint vertexIndex : SV_VertexID)
{
    VertexOutput output;
    
    // Hardcoded triangle vertices (normalized device coordinates)
    // Triangle centered at origin, roughly 0.5 units wide and tall
    float3 vertices[3] = {
        float3(-0.5, -0.5, 0.0),  // Bottom-left
        float3( 0.5, -0.5, 0.0),  // Bottom-right
        float3( 0.0,  0.5, 0.0)   // Top
    };
    
    // Hardcoded per-vertex colors
    float3 colors[3] = {
        float3(1.0, 0.0, 0.0),    // Red
        float3(0.0, 1.0, 0.0),    // Green
        float3(0.0, 0.0, 1.0)     // Blue
    };
    
    // Get vertex data from hardcoded arrays
    float3 pos = vertices[vertexIndex];
    output.color = colors[vertexIndex];
    
    // Output in clip space (already in NDC)
    output.sv_position = float4(pos, 1.0);
    
    return output;
}

[shader("fragment")]
float4 fragMain(FragmentInput input) : SV_Target
{
    // Simple fragment shader: output the interpolated color
    return float4(input.color, 1.0);
}
