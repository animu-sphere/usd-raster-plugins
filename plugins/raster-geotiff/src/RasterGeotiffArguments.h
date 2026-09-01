#pragma once

#include <usdraster/NoData.h>
#include <usdraster/RasterGeoTransform.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace usd_raster_geotiff {

struct RasterArguments {
    std::string representation = "metadata";
    std::uint32_t band = 1;
    usdraster::PixelAnchor pixelAnchor = usdraster::PixelAnchor::Unknown;
    double heightScale = 1.0;
    usdraster::NoDataPolicy noDataPolicy = usdraster::NoDataPolicy::Skip;
    std::optional<double> fillValue;
    bool heightScaleSpecified = false;
    bool noDataPolicySpecified = false;
    bool fillValueSpecified = false;
};

bool ParseRasterArguments(
    const std::map<std::string, std::string>& values,
    RasterArguments& result, usdgeo::DiagnosticSink& diagnostics);

}  // namespace usd_raster_geotiff