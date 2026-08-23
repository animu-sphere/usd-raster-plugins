#ifndef USDRASTER_RASTERMETADATA_H
#define USDRASTER_RASTERMETADATA_H

#include "usdraster/NoData.h"
#include "usdraster/RasterGeoTransform.h"
#include "usdraster/RasterTypes.h"

#include <usdgeo/CrsDescription.h>
#include <usdgeo/GeoBounds.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace usdraster {

/// What a source says about one band.
struct RasterBandInfo {
    /// 1-based, matching the GeoTIFF and GDAL convention. Deliberately not
    /// 0-based: every external tool a user cross-checks against counts from
    /// one, and an off-by-one in a band selector reads the wrong data
    /// silently.
    std::uint32_t index = 1;
    RasterDataType dataType = RasterDataType::Float32;
    NoDataValue noData;

    /// The source's own scale and offset for the band, applied as
    /// `scale * raw + offset` through `double`. Distinct from the display-side
    /// `heightScale`, which is applied on top and is a lossy choice the caller
    /// makes; this pair is part of what the value means.
    std::optional<double> scale;
    std::optional<double> offset;

    std::string description;
    std::string unit;

    /// The band's stated value, given a raw sample. Applies scale and offset
    /// when present; returns the sample unchanged when they are not.
    double ApplyScaleAndOffset(double rawSample) const;
};

/// Everything readable without decoding a pixel.
///
/// That property is what makes `representation=metadata` cheap and what makes
/// inspecting a multi-gigabyte remote DEM practical: a header and a few IFDs
/// rather than a download. See section 2 of
/// docs/architecture/RASTER_READER.md.
struct RasterMetadata {
    RasterSize size;
    std::vector<RasterBandInfo> bands;
    RasterGeoTransform geoTransform;

    /// `Unknown` until the source states it. A reader that had to guess
    /// reports the anchoring as unknown and lets the caller supply it; it does
    /// not default.
    PixelAnchor pixelAnchor = PixelAnchor::Unknown;

    /// True when the source carried usable georeferencing at all. False means
    /// `geoTransform` is the identity placeholder, not a transform that was
    /// read -- the two are indistinguishable from the coefficients alone, and
    /// a plain non-geospatial TIFF is an ordinary case, not an exotic one.
    bool hasGeoTransform = false;

    usdgeo::CrsDescription crs;

    /// The native tile size, empty when the source is strip-organized. The
    /// distinction drives read planning: a window read from a tiled source
    /// touches the intersecting tiles, and from a striped source it touches
    /// whole rows.
    std::optional<RasterSize> nativeTileSize;

    /// Reduced-resolution levels, finest first, excluding the full-resolution
    /// image itself. Recorded from milestone 2 and used from milestone 9.
    std::vector<RasterSize> overviewSizes;

    /// The source-coordinate extent, under the anchoring in force. Z is the
    /// degenerate range at zero until band values are read.
    usdgeo::GeoBounds bounds;

    /// Anything the source carried that this model has no field for, kept as
    /// read. Unknown metadata is preserved or reported, never silently
    /// discarded; that is invariant 7 of the workspace contract, and this is
    /// where the "preserved" half of it lives.
    std::vector<std::pair<std::string, std::string>> extra;

    const RasterBandInfo* FindBand(std::uint32_t index) const;

    std::uint32_t GetBandCount() const {
        return static_cast<std::uint32_t>(bands.size());
    }

    bool IsTiled() const { return nativeTileSize.has_value(); }

    /// The downsampling factor of overview `level`, relative to full
    /// resolution. Level 0 is the first entry of `overviewSizes`. Zero when
    /// the level does not exist.
    std::uint64_t GetOverviewFactor(std::uint32_t level) const;
};

}  // namespace usdraster

#endif  // USDRASTER_RASTERMETADATA_H
