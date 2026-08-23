#ifndef USDGEO_LOCALORIGIN_H
#define USDGEO_LOCALORIGIN_H

#include "usdgeo/GeoBounds.h"
#include "usdgeo/Vec3d.h"

namespace usdgeo {

/// The offset that makes `float` stage positions viable.
///
/// A projected easting near 500000 m resolves to about 0.03 m in `float`,
/// which is visible wobble on a terrain surface. Authored `points` are `float`
/// because that is what `UsdGeomMesh` stores, so the fix is to move the origin
/// rather than to widen the storage: positions are authored relative to an
/// explicit origin, and the source coordinate stays exactly recoverable as
///
///     sourceCoordinate = geo:localOrigin + stageLocalPosition
///
/// See ADR-0006 and section 5 of docs/architecture/COORDINATE_MODEL.md.
///
/// **This type does not apply the up-axis mapping.** Subtracting the origin
/// and remapping X/Y/Z onto a Y-up stage are separate steps, and the remap
/// lives in exactly one place, `usdRasterAuthoring`. Doing it here would put
/// the sign convention in two places, which is how a north/south flip survives
/// a code review.
class LocalOrigin {
public:
    LocalOrigin() = default;
    explicit LocalOrigin(const Vec3d& value) : _value(value) {}

    /// Derive a stable origin from an extent.
    ///
    /// The origin is the centre of the extent, quantized to a multiple of
    /// `quantum`. Quantization is the point of this function rather than an
    /// optimization: an un-quantized centre moves when a tile is added or a
    /// window changes, and a moving origin invalidates every cached tile and
    /// every already-authored position. `quantum` defaults to one metre, which
    /// is coarse enough to absorb a window change and fine enough that the
    /// residual coordinates stay small.
    ///
    /// An invalid extent yields the origin at zero, which is correct: nothing
    /// is known to offset.
    static LocalOrigin FromBounds(const GeoBounds& bounds,
                                  double quantum = 1.0);

    const Vec3d& GetValue() const { return _value; }

    /// Source coordinate -> stage-local offset. Still `double`; narrowing to
    /// `float` is the authoring library's step, and it happens after this one
    /// so the subtraction never loses precision it could have kept.
    Vec3d ToStageLocal(const Vec3d& source) const;

    /// The exact inverse of `ToStageLocal`, which is the property the
    /// recovery formula in the metadata contract depends on.
    Vec3d ToSource(const Vec3d& stageLocal) const;

private:
    Vec3d _value;
};

inline bool operator==(const LocalOrigin& left, const LocalOrigin& right) {
    return left.GetValue() == right.GetValue();
}

}  // namespace usdgeo

#endif  // USDGEO_LOCALORIGIN_H
