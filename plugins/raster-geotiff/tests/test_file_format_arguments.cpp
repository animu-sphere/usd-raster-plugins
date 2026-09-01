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
              {{"band", "02"}, {"representation", "mesh"}},
              arguments, diagnostics),
          "valid arguments parse");
    Check(arguments.band == 2 && arguments.representation == "mesh",
          "band is normalized to a numeric value");

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
    Check(!usd_raster_geotiff::ParseRasterArguments(
              {{"unknown", "value"}}, arguments, diagnostics),
          "unknown argument is rejected");
    Check(HasCode(diagnostics, usdgeo::DiagnosticCode::UnknownFormatArgument),
          "unknown argument diagnostic");
    return 0;
}