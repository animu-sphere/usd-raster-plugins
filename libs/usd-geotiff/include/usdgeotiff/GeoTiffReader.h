#ifndef USDGEOTIFF_GEOTIFFREADER_H
#define USDGEOTIFF_GEOTIFFREADER_H

#include <usdraster/RandomAccessSource.h>
#include <usdraster/RasterGrid.h>
#include <usdraster/RasterMetadata.h>
#include <usdraster/RasterReadOptions.h>

#include <usdgeo/Diagnostic.h>

#include <cstdint>

namespace usdgeotiff {

/// Coordinates one native TIFF tile or strip. This is deliberately separate
/// from usdgeo::TileId, which identifies a USD spatial tile.
struct SourceTileId {
    std::uint64_t x = 0;
    std::uint64_t y = 0;
};

class GeoTiffReader {
public:
    explicit GeoTiffReader(usdraster::RandomAccessSource& source);

    /// Reads TIFF structure and metadata only. Pixel segments are never read.
    bool ReadMetadata(usdraster::RasterMetadata* metadata,
                      usdgeo::DiagnosticSink* diagnostics) const;

    /// Reads only the source segments that intersect `window`.
    bool ReadWindow(const usdraster::RasterWindow& window,
                    const usdraster::RasterReadOptions& options,
                    usdraster::RasterGrid* grid,
                    usdgeo::DiagnosticSink* diagnostics) const;

    /// Reads one native TIFF tile or strip. The x/y coordinates are segment
    /// coordinates, not USD spatial-tile coordinates.
    bool ReadTile(const SourceTileId& tileId,
                  const usdraster::RasterReadOptions& options,
                  usdraster::RasterGrid* grid,
                  usdgeo::DiagnosticSink* diagnostics) const;

    /// Reads a contiguous run of full-width source rows.
    bool ReadScanlines(std::uint64_t firstRow, std::uint64_t rowCount,
                       const usdraster::RasterReadOptions& options,
                       usdraster::RasterGrid* grid,
                       usdgeo::DiagnosticSink* diagnostics) const;

private:
    usdraster::RandomAccessSource& _source;
};

}  // namespace usdgeotiff

#endif  // USDGEOTIFF_GEOTIFFREADER_H