#pragma once

static const uint MAX_AGENTS = 512;

struct AgentData
{
    float4x4 camera;
    float4 data;
};

struct UBO
{
    float4x4 view;
    float4x4 proj;
    float4x4 model;
    uint vertexCount;
    uint agentCount;
    float timeStep;
    uint _padding0;
    AgentData agents[MAX_AGENTS];
};

struct VertexCoords
{
    float4 values;
};

struct VertexData
{
    float4 values;
};

// ==========================================
// Camera Packing Contract (CPU ↔ GPU)
// ==========================================
// The C++ side (Application.cpp) packs both view AND projection parameters
// into a single float4x4 to compress satellite data to 64 bytes per agent.
//
// GLM is COLUMN-major: view[col][row]. Slang is ROW-major: c[row][col].
// After memcpy across the boundary, GLM column N becomes Slang row N.
//
// The C++ side overwrites the 4th element of each GLM column (row 3):
//   view[0][3] = tanHalfFov   → Slang c[3].x
//   view[1][3] = aspect       → Slang c[3].y
//   view[2][3] = zNear        → Slang c[3].z
//   view[3][3] = zFar         → Slang c[3].w
//
// The original lookAt translation lives in GLM's 4th column (view[3][0..2]),
// which maps to Slang c[0].w, c[1].w, c[2].w — these are NOT overwritten.
//
// unpackCamera: extracts projection params from c[3], restores c[3] to {0,0,0,1}.
// getInvViewProj: uses c[i].w (i=0,1,2) as the preserved translation components.
//
// WARNING: If the C++ packing scheme changes, BOTH functions must be updated.
// ==========================================

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

float4x4 getInvViewProj(float4x4 c)
{
    float t = c[3].x;
    float a = c[3].y;
    float n = c[3].z;
    float f = c[3].w;

    float4x4 iView = float4x4(
        float4(c[0].x, c[1].x, c[2].x, -c[0].w * c[0].x - c[1].w * c[1].x - c[2].w * c[2].x),
        float4(c[0].y, c[1].y, c[2].y, -c[0].w * c[0].y - c[1].w * c[1].y - c[2].w * c[2].y),
        float4(c[0].z, c[1].z, c[2].z, -c[0].w * c[0].z - c[1].w * c[1].z - c[2].w * c[2].z),
        float4(0.0, 0.0, 0.0, 1.0)
    );

    float4x4 iProj = float4x4(
        (a * t), 0.0, 0.0, 0.0,
        0.0, -t, 0.0, 0.0,
        0.0, 0.0, 0.0, -1.0,
        0.0, 0.0, 1.0 / f - 1.0 / n, 1.0 / n
    );

    return mul(iView, iProj);
}