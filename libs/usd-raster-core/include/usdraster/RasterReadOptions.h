#ifndef USDRASTER_RASTERREADOPTIONS_H
#define USDRASTER_RASTERREADOPTIONS_H

#include "usdraster/RasterTypes.h"

#include <usdgeo/CacheKey.h>

#include <cstdint>
#include <functional>
#include <optional>

namespace usdraster {

/// I/O work performed for one raster read.
struct RasterReadStatistics {
    /// Logical source bytes represented by the requested window.
    std::uint64_t requestedBytes = 0;
    /// Bytes requested from RandomAccessSource, including layout overhead.
    std::uint64_t fetchedBytes = 0;
    /// Number of RandomAccessSource read calls issued for pixel data.
    std::uint64_t requestCount = 0;

    double GetAmplificationRatio() const {
        return requestedBytes == 0
            ? 0.0
            : static_cast<double>(fetchedBytes) /
                  static_cast<double>(requestedBytes);
    }

    void Reset() { *this = RasterReadStatistics{}; }
};

/// What a caller asks of a windowed read.
struct RasterReadOptions {
    /// 1-based. One band per call: a multi-band read is a separate call, not a
    /// hidden fan-out, so the memory a read costs stays predictable from its
    /// arguments.
    std::uint32_t band = 1;

    /// Empty lets the reader pick the coarsest level that still satisfies
    /// `samplingStep` without losing detail the caller asked for. An explicit
    /// level disables that choice, and exists for tests and for the converter,
    /// where reproducibility matters more than the reader's judgement.
    std::optional<std::uint32_t> overviewLevel;

    /// Decimation in source pixels. Never re-anchors the grid.
    std::uint64_t samplingStep = 1;

    /// The type the caller wants back, for reporting and for deciding whether
    /// the conversion is lossy. `RasterGrid` itself holds `double`.
    RasterDataType outputType = RasterDataType::Float32;

    /// Zero means caller-managed. Non-zero bounds decode buffers: a request
    /// that cannot be satisfied within the budget fails with a diagnostic
    /// naming the size it needed, and does not silently allocate more. A
    /// budget that could be exceeded "just this once" is not a budget.
    std::uint64_t memoryBudgetBytes = 0;

    /// Checked at window and tile boundaries. A true result stops the read
    /// with `Cancelled` and releases partial buffers. Cancellation is a
    /// distinct outcome from failure, because a host needs to tell a user who
    /// navigated away from a file that is broken.
    std::function<bool()> isCancelled;

    /// Optional output for I/O counters. This does not affect cache identity.
    RasterReadStatistics* statistics = nullptr;

    bool IsCancelled() const { return isCancelled && isCancelled(); }

    /// Fold the options that change decoded values into a cache key.
    ///
    /// `memoryBudgetBytes` and `isCancelled` are deliberately excluded: they
    /// change how a read is scheduled, not what it produces, and including
    /// them would key two identical results to two cache entries. See section
    /// 3 of docs/architecture/RASTER_READER.md.
    void ContributeToCacheKey(usdgeo::CacheKey& key) const;
};

}  // namespace usdraster

#endif  // USDRASTER_RASTERREADOPTIONS_H
