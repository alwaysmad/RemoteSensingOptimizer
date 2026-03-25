#pragma once

namespace svk 
{
    // The Engine's ultimate source of truth for validation.
#ifdef NDEBUG
    constexpr bool enableValidationLayers = false;
#else
    constexpr bool enableValidationLayers = true;
#endif

    // (You can add other engine-wide constexpr flags here later)
}