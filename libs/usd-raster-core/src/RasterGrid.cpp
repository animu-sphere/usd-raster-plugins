#include "usdraster/RasterGrid.h"

namespace usdraster {

RasterGrid::RasterGrid(const RasterWindow& window, std::uint64_t samplingStep,
                       std::uint32_t band, RasterDataType sourceType,
                       const NoDataValue& noData)
    : _window(window),
      _samplingStep(samplingStep == 0 ? 1 : samplingStep),
      _band(band),
      _sourceType(sourceType),
      _noData(noData) {
    _size = GetSampledSize(_window, _samplingStep);
    _samples.assign(static_cast<std::size_t>(_size.GetPixelCount()), 0.0);
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
