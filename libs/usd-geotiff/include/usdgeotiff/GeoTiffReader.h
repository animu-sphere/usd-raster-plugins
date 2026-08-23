#ifndef USDGEOTIFF_GEOTIFFREADER_H
#define USDGEOTIFF_GEOTIFFREADER_H

#include <usdraster/RandomAccessSource.h>
#include <usdraster/RasterGrid.h>
#include <usdraster/RasterMetadata.h>
#include <usdraster/RasterReadOptions.h>

#include <usdgeo/Diagnostic.h>

namespace usdgeotiff {

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

private:
    usdraster::RandomAccessSource& _source;
};

}  // namespace usdgeotiff

#endif  // USDGEOTIFF_GEOTIFFREADER_H