#include "usdraster/RasterMetadata.h"

namespace usdraster {

double RasterBandInfo::ApplyScaleAndOffset(double rawSample) const {
    double value = rawSample;
    if (scale.has_value()) {
        value *= *scale;
    }
    if (offset.has_value()) {
        value += *offset;
    }
    return value;
}

const RasterBandInfo* RasterMetadata::FindBand(std::uint32_t index) const {
    for (const RasterBandInfo& band : bands) {
        if (band.index == index) {
            return &band;
        }
    }
    return nullptr;
}

std::uint64_t RasterMetadata::GetOverviewFactor(std::uint32_t level) const {
    if (level >= overviewSizes.size() || size.width == 0) {
        return 0;
    }
    const RasterSize& overview = overviewSizes[level];
    if (overview.width == 0) {
        return 0;
    }
    // Rounded rather than truncated. An overview of a 4001-pixel image is 2001
    // pixels wide at factor 2, and 4001 / 2001 truncates to 1 -- which would
    // report the half-resolution level as full resolution and read the wrong
    // pixels for every window.
    return (size.width + overview.width / 2) / overview.width;
}

}  // namespace usdraster
