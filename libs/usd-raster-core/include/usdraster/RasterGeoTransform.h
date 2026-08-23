#ifndef USDRASTER_RASTERGEOTRANSFORM_H
#define USDRASTER_RASTERGEOTRANSFORM_H

#include "usdraster/RasterTypes.h"
#include "usdraster/RasterWindow.h"

#include <usdgeo/GeoBounds.h>

#include <cstdint>

namespace usdraster {

/// How a file relates its geotransform to its pixel grid.
///
/// This is about the file's convention, not about which position a mesh vertex
/// takes: **a vertex is always sampled at the pixel centre**, under either
/// convention. Conflating the two is the specific half-pixel error this
/// distinction exists to prevent. See section 2 of
/// docs/architecture/COORDINATE_MODEL.md.
enum class PixelAnchor {
    /// Never guessed. GeoTIFF's GTRasterTypeGeoKey is optional, and a source
    /// that omits it gets a diagnostic and an explicit `pixelAnchor` argument,
    /// not a default. A silent default is a half-pixel shift that looks
    /// plausible everywhere.
    Unknown,
    /// A pixel is a cell covering an area; the transform maps the upper-left
    /// corner of pixel (0, 0).
    Area,
    /// A pixel is a point sample; the transform maps the centre of pixel
    /// (0, 0).
    Point
};

const char* GetPixelAnchorName(PixelAnchor anchor);

/// A continuous position in pixel space, as distinct from an integer pixel
/// index. The conversion between the two is where the anchoring convention
/// applies, and naming them differently is what keeps the two from being
/// passed to each other's functions.
struct PixelCoord {
    double px = 0.0;
    double py = 0.0;
};

/// The six-coefficient affine map from pixel space to source coordinates:
///
///     X = a0 + a1 * px + a2 * py
///     Y = b0 + b1 * px + b2 * py
///
/// For a north-up raster `a2` and `b1` are zero, `a1` is the positive pixel
/// width, and `b2` is the **negative** pixel height, because the row index
/// increases southward while Y increases northward. A positive `b2` is legal
/// and means a south-up raster: it is handled and recorded, not rejected.
///
/// Non-zero `a2` or `b1` means a rotated raster. Rotation is supported here
/// from the first line of code, because a rotation that is ignored produces
/// geometry that is silently in the wrong place -- which is worse than
/// geometry that fails to load. Whether a given authoring path accepts a
/// rotated source is a separate question, recorded in the capability matrix.
struct RasterGeoTransform {
    double a0 = 0.0, a1 = 1.0, a2 = 0.0;
    double b0 = 0.0, b1 = 0.0, b2 = 1.0;

    /// Build the common north-up case from a GeoTIFF ModelPixelScale and the
    /// raster-space/model-space point pair of a ModelTiepoint.
    ///
    /// `pixelSizeY` is the tag's value, which is positive and unsigned; the
    /// downward row direction is implicit in the tag and explicit in `b2`.
    /// Negating it here rather than at the call site is deliberate: it is the
    /// single place the convention is applied.
    static RasterGeoTransform FromPixelScaleAndTiepoint(
        double pixelSizeX, double pixelSizeY, double tiepointPixelX,
        double tiepointPixelY, double tiepointSourceX, double tiepointSourceY);

    /// Take the affine terms of a GeoTIFF ModelTransformation 4x4, given in
    /// row-major order.
    static RasterGeoTransform FromMatrix(const double matrix[16]);

    /// Pixel space -> source coordinates.
    void Apply(const PixelCoord& pixel, double& sourceX, double& sourceY) const;

    /// Integer pixel index -> source coordinates, applying `anchor`.
    ///
    /// This is the function the whole coordinate chain funnels through, and
    /// the reason `PixelAnchor::Unknown` is a value rather than an absence: a
    /// caller that has not resolved the anchoring cannot call this and get a
    /// plausible answer. It returns false for `Unknown` instead.
    bool TryPixelToSource(std::uint64_t i, std::uint64_t j, PixelAnchor anchor,
                          double& sourceX, double& sourceY) const;

    /// The pixel-space coordinate of sample `(i, j)` under `anchor`.
    ///
    ///     Area   px = i + 0.5, py = j + 0.5
    ///     Point  px = i,       py = j
    static PixelCoord GetPixelCoord(std::uint64_t i, std::uint64_t j,
                                    PixelAnchor anchor);

    /// The determinant of the linear part. Negative means the map flips
    /// handedness, which the authoring library compensates for when it winds
    /// faces -- from this value rather than from an assumption about north-up
    /// rasters. Zero means the transform is not invertible.
    double GetDeterminant() const;

    /// True when the transform can be inverted. A degenerate transform -- zero
    /// pixel size, or a collapsed axis -- is a malformed source, and window
    /// planning needs the inverse, so this is checked rather than assumed.
    bool IsInvertible() const;

    /// Source coordinates -> pixel space. False when the transform is not
    /// invertible.
    bool TryInverse(double sourceX, double sourceY, PixelCoord& pixel) const;

    /// Source coordinates -> the integer pixel containing them, under
    /// `anchor`. False when the transform is not invertible, the anchoring is
    /// unknown, or the position falls outside `size`.
    bool TrySourceToPixel(double sourceX, double sourceY, PixelAnchor anchor,
                          const RasterSize& size, std::uint64_t& i,
                          std::uint64_t& j) const;

    bool IsNorthUp() const;
    bool IsRotated() const;

    /// True when Y increases with the row index, i.e. the raster is stored
    /// bottom-to-top in source space. Legal, handled, and recorded.
    bool IsSouthUp() const;

    /// Pixel width and height in source units, from the transform's column
    /// lengths. For a rotated transform these are the true sample spacings,
    /// which is not the same as `a1` and `b2`.
    double GetPixelWidth() const;
    double GetPixelHeight() const;
};

bool operator==(const RasterGeoTransform& left, const RasterGeoTransform& right);

/// The source-coordinate extent of a window under a transform and anchoring.
///
/// Under `Area` the extent is the union of the cell areas, so it spans
/// `width` cells. Under `Point` it spans the outermost sample positions, which
/// is `width - 1` intervals. The two differ by a pixel, and reporting the
/// wrong one is a bounds error that survives every visual check. See section 2
/// of the coordinate contract.
///
/// All four corners are transformed, not just two, because a rotated
/// transform's extent is not the image of the corner pair.
///
/// The Z range is left at the empty extent: elevation comes from band values,
/// not from the transform.
bool TryGetWindowBounds(const RasterGeoTransform& transform,
                        const RasterWindow& window, PixelAnchor anchor,
                        usdgeo::GeoBounds& bounds);

}  // namespace usdraster

#endif  // USDRASTER_RASTERGEOTRANSFORM_H
