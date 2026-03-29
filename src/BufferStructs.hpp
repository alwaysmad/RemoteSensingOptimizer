#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <vector>

#include <glm/gtc/packing.hpp>
#include <glm/mat4x4.hpp>
#include <vulkan/vulkan_raii.hpp>

constexpr uint32_t MAX_AGENTS = 512;

struct alignas(16) AgentData
{
    glm::mat4 camera;
    float data[4];
};

struct alignas(16) UBO
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 model;
    uint32_t vertexCount;
    uint32_t agentCount;
    float timeStep;
    uint32_t _padding0;
    AgentData agents[MAX_AGENTS];
};

struct alignas(8) VertexCoords
{
    int16_t values[4];

    VertexCoords() = default;

    explicit VertexCoords(const std::array<float, 4>& input)
        : values {
            packSnorm(input[0]),
            packSnorm(input[1]),
            packSnorm(input[2]),
            packSnorm(input[3])
        }
    {}

private:
    [[nodiscard]] static inline int16_t packSnorm(float value)
    {
        const float clamped = std::clamp(value, -1.0f, 1.0f);
        return static_cast<int16_t>(std::round(clamped * 32767.0f));
    }
};

struct alignas(8) VertexData
{
    uint16_t values[4];

    VertexData() = default;

    explicit VertexData(const std::array<float, 4>& input)
        : values {
            glm::packHalf1x16(input[0]),
            glm::packHalf1x16(input[1]),
            glm::packHalf1x16(input[2]),
            glm::packHalf1x16(input[3])
        }
    {}
};

struct Mesh
{
    std::vector<VertexCoords> vertices;
    std::vector<VertexData> vertexData;

    Mesh() = default;

    // Helper to pack float into 16-bit SNORM (-1.0 to 1.0)
    static inline int16_t packSnorm16(float v)
    {
        return static_cast<int16_t>(std::clamp(v, -1.0f, 1.0f) * 32767.0f);
    }

    // Helper to calculate the exact solid angle of a rectangle on the z=1 face
    static inline float solidAngle(float x, float y)
    {
        return std::atan( (x * y) / std::sqrt(x * x + y * y + 1.0f) );
    }

    // Populates a point-cloud cubesphere. 
    // 'resolution' is the number of cells per dimension on a single face.
    inline void populateMesh(uint32_t resolution)
    {
        vertices.clear();
        vertexData.clear();

        const uint32_t cellsPerFace = resolution * resolution;
        const uint32_t totalCells = 6 * cellsPerFace;
        const size_t vertexBytes = static_cast<size_t>(totalCells) * sizeof(VertexCoords);
        const size_t vertexDataBytes = static_cast<size_t>(totalCells) * sizeof(VertexData);
        const size_t totalBytes = vertexBytes + vertexDataBytes;
        const double totalMB = static_cast<double>(totalBytes) / (1024.0 * 1024.0);
        const double totalGB = static_cast<double>(totalBytes) / (1024.0 * 1024.0 * 1024.0);

        std::printf("[Mesh] total cells: %u\n", totalCells);
        if (totalBytes < (1ull << 30))
            { std::printf("[Mesh] total required memory: %.2f MB\n", totalMB); }
        else
            { std::printf("[Mesh] total required memory: %.2f GB\n", totalGB); }
        
        vertices.reserve(totalCells);
        vertexData.reserve(totalCells);

        const float invRes = 1.0f / static_cast<float>(resolution);
        const float pi_over_4 = glm::pi<float>() / 4.0f;

        for (int face = 0; face < 6; ++face)
        {
            for (uint32_t i = 0; i < resolution; ++i)
            {
                for (uint32_t j = 0; j < resolution; ++j)
                {
                    // 1. Grid coordinates mapped to [-1.0, 1.0]
                    const float u0 = -1.0f + 2.0f * (static_cast<float>(i) * invRes);
                    const float u1 = -1.0f + 2.0f * (static_cast<float>(i + 1) * invRes);
                    const float v0 = -1.0f + 2.0f * (static_cast<float>(j) * invRes);
                    const float v1 = -1.0f + 2.0f * (static_cast<float>(j + 1) * invRes);

                    const float uCenter = (u0 + u1) * 0.5f;
                    const float vCenter = (v0 + v1) * 0.5f;

                    // 2. Equi-angular Warp (minimizes area dispersion)
                    const float x0 = std::tan(u0 * pi_over_4);
                    const float x1 = std::tan(u1 * pi_over_4);
                    const float y0 = std::tan(v0 * pi_over_4);
                    const float y1 = std::tan(v1 * pi_over_4);

                    const float xCenter = std::tan(uCenter * pi_over_4);
                    const float yCenter = std::tan(vCenter * pi_over_4);

                    // 3. Exact surface area calculation (Solid Angle of the cell)
                    const float area = solidAngle(x1, y1) - solidAngle(x0, y1) - solidAngle(x1, y0) + solidAngle(x0, y0);

                    // 4. Map the 2D face coordinates to 3D cube coordinates based on the face index
                    glm::vec3 pos;
                    switch (face)
                    {
                        case 0: pos = glm::vec3( 1.0f, -yCenter, -xCenter); break; // +X
                        case 1: pos = glm::vec3(-1.0f, -yCenter,  xCenter); break; // -X
                        case 2: pos = glm::vec3( xCenter,  1.0f,  yCenter); break; // +Y
                        case 3: pos = glm::vec3( xCenter, -1.0f, -yCenter); break; // -Y
                        case 4: pos = glm::vec3( xCenter, -yCenter,  1.0f); break; // +Z
                        case 5: pos = glm::vec3(-xCenter, -yCenter, -1.0f); break; // -Z
                    }

                    // 5. Normalize to project onto the sphere
                    pos = glm::normalize(pos);

                    // 6. Define payload parameters
                    constexpr float alpha = 1.0f;
                    constexpr float epsilon = 0.05f;
                    constexpr float v = 1.0f;
                    const float s = area;
                    constexpr float w = 1.0f;

                    // 7. Add vertex to GPU arrays
                    vertices.emplace_back(std::array<float, 4>{ pos.x, pos.y, pos.z, alpha });
                    vertexData.emplace_back(std::array<float, 4>{ epsilon, v, s, w });
                }
            }
            std::printf("[Mesh] generated face %d/6\n", face + 1);
        }
    }
};

static constexpr std::array<vk::VertexInputBindingDescription, 1> kVertexCoordsBindingDescriptions {{
    {
        .binding = 0,
        .stride = sizeof(VertexCoords),
        .inputRate = vk::VertexInputRate::eVertex,
    }
}};

static constexpr std::array<vk::VertexInputAttributeDescription, 1> kVertexCoordsAttributeDescriptions {{
    {
        .location = 0,
        .binding = 0,
        .format = vk::Format::eR16G16B16A16Snorm,
        .offset = static_cast<uint32_t>(offsetof(VertexCoords, values)),
    }
}};

static constexpr std::array<vk::VertexInputBindingDescription, 1> kVertexDataBindingDescriptions {{
    {
        .binding = 1,
        .stride = sizeof(VertexData),
        .inputRate = vk::VertexInputRate::eVertex,
    }
}};

static constexpr std::array<vk::VertexInputAttributeDescription, 1> kVertexDataAttributeDescriptions {{
    {
        .location = 1,
        .binding = 1,
        .format = vk::Format::eR16G16B16A16Sfloat,
        .offset = static_cast<uint32_t>(offsetof(VertexData, values)),
    }
}};
