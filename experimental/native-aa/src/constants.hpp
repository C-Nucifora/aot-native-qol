#pragma once
#include <array>
#include <cstdint>
#include <optional>

namespace aot::postprocess {
struct alignas(16) CasConstants {
    std::array<std::uint32_t, 4> const0;
    std::array<std::uint32_t, 4> const1;
    std::array<std::uint32_t, 2> extent;
    std::array<std::uint32_t, 2> padding;
};
struct Constants {
    std::array<float, 4> smaa_metrics;
    CasConstants cas;
};
// Reject invalid inputs; caller must skip the optional effect on failure.
std::optional<Constants> make_constants(std::uint32_t width, std::uint32_t height, float sharpness);
}
