#pragma once

namespace svk 
{
#ifdef NDEBUG
    constexpr bool enableValidationLayers = false;
#else
    constexpr bool enableValidationLayers = true;
#endif

    // (You can add other engine-wide constexpr flags here later)
}