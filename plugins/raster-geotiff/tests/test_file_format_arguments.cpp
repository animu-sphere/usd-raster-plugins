#include "RasterGeotiffArguments.h"

#include <cstdio>
#include <cstdlib>

namespace {

void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", message);
        std::abort();
    }
}

bool HasCode(const usdgeo::DiagnosticSink& diagnostics,
            usdgeo::DiagnosticCode code) {
    for (const auto& diagnostic : diagnostics.GetDiagnostics()) {
        if (diagnostic.code == code) return true;
    }
    return false;
}

}  // namespace

int main() {
    usdgeo::DiagnosticSink diagnostics;
    usd_raster_geotiff::RasterArguments arguments;

    Check(usd_raster_geotiff::ParseRasterArguments(
              {{"band", "02"}, {"fillValue", "-1.5"},
               {"heightScale", "2.0"}, {"nodata", "fill"},
               {"representation", "mesh"}},
              arguments, diagnostics),
          "valid arguments parse");
        Check(arguments.band == 2 && arguments.representation == "mesh" &&
              arguments.heightScale == 2.0 &&
              arguments.noDataPolicy == usdraster::NoDataPolicy::Fill &&
              arguments.fillValue.has_value() && *arguments.fillValue == -1.5 &&
              arguments.heightScaleSpecified && arguments.noDataPolicySpecified &&
              arguments.fillValueSpecified,
            "mesh arguments are normalized");

    diagnostics.Clear();
    arguments = {};
    Check(!usd_raster_geotiff::ParseRasterArguments(
              {{"band", "0"}}, arguments, diagnostics),
          "zero band is rejected");
    Check(HasCode(diagnostics, usdgeo::DiagnosticCode::InvalidFormatArgument),
          "invalid band diagnostic");

    diagnostics.Clear();
    Check(!usd_raster_geotiff::ParseRasterArguments(
              {{"band", "1.0"}}, arguments, diagnostics),
          "non-integer band is rejected");
    Check(HasCode(diagnostics, usdgeo::DiagnosticCode::InvalidFormatArgument),
          "strict band diagnostic");

        diagnostics.Clear();
        arguments = {};
        Check(!usd_raster_geotiff::ParseRasterArguments(
              {{"heightScale", "0"}, {"representation", "mesh"}},
              arguments, diagnostics),
            "non-positive height scale is rejected");
        Check(HasCode(diagnostics, usdgeo::DiagnosticCode::InvalidFormatArgument),
            "height scale diagnostic");

        diagnostics.Clear();
        arguments = {};
        Check(!usd_raster_geotiff::ParseRasterArguments(
              {{"fillValue", "0"}, {"representation", "mesh"}},
              arguments, diagnostics),
            "fill value without policy is rejected");
        Check(HasCode(diagnostics, usdgeo::DiagnosticCode::ConflictingFormatArguments),
            "fill value conflict diagnostic");

        diagnostics.Clear();
        arguments = {};
        Check(!usd_raster_geotiff::ParseRasterArguments(
              {{"nodata", "fill"}, {"representation", "mesh"}},
              arguments, diagnostics),
            "fill policy without value is rejected");
        Check(HasCode(diagnostics, usdgeo::DiagnosticCode::InvalidFormatArgument),
            "missing fill value diagnostic");

        diagnostics.Clear();
        arguments = {};
        Check(!usd_raster_geotiff::ParseRasterArguments(
              {{"heightScale", "2"}}, arguments, diagnostics),
            "mesh-only argument on metadata is rejected");
        Check(HasCode(diagnostics, usdgeo::DiagnosticCode::UnsupportedFormatArgument),
            "metadata mesh argument diagnostic");

    diagnostics.Clear();
    Check(!usd_raster_geotiff::ParseRasterArguments(
              {{"unknown", "value"}}, arguments, diagnostics),
          "unknown argument is rejected");
    Check(HasCode(diagnostics, usdgeo::DiagnosticCode::UnknownFormatArgument),
          "unknown argument diagnostic");
    return 0;
}