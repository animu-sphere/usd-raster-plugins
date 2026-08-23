#include "usdgeotiff/GeoTiffReader.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
void Check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", what);
        std::abort();
    }
}

std::vector<unsigned char> ReadFile(const std::string& path) {
    std::FILE* file = std::fopen(path.c_str(), "rb"); Check(file != nullptr, path.c_str());
    std::fseek(file, 0, SEEK_END); const long length = std::ftell(file);
    Check(length > 0, "fixture has bytes"); std::rewind(file);
    std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
    Check(std::fread(bytes.data(), 1, bytes.size(), file) == bytes.size(), "read fixture");
    std::fclose(file); return bytes;
}

bool HasCode(const usdgeo::DiagnosticSink& diagnostics,
             usdgeo::DiagnosticCode code) {
    for (const auto& diagnostic : diagnostics.GetDiagnostics())
        if (diagnostic.code == code) return true;
    return false;
}

usdraster::RasterMetadata ReadMetadata(const char* fixture,
                                       usdgeo::DiagnosticSink& diagnostics,
                                       std::uint64_t& bytesRead) {
    auto bytes = ReadFile(std::string(FIXTURE_DIR) + "/" + fixture);
    usdraster::MemorySource source(bytes.data(), bytes.size(), fixture);
    usdraster::RecordingSource recording(source); usdgeotiff::GeoTiffReader reader(recording);
    usdraster::RasterMetadata metadata;
    Check(reader.ReadMetadata(&metadata, &diagnostics), fixture);
    bytesRead = recording.GetBytesRead();
    Check(bytesRead < bytes.size(), "metadata does not read pixel segments");
    return metadata;
}
}
int main() {
    usdgeo::DiagnosticSink diagnostics;
    std::uint64_t bytesRead = 0;
    auto metadata = ReadMetadata("geotiff-2x2-float32-le.tif", diagnostics, bytesRead);
    Check(metadata.size.width == 2 && metadata.size.height == 2, "dimensions");
    Check(metadata.bands.size() == 1 && metadata.bands[0].dataType == usdraster::RasterDataType::Float32, "float32 band");
    Check(metadata.pixelAnchor == usdraster::PixelAnchor::Area, "pixel-is-area");
    Check(metadata.hasGeoTransform && metadata.geoTransform.a0 == 300000.0, "north-up transform");
    Check(metadata.crs.epsgCode == 32654 && metadata.crs.linearUnit == usdgeo::LinearUnit::Metre, "projected CRS");

    diagnostics.Clear();
    ReadMetadata("geotiff-2x2-float32-be.tif", diagnostics, bytesRead);
    diagnostics.Clear();
    ReadMetadata("geotiff-2x2-float32-bigtiff.tif", diagnostics, bytesRead);

    diagnostics.Clear();
    metadata = ReadMetadata("geotiff-2x2-float32-pixelispoint.tif", diagnostics, bytesRead);
    Check(metadata.pixelAnchor == usdraster::PixelAnchor::Point, "pixel-is-point");
    diagnostics.Clear();
    metadata = ReadMetadata("geotiff-2x2-float32-rotated.tif", diagnostics, bytesRead);
    Check(metadata.geoTransform.IsRotated(), "rotated transform");
    diagnostics.Clear();
    ReadMetadata("geotiff-2x2-float32-conflicting-geo.tif", diagnostics, bytesRead);
    Check(HasCode(diagnostics, usdgeo::DiagnosticCode::ConflictingGeoTransform), "conflicting transform warning");
    diagnostics.Clear();
    metadata = ReadMetadata("geotiff-2x2-float32-no-geo.tif", diagnostics, bytesRead);
    Check(!metadata.hasGeoTransform && metadata.crs.IsEmpty(), "plain TIFF has no georeferencing");
    diagnostics.Clear();
    metadata = ReadMetadata("geotiff-2x2-float32-no-raster-type.tif", diagnostics, bytesRead);
    Check(metadata.pixelAnchor == usdraster::PixelAnchor::Unknown, "unknown pixel anchor preserved");
    Check(HasCode(diagnostics, usdgeo::DiagnosticCode::UnknownPixelAnchor), "unknown pixel anchor warning");
    diagnostics.Clear();
    metadata = ReadMetadata("geotiff-2x2-float32-nodata.tif", diagnostics, bytesRead);
    Check(metadata.bands[0].noData.Matches(-9999.0), "numeric nodata");
    diagnostics.Clear();
    metadata = ReadMetadata("geotiff-2x2-float32-nodata-nan.tif", diagnostics, bytesRead);
    Check(metadata.bands[0].noData.IsSet() && std::isnan(*metadata.bands[0].noData.Get()), "NaN nodata");
    diagnostics.Clear();
    metadata = ReadMetadata("geotiff-2x2-float64-geographic.tif", diagnostics, bytesRead);
    Check(metadata.bands[0].dataType == usdraster::RasterDataType::Float64, "float64 band");
    Check(metadata.crs.kind == usdgeo::CrsKind::Geographic && metadata.crs.epsgCode == 4326, "geographic CRS");
    diagnostics.Clear();
    metadata = ReadMetadata("geotiff-8x8-uint16-striped.tif", diagnostics, bytesRead);
    Check(metadata.size.width == 8 && metadata.bands[0].dataType == usdraster::RasterDataType::UInt16 && !metadata.IsTiled(), "strip layout");
    diagnostics.Clear();
    metadata = ReadMetadata("geotiff-32x32-uint16-tiled.tif", diagnostics, bytesRead);
    Check(metadata.IsTiled() && metadata.nativeTileSize->width == 16 && metadata.nativeTileSize->height == 16, "tile layout");
    diagnostics.Clear();
    metadata = ReadMetadata("geotiff-20x20-uint16-tiled-partial.tif", diagnostics, bytesRead);
    Check(metadata.size.width == 20 && metadata.size.height == 20 && metadata.IsTiled(), "partial tile layout");
    return 0;
}