#pragma once

#include <usdraster/RasterGeoTransform.h>

#include <cstdint>
#include <map>
#include <string>

namespace usd_raster_geotiff {

struct RasterArguments {
    std::string representation = "metadata";
    std::uint32_t band = 1;
    usdraster::PixelAnchor pixelAnchor = usdraster::PixelAnchor::Unknown;
};

bool ParseRasterArguments(
    const std::map<std::string, std::string>& values,
    RasterArguments& result, usdgeo::DiagnosticSink& diagnostics);

}  // namespace usd_raster_geotiff