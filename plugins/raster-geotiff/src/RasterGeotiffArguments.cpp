#include "RasterGeotiffArguments.h"

#include <charconv>
#include <limits>

namespace usd_raster_geotiff {
namespace {

bool ParseBand(const std::string& value, std::uint32_t& band) {
    std::uint64_t parsed = 0;
    const char* first = value.data();
    const char* last = first + value.size();
    const auto parsedResult = std::from_chars(first, last, parsed, 10);
    if (parsedResult.ec != std::errc() || parsedResult.ptr != last ||
        parsed == 0 || parsed > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    band = static_cast<std::uint32_t>(parsed);
    return true;
}

}  // namespace

bool ParseRasterArguments(
    const std::map<std::string, std::string>& values,
    RasterArguments& result, usdgeo::DiagnosticSink& diagnostics) {
    for (const auto& argument : values) {
        if (argument.first == "representation") {
            result.representation = argument.second;
            if (result.representation != "metadata" &&
                result.representation != "mesh") {
                diagnostics.AddError(usdgeo::DiagnosticCode::UnsupportedFormatArgument,
                                     "representation must be metadata or mesh");
                return false;
            }
        } else if (argument.first == "band") {
            if (!ParseBand(argument.second, result.band)) {
                diagnostics.AddError(usdgeo::DiagnosticCode::InvalidFormatArgument,
                                     "band must be a positive integer");
                return false;
            }
        } else if (argument.first == "pixelAnchor") {
            if (argument.second == "area") {
                result.pixelAnchor = usdraster::PixelAnchor::Area;
            } else if (argument.second == "point") {
                result.pixelAnchor = usdraster::PixelAnchor::Point;
            } else {
                diagnostics.AddError(usdgeo::DiagnosticCode::InvalidFormatArgument,
                                     "pixelAnchor must be area or point");
                return false;
            }
        } else {
            diagnostics.AddError(usdgeo::DiagnosticCode::UnknownFormatArgument,
                                 "unknown file-format argument: " + argument.first);
            return false;
        }
    }
    return true;
}

}  // namespace usd_raster_geotiff