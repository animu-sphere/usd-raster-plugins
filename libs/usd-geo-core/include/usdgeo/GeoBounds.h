#ifndef USDGEO_GEOBOUNDS_H
#define USDGEO_GEOBOUNDS_H

#include "usdgeo/Vec3d.h"

namespace usdgeo {

/// An axis-aligned extent in source coordinates.
///
/// Source coordinates, not stage-local ones: bounds are computed and compared
/// before the local origin is subtracted, so that a bounds value is stable
/// regardless of which origin a later conversion picks. Section 1 of
/// docs/architecture/COORDINATE_MODEL.md fixes the distinction.
struct GeoBounds {
    Vec3d min;
    Vec3d max;

    /// The empty bounds: an inverted range that any real point widens. A
    /// default-constructed `GeoBounds` is the degenerate box at the origin,
    /// which is a valid extent, so an accumulator must start from `Empty`.
    static GeoBounds Empty();

    bool IsValid() const;

    /// Widen to include a point. Non-finite components are ignored rather than
    /// poisoning the extent: a single NaN elevation in a DEM would otherwise
    /// make every bound NaN, and the NoData path is where a sentinel elevation
    /// belongs.
    void Expand(const Vec3d& point);

    /// Widen to include another extent. An invalid extent contributes nothing.
    ///
    /// Named `Union` rather than being a second `Expand` overload on purpose:
    /// `GeoBounds` is an aggregate of two `Vec3d`, so brace elision makes
    /// `Expand({1.0, 2.0, 3.0})` ambiguous between the two -- it could be a
    /// point or a partly-initialized extent. One name per parameter type
    /// removes the trap instead of leaving it for each caller to trip over.
    void Union(const GeoBounds& other);

    Vec3d Center() const;
    Vec3d Size() const;
};

bool operator==(const GeoBounds& left, const GeoBounds& right);

}  // namespace usdgeo

#endif  // USDGEO_GEOBOUNDS_H
