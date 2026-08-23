#include "usdgeotiff/GeoTiffReader.h"

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
    std::FILE* file = std::fopen(path.c_str(), "rb");
    Check(file != nullptr, path.c_str());
    std::fseek(file, 0, SEEK_END);
    const long length = std::ftell(file);
    Check(length > 0, "fixture has bytes");
    std::rewind(file);
    std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
    Check(std::fread(bytes.data(), 1, bytes.size(), file) == bytes.size(),
          "read fixture");
    std::fclose(file);
    return bytes;
}

std::vector<std::size_t> ReadWindow(const char* fixture,
                                    const usdraster::RasterWindow& window,
                                    const usdraster::RasterReadOptions& options,
                                    usdraster::RasterGrid& grid,
                                    std::size_t pixelBytes,
                                    usdgeo::DiagnosticSink& diagnostics) {
    auto bytes = ReadFile(std::string(FIXTURE_DIR) + "/" + fixture);
    usdraster::MemorySource source(bytes.data(), bytes.size(), fixture);
    usdraster::RecordingSource recording(source);
    usdgeotiff::GeoTiffReader reader(recording);
    Check(reader.ReadWindow(window, options, &grid, &diagnostics), fixture);

    const std::uint64_t pixelOffset = bytes.size() - pixelBytes;
    std::vector<std::size_t> pixelRanges;
    for (const auto& range : recording.GetRanges()) {
        if (range.offset >= pixelOffset)
            pixelRanges.push_back(range.size);
    }
    return pixelRanges;
}
}  // namespace

int main() {
    usdgeo::DiagnosticSink diagnostics;
    usdraster::RasterGrid grid;
    usdraster::RasterReadOptions options;

    auto stripRanges = ReadWindow("geotiff-8x8-uint16-striped.tif",
                                  {2, 1, 3, 3}, options, grid, 128,
                                  diagnostics);
    Check(grid.GetSize() == (usdraster::RasterSize{3, 3}),
          "strip window size");
    Check(grid.GetSample(0, 0) == 10.0 && grid.GetSample(2, 2) == 28.0,
          "strip window values");
    Check(stripRanges.size() == 2 && stripRanges[0] == 32 &&
              stripRanges[1] == 32,
          "strip window reads intersecting strips");

    diagnostics.Clear();
    auto tileRanges = ReadWindow("geotiff-32x32-uint16-tiled.tif",
                                 {2, 3, 4, 4}, options, grid, 2048,
                                 diagnostics);
    Check(grid.GetSize() == (usdraster::RasterSize{4, 4}) &&
              grid.GetSample(0, 0) == 98.0 &&
              grid.GetSample(3, 3) == 197.0,
          "tile window values");
    Check(tileRanges.size() == 1 && tileRanges[0] == 512,
          "tile window reads one tile");

    diagnostics.Clear();
    auto partialRanges = ReadWindow("geotiff-20x20-uint16-tiled-partial.tif",
                                    {16, 16, 4, 4}, options, grid, 2048,
                                    diagnostics);
    Check(grid.GetSample(0, 0) == 336.0 &&
              grid.GetSample(3, 3) == 399.0,
          "partial tile drops padding");
    Check(partialRanges.size() == 1 && partialRanges[0] == 512,
          "partial window reads one tile");

    diagnostics.Clear();
    options.samplingStep = 2;
    ReadWindow("geotiff-8x8-uint16-striped.tif", {1, 1, 5, 5}, options,
               grid, 128, diagnostics);
    Check(grid.GetSize() == (usdraster::RasterSize{3, 3}) &&
              grid.GetSample(0, 0) == 9.0 &&
              grid.GetSample(2, 2) == 45.0,
          "sampled window values");

    diagnostics.Clear();
    options.samplingStep = 1;
    ReadWindow("geotiff-2x2-float32-be.tif", {0, 0, 2, 2}, options,
               grid, 16, diagnostics);
    Check(grid.GetSample(0, 0) == 10.0 && grid.GetSample(1, 1) == 40.0,
          "big-endian pixel values");
    return 0;
}
