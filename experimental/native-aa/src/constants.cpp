#include "constants.hpp"
#include <cmath>
#define A_CPU 1
#define A_STATIC static inline
#define A_RESTRICT
#include "../third_party/cas/ffx_a.h"
#include "../third_party/cas/ffx_cas.h"

namespace aot::postprocess {
static_assert(sizeof(CasConstants) == 48);
std::optional<Constants> make_constants(std::uint32_t width, std::uint32_t height, float sharpness) {
    if (!width || !height || !std::isfinite(sharpness) || sharpness < 0.f || sharpness > 1.f)
        return std::nullopt;
    Constants result{};
    result.smaa_metrics = {1.f / width, 1.f / height, float(width), float(height)};
    CasSetup(result.cas.const0.data(), result.cas.const1.data(), sharpness,
             float(width), float(height), float(width), float(height));
    result.cas.extent = {width, height};
    return result;
}
}
