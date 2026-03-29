#pragma once

static const uint MAX_AGENTS = 512;

struct AgentData
{
    float4x4 camera;
    float4 data;
};

struct UBO
{
    float4x4 viewProj;
    float4x4 model;
    uint vertexCount;
    uint agentCount;
    float timeStep;
    uint _padding0;
    AgentData agents[MAX_AGENTS];
};

struct VertexCoords
{
    int16_t4 values;
};

struct VertexData
{
    uint16_t4 values;
};