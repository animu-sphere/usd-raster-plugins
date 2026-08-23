#include "usdraster/RasterGrid.h"

#include <limits>

namespace usdraster {

RasterGrid::RasterGrid(const RasterWindow& window, std::uint64_t samplingStep,
                       std::uint32_t band, RasterDataType sourceType,
                       const NoDataValue& noData)
    : _window(window),
      _samplingStep(samplingStep == 0 ? 1 : samplingStep),
      _band(band),
      _sourceType(sourceType),
      _noData(noData) {
    const RasterSize requested = GetSampledSize(_window, _samplingStep);
    const std::uint64_t count = requested.GetPixelCount();

    // `GetPixelCount` saturates so that a malformed size stays diagnosable
    // rather than wrapping to a small number that allocates successfully.
    // Handing that saturated value to the allocator would throw
    // `std::length_error` and undo the whole point of it, so the oversized
    // case yields an empty grid instead: `IsEmpty()` is true, `GetSize()` is
    // zero, and `GetWindow()` still names the region the caller asked for so
    // the diagnostic can quote it.
    //
    // The size is zeroed rather than kept, because `GetSample` bounds-checks
    // against `_size` -- a preserved size with no buffer behind it would turn
    // every read into an out-of-bounds index.
    constexpr std::uint64_t kMaxCount =
        static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) /
        sizeof(double);
    if (count > kMaxCount || count > _samples.max_size()) {
        return;
    }

    _size = requested;
    _samples.assign(static_cast<std::size_t>(count), 0.0);
}

double RasterGrid::GetSample(std::uint64_t column, std::uint64_t row) const {
    if (column >= _size.width || row >= _size.height) {
        return 0.0;
    }
    return _samples[static_cast<std::size_t>(row * _size.width + column)];
}

void RasterGrid::SetSample(std::uint64_t column, std::uint64_t row,
                           double value) {
    if (column >= _size.width || row >= _size.height) {
        return;
    }
    _samples[static_cast<std::size_t>(row * _size.width + column)] = value;
}

bool RasterGrid::IsNoData(std::uint64_t column, std::uint64_t row) const {
    if (column >= _size.width || row >= _size.height) {
        return false;
    }
    return _noData.Matches(GetSample(column, row));
}

void RasterGrid::GetSourcePixel(std::uint64_t column, std::uint64_t row,
                                std::uint64_t& i, std::uint64_t& j) const {
    // Sample (0, 0) is the window's first pixel under every step, which is the
    // registration property the coordinate contract requires.
    i = _window.x + column * _samplingStep;
    j = _window.y + row * _samplingStep;
}

}  // namespace usdraster
