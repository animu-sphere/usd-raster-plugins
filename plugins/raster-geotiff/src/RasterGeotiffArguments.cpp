#include "RasterGeotiffArguments.h"

#include <charconv>
#include <cmath>
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

bool ParseFiniteDouble(const std::string& value, double& result) {
    if (value.empty()) return false;
    const char* first = value.data();
    const char* last = first + value.size();
    const auto parsed = std::from_chars(first, last, result,
                                        std::chars_format::general);
    return parsed.ec == std::errc() && parsed.ptr == last &&
           std::isfinite(result);
}

}  // namespace

bool ParseRasterArguments(
    const std::map<std::string, std::string>& values,
    RasterArguments& result, usdgeo::DiagnosticSink& diagnostics) {
    result = RasterArguments{};
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
        } else if (argument.first == "heightScale") {
            if (!ParseFiniteDouble(argument.second, result.heightScale) ||
                result.heightScale <= 0.0) {
                diagnostics.AddError(usdgeo::DiagnosticCode::InvalidFormatArgument,
                                     "heightScale must be a finite number greater than zero");
                return false;
            }
            result.heightScaleSpecified = true;
        } else if (argument.first == "nodata") {
            if (argument.second == "skip") {
                result.noDataPolicy = usdraster::NoDataPolicy::Skip;
            } else if (argument.second == "fill") {
                result.noDataPolicy = usdraster::NoDataPolicy::Fill;
            } else if (argument.second == "keep") {
                result.noDataPolicy = usdraster::NoDataPolicy::Keep;
            } else {
                diagnostics.AddError(usdgeo::DiagnosticCode::InvalidFormatArgument,
                                     "nodata must be skip, fill, or keep");
                return false;
            }
            result.noDataPolicySpecified = true;
        } else if (argument.first == "fillValue") {
            double fillValue = 0.0;
            if (!ParseFiniteDouble(argument.second, fillValue)) {
                diagnostics.AddError(usdgeo::DiagnosticCode::InvalidFormatArgument,
                                     "fillValue must be a finite number");
                return false;
            }
            result.fillValue = fillValue;
            result.fillValueSpecified = true;
        } else {
            diagnostics.AddError(usdgeo::DiagnosticCode::UnknownFormatArgument,
                                 "unknown file-format argument: " + argument.first);
            return false;
        }
    }
    if (result.representation != "mesh" &&
        (result.heightScaleSpecified || result.noDataPolicySpecified ||
         result.fillValueSpecified)) {
        diagnostics.AddError(usdgeo::DiagnosticCode::UnsupportedFormatArgument,
                             "heightScale, nodata, and fillValue require representation=mesh");
        return false;
    }
    if (result.fillValue.has_value() &&
        result.noDataPolicy != usdraster::NoDataPolicy::Fill) {
        diagnostics.AddError(usdgeo::DiagnosticCode::ConflictingFormatArguments,
                             "fillValue requires nodata=fill");
        return false;
    }
    if (result.noDataPolicy == usdraster::NoDataPolicy::Fill &&
        !result.fillValue.has_value()) {
        diagnostics.AddError(usdgeo::DiagnosticCode::InvalidFormatArgument,
                             "nodata=fill requires fillValue");
        return false;
    }
    return true;
}

}  // namespace usd_raster_geotiff