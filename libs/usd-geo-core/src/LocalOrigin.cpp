#include "usdgeo/LocalOrigin.h"

#include <cmath>

namespace usdgeo {

namespace {

/// Round to the nearest multiple of `quantum`, halves away from zero.
///
/// `std::round` rather than `std::nearbyint`: `nearbyint` honours the current
/// rounding mode, so a host that changed it would get a different origin from
/// the same extent, and the origin has to be reproducible across the
/// converter, the plugin, and the cache key that names them both.
double Quantize(double value, double quantum) {
    if (!(quantum > 0.0) || !std::isfinite(value)) {
        return std::isfinite(value) ? value : 0.0;
    }
    return std::round(value / quantum) * quantum;
}

}  // namespace

LocalOrigin LocalOrigin::FromBounds(const GeoBounds& bounds, double quantum) {
    if (!bounds.IsValid()) {
        return LocalOrigin();
    }
    const Vec3d center = bounds.Center();
    return LocalOrigin(Vec3d{Quantize(center.x, quantum),
                             Quantize(center.y, quantum),
                             Quantize(center.z, quantum)});
}

Vec3d LocalOrigin::ToStageLocal(const Vec3d& source) const {
    return source - _value;
}

Vec3d LocalOrigin::ToSource(const Vec3d& stageLocal) const {
    return stageLocal + _value;
}

}  // namespace usdgeo
