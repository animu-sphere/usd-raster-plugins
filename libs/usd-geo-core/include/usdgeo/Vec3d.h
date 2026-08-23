// usdGeoCore -- format-independent geospatial values.
//
// This header must not include an OpenUSD, libtiff, PROJ, GDAL, or transport
// header. That is invariant 1 of docs/architecture/WORKSPACE.md, and the
// core CI lane exists to catch a violation of it.

#ifndef USDGEO_VEC3D_H
#define USDGEO_VEC3D_H

namespace usdgeo {

/// A point or offset in a source coordinate reference system.
///
/// Always `double`. Projected coordinates are routinely in the hundreds of
/// thousands of metres, where `float` resolves to centimetres at best, so
/// every internal computation stays in `double` and only the final authored
/// position narrows to `float` -- relative to a local origin. See section 5 of
/// docs/architecture/COORDINATE_MODEL.md.
struct Vec3d {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

inline Vec3d operator+(const Vec3d& left, const Vec3d& right) {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

inline Vec3d operator-(const Vec3d& left, const Vec3d& right) {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

inline bool operator==(const Vec3d& left, const Vec3d& right) {
    return left.x == right.x && left.y == right.y && left.z == right.z;
}

inline bool operator!=(const Vec3d& left, const Vec3d& right) {
    return !(left == right);
}

}  // namespace usdgeo

#endif  // USDGEO_VEC3D_H
