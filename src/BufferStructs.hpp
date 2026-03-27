#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include <glm/gtc/packing.hpp>
#include <vulkan/vulkan_raii.hpp>

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
