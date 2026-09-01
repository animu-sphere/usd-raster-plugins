#include "usdgeotiff/GeoTiffReader.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

namespace {
void Check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", what);
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

void CheckReadFailure(const char* fixture,
                                const usdraster::RasterWindow& window,
                                const usdraster::RasterReadOptions& options,
                                usdgeo::DiagnosticCode code,
                                usdgeo::DiagnosticSink& diagnostics) {
      auto bytes = ReadFile(std::string(FIXTURE_DIR) + "/" + fixture);
      usdraster::MemorySource source(bytes.data(), bytes.size(), fixture);
      usdgeotiff::GeoTiffReader reader(source);
      usdraster::RasterGrid grid;
      Check(!reader.ReadWindow(window, options, &grid, &diagnostics), fixture);
      Check(HasCode(diagnostics, code), "typed pixel-read diagnostic");
}

void Put16(std::vector<unsigned char>& bytes, std::size_t offset,
               std::uint16_t value) {
      bytes[offset] = static_cast<unsigned char>(value);
      bytes[offset + 1] = static_cast<unsigned char>(value >> 8);
}

void Put64(std::vector<unsigned char>& bytes, std::size_t offset,
               std::uint64_t value) {
      for (std::size_t index = 0; index < 8; ++index)
            bytes[offset + index] = static_cast<unsigned char>(value >> (index * 8));
}

std::vector<unsigned char> BuildOverflowFixture() {
      constexpr std::size_t ifdOffset = 16;
      constexpr std::size_t entryCount = 9;
      constexpr std::size_t entrySize = 20;
      constexpr std::size_t pixelOffset = ifdOffset + 8 + entryCount * entrySize + 8;
      std::vector<unsigned char> bytes(pixelOffset + 1, 0);
      bytes[0] = 'I'; bytes[1] = 'I';
      Put16(bytes, 2, 43); Put16(bytes, 4, 8); Put16(bytes, 6, 0);
      Put64(bytes, 8, ifdOffset);
      Put64(bytes, ifdOffset, entryCount);

      std::size_t entry = ifdOffset + 8;
      auto add = [&](std::uint16_t tag, std::uint16_t type,
                           std::uint64_t value) {
            Put16(bytes, entry, tag); Put16(bytes, entry + 2, type);
            Put64(bytes, entry + 4, 1); Put64(bytes, entry + 12, value);
            entry += entrySize;
      };
      add(256, 16, std::numeric_limits<std::uint64_t>::max());
      add(257, 16, 2);
      add(258, 3, 8);
      add(259, 3, 1);
      add(273, 16, pixelOffset);
      add(277, 3, 1);
      add(278, 16, 2);
      add(279, 16, 1);
      add(339, 3, 1);
      bytes[pixelOffset] = 7;
      return bytes;
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

    diagnostics.Clear();
    options.outputType = usdraster::RasterDataType::Float32;
    ReadWindow("geotiff-2x2-float64-geographic.tif", {0, 0, 2, 2}, options,
               grid, 32, diagnostics);
    Check(HasCode(diagnostics, usdgeo::DiagnosticCode::LossyConversion),
          "lossy output conversion warning");

    diagnostics.Clear();
    options.outputType = usdraster::RasterDataType::Float32;
    ReadWindow("geotiff-2x2-uint16-scaled-metadata.tif", {0, 0, 2, 2},
               options, grid, 8, diagnostics);
    Check(grid.GetSample(0, 0) == 105.0 && grid.GetSample(1, 1) == 120.0,
          "band scale and offset are applied");

    diagnostics.Clear();
    options.memoryBudgetBytes = 103;
    CheckReadFailure("geotiff-8x8-uint16-striped.tif", {2, 1, 3, 3}, options,
                     usdgeo::DiagnosticCode::MemoryBudgetExceeded, diagnostics);
    diagnostics.Clear();
    options.memoryBudgetBytes = 104;
    ReadWindow("geotiff-8x8-uint16-striped.tif", {2, 1, 3, 3}, options,
               grid, 128, diagnostics);
    Check(!diagnostics.HasError(), "selected segment fits memory budget");

    diagnostics.Clear();
    options.memoryBudgetBytes = 0;
    CheckReadFailure("geotiff-8x8-uint16-striped.tif", {8, 0, 1, 1}, options,
                     usdgeo::DiagnosticCode::WindowOutOfBounds, diagnostics);

    diagnostics.Clear();
    options.isCancelled = [] { return true; };
    CheckReadFailure("geotiff-8x8-uint16-striped.tif", {0, 0, 1, 1}, options,
                     usdgeo::DiagnosticCode::Cancelled, diagnostics);
    options.isCancelled = {};

    auto overflowBytes = BuildOverflowFixture();
    usdraster::MemorySource overflowSource(overflowBytes.data(),
                                           overflowBytes.size(), "overflow");
    usdgeotiff::GeoTiffReader overflowReader(overflowSource);
    usdraster::RasterGrid overflowGrid;
    diagnostics.Clear();
    Check(!overflowReader.ReadWindow({1, 1, 1, 1}, options, &overflowGrid,
                                     &diagnostics), "overflow fixture fails");
    Check(HasCode(diagnostics, usdgeo::DiagnosticCode::InconsistentTileLayout),
          "overflow has typed diagnostic");
    return 0;
}
