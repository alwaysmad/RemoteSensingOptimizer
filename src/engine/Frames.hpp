#pragma once

namespace svk 
{
    constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    inline uint32_t advanceFrame(uint32_t currentFrame) { return currentFrame ^ 1u; }
} // namespace svk