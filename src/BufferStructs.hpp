#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/gtc/packing.hpp>
#include <glm/mat4x4.hpp>
#include <vulkan/vulkan_raii.hpp>

struct alignas(16) UBO
{
    glm::mat4 viewProj;
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
    alignas(16) std::vector<uint32_t> indices;

    Mesh() = default;

    inline void populateCube()
    {
        vertices.clear();
        vertexData.clear();
        indices.clear();

        vertices.reserve(8);
        vertexData.reserve(8);
        indices.reserve(36);

        constexpr std::array<std::array<float, 4>, 8> cubeVertices = {
            std::array<float, 4>{-0.5f, -0.5f, -0.5f, 0.15f},
            std::array<float, 4>{ 0.5f, -0.5f, -0.5f, 0.30f},
            std::array<float, 4>{ 0.5f,  0.5f, -0.5f, 0.45f},
            std::array<float, 4>{-0.5f,  0.5f, -0.5f, 0.60f},
            std::array<float, 4>{-0.5f, -0.5f,  0.5f, 0.35f},
            std::array<float, 4>{ 0.5f, -0.5f,  0.5f, 0.55f},
            std::array<float, 4>{ 0.5f,  0.5f,  0.5f, 0.75f},
            std::array<float, 4>{-0.5f,  0.5f,  0.5f, 1.00f},
        };

        for (const auto& coords : cubeVertices)
        {
            vertices.emplace_back(coords);
            vertexData.emplace_back(std::array<float, 4>{coords[3], coords[3], coords[3], 1.0f});
        }

        indices = {
            0, 1, 2, 2, 3, 0,
            1, 5, 6, 6, 2, 1,
            5, 4, 7, 7, 6, 5,
            4, 0, 3, 3, 7, 4,
            3, 2, 6, 6, 7, 3,
            4, 5, 1, 1, 0, 4,
        };
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
