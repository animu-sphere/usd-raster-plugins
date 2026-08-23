#ifndef USDRASTER_RASTERWINDOW_H
#define USDRASTER_RASTERWINDOW_H

#include "usdraster/RasterTypes.h"

#include <usdgeo/Diagnostic.h>

#include <cstdint>
#include <vector>

namespace usdraster {

/// A rectangle of source pixels.
///
/// **Always in full-resolution source pixel coordinates**, regardless of which
/// overview level ultimately serves the read. A caller asking for
/// `(1000, 1000)`-`(1512, 1512)` gets that region whether an overview serves
/// it or not, and never rescales its own request when overview selection
/// changes. Putting the rescaling in the caller would spread the same
/// arithmetic across every consumer, and each copy would be a place to get it
/// wrong. See section 1 of docs/architecture/RASTER_READER.md.
struct RasterWindow {
    std::uint64_t x = 0;       ///< leftmost column
    std::uint64_t y = 0;       ///< topmost row
    std::uint64_t width = 0;
    std::uint64_t height = 0;

    static RasterWindow FromSize(const RasterSize& size) {
        return RasterWindow{0, 0, size.width, size.height};
    }

    bool IsEmpty() const { return width == 0 || height == 0; }

    /// One past the last column. Exclusive, so an empty window has
    /// `GetEndX() == x` rather than a negative extent that unsigned
    /// arithmetic cannot express.
    ///
    /// Saturating, like `RasterSize::GetPixelCount`: an origin and extent that
    /// together exceed the range wrap to a *small* end coordinate, which would
    /// make a window silently appear to lie before its own origin and turn
    /// every intersection and clip against it into a wrong answer rather than
    /// a rejected one.
    std::uint64_t GetEndX() const;
    std::uint64_t GetEndY() const;

    RasterSize GetSize() const { return RasterSize{width, height}; }

    /// Saturating, like `RasterSize::GetPixelCount`.
    std::uint64_t GetPixelCount() const { return GetSize().GetPixelCount(); }

    bool Contains(std::uint64_t px, std::uint64_t py) const;
    bool Contains(const RasterWindow& other) const;

    /// The overlap of two windows, empty when they do not overlap. The result
    /// is anchored at the overlap's own origin, not at either input's.
    RasterWindow Intersect(const RasterWindow& other) const;

    /// This window clipped to a raster's extent. A window that starts past the
    /// extent clips to empty rather than wrapping, which is the failure
    /// unsigned subtraction produces if the clip is written the obvious way.
    RasterWindow ClipTo(const RasterSize& size) const;

    /// Subdivide into a grid of tiles of at most `tileWidth` x `tileHeight`.
    ///
    /// The subdivision covers this window exactly: no gap, no overlap, and
    /// edge tiles are truncated rather than padded. Sizes that do not divide
    /// evenly are the normal case, not the exception -- a raster whose width
    /// is a multiple of its tile width is the coincidence.
    ///
    /// Tiles come back in row-major order, which is the order that reads the
    /// source most nearly sequentially.
    std::vector<RasterWindow> Subdivide(std::uint64_t tileWidth,
                                        std::uint64_t tileHeight) const;

    /// This window expressed in the coordinates of an overview downsampled by
    /// `factor`.
    ///
    /// The result is the smallest overview-level window that covers this one:
    /// the origin rounds down and the far edge rounds up. Rounding the extent
    /// down instead would silently drop the last row and column of a window
    /// whose size is not a multiple of the factor -- a one-pixel seam between
    /// adjacent tiles, which is the kind of defect that only shows up once
    /// tiling exists.
    RasterWindow ToOverview(std::uint64_t factor) const;

    /// The full-resolution window an overview-level window covers. The inverse
    /// direction of `ToOverview`, and not its exact inverse: the round trip
    /// widens, because an overview pixel covers `factor` full-resolution ones.
    RasterWindow FromOverview(std::uint64_t factor) const;

    /// The diagnostic anchor form. `usdGeoCore` cannot depend on this library,
    /// so the anchor is its own plain rectangle; see `usdgeo::PixelWindow`.
    usdgeo::PixelWindow ToAnchor() const;
};

bool operator==(const RasterWindow& left, const RasterWindow& right);
bool operator!=(const RasterWindow& left, const RasterWindow& right);

/// The number of samples a decimated read produces along one axis.
///
/// `ceil(extent / step)`, computed without floating point so that a large
/// extent cannot land on the wrong side of a rounding boundary. A step never
/// re-anchors the grid: sample 0 is always the window's first pixel, which is
/// what keeps a coarse mesh registered with a fine one. See section 8 of
/// docs/architecture/COORDINATE_MODEL.md.
std::uint64_t GetSampledExtent(std::uint64_t extent, std::uint64_t step);

/// The sampled size of a window under a decimation step.
RasterSize GetSampledSize(const RasterWindow& window, std::uint64_t step);

}  // namespace usdraster

#endif  // USDRASTER_RASTERWINDOW_H
