#ifndef USDRASTER_RASTERGRID_H
#define USDRASTER_RASTERGRID_H

#include "usdraster/NoData.h"
#include "usdraster/RasterTypes.h"
#include "usdraster/RasterWindow.h"

#include <cstdint>
#include <vector>

namespace usdraster {

/// The decoded result of one window read.
///
/// `RasterGrid` **owns** its buffer. The alternative -- a view over a
/// caller-provided buffer -- was considered and deferred: owning is what makes
/// the lifetime rule statable in one sentence, and a non-owning view is added
/// only if a measured copy cost justifies it. See the outstanding questions in
/// docs/roadmap/phase-1-raster-core.md.
///
/// Samples are `double` regardless of the source's sample format. A raster
/// grid is what the authoring library consumes, not what the decoder produces
/// internally: the decoder works in the native type and converts once, here,
/// so that every consumer downstream has one type to handle rather than eight.
/// The memory cost is bounded by the window, which is the point of the window
/// being the unit of transfer.
///
/// Coordinates: `window` is in **full-resolution source pixels**, and
/// `samplingStep` is the decimation applied within it. Sample `(0, 0)` is
/// always the window's first pixel -- a step never re-anchors the grid, which
/// is what keeps a coarse grid registered with a fine one.
class RasterGrid {
public:
    RasterGrid() = default;

    /// Allocate for `window` under `step`, zero-filled. The stored size is the
    /// sampled size, not the window size, and they differ whenever the step is
    /// greater than one.
    ///
    /// A request whose sampled extent cannot be allocated -- which is what a
    /// malformed source size produces, since `RasterSize::GetPixelCount`
    /// saturates rather than wrapping -- yields an **empty** grid: `IsEmpty()`
    /// is true and `GetSize()` is zero. It does not throw. `GetWindow()` still
    /// returns the region that was asked for, so a caller can name it in a
    /// diagnostic.
    RasterGrid(const RasterWindow& window, std::uint64_t samplingStep,
               std::uint32_t band, RasterDataType sourceType,
               const NoDataValue& noData);

    const RasterWindow& GetWindow() const { return _window; }
    std::uint64_t GetSamplingStep() const { return _samplingStep; }

    /// The sampled extent: `ceil(window.width / step)` by
    /// `ceil(window.height / step)`. This is the buffer's shape, and it is
    /// what a mesh's vertex grid is built from.
    const RasterSize& GetSize() const { return _size; }

    std::uint32_t GetBand() const { return _band; }

    /// The source's sample format, kept for reporting. The buffer is `double`
    /// whatever this says.
    RasterDataType GetSourceType() const { return _sourceType; }

    const NoDataValue& GetNoData() const { return _noData; }

    bool IsEmpty() const { return _samples.empty(); }
    std::size_t GetSampleCount() const { return _samples.size(); }

    /// Row-major, `size.width` samples per row. The buffer's lifetime is this
    /// object's; a caller that needs it to outlive the grid copies it.
    const std::vector<double>& GetSamples() const { return _samples; }
    std::vector<double>& GetSamples() { return _samples; }

    /// Sample at sampled-grid coordinates, **not** source pixel coordinates.
    /// Out of range returns 0.0 rather than reading past the buffer.
    double GetSample(std::uint64_t column, std::uint64_t row) const;
    void SetSample(std::uint64_t column, std::uint64_t row, double value);

    /// True when the sample at these sampled-grid coordinates is NoData.
    bool IsNoData(std::uint64_t column, std::uint64_t row) const;

    /// The full-resolution source pixel a sampled-grid coordinate came from.
    /// This is the function that keeps the two coordinate spaces from being
    /// confused at a call site.
    void GetSourcePixel(std::uint64_t column, std::uint64_t row,
                        std::uint64_t& i, std::uint64_t& j) const;

private:
    RasterWindow _window;
    std::uint64_t _samplingStep = 1;
    RasterSize _size;
    std::uint32_t _band = 1;
    RasterDataType _sourceType = RasterDataType::Float32;
    NoDataValue _noData;
    std::vector<double> _samples;
};

}  // namespace usdraster

#endif  // USDRASTER_RASTERGRID_H
