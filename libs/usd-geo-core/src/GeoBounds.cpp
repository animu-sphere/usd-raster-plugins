#include "usdgeo/GeoBounds.h"

#include <cmath>
#include <limits>

namespace usdgeo {

namespace {
constexpr double kInfinity = std::numeric_limits<double>::infinity();

bool IsFinite(const Vec3d& point) {
    return std::isfinite(point.x) && std::isfinite(point.y) &&
           std::isfinite(point.z);
}
}  // namespace

GeoBounds GeoBounds::Empty() {
    GeoBounds bounds;
    bounds.min = {kInfinity, kInfinity, kInfinity};
    bounds.max = {-kInfinity, -kInfinity, -kInfinity};
    return bounds;
}

bool GeoBounds::IsValid() const {
    return min.x <= max.x && min.y <= max.y && min.z <= max.z;
}

void GeoBounds::Expand(const Vec3d& point) {
    if (!IsFinite(point)) {
        return;
    }
    if (point.x < min.x) min.x = point.x;
    if (point.y < min.y) min.y = point.y;
    if (point.z < min.z) min.z = point.z;
    if (point.x > max.x) max.x = point.x;
    if (point.y > max.y) max.y = point.y;
    if (point.z > max.z) max.z = point.z;
}

void GeoBounds::Union(const GeoBounds& other) {
    if (!other.IsValid()) {
        return;
    }
    Expand(other.min);
    Expand(other.max);
}

Vec3d GeoBounds::Center() const {
    // Computed as min + size/2 rather than (min + max)/2. The two agree for
    // ordinary values, but the midpoint form overflows to infinity for extents
    // spanning half the double range, and this form does not.
    return {min.x + (max.x - min.x) * 0.5, min.y + (max.y - min.y) * 0.5,
            min.z + (max.z - min.z) * 0.5};
}

Vec3d GeoBounds::Size() const {
    if (!IsValid()) {
        return {0.0, 0.0, 0.0};
    }
    return {max.x - min.x, max.y - min.y, max.z - min.z};
}

bool operator==(const GeoBounds& left, const GeoBounds& right) {
    return left.min == right.min && left.max == right.max;
}

}  // namespace usdgeo
