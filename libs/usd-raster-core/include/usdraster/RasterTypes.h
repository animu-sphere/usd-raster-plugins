// usdRasterCore -- the format-independent raster value model.
//
// No header in this library may include an OpenUSD, TIFF/GeoTIFF, libtiff, or
// transport header. See section 2 of docs/architecture/WORKSPACE.md.

#ifndef USDRASTER_RASTERTYPES_H
#define USDRASTER_RASTERTYPES_H

#include <cstdint>

namespace usdraster {

/// A raster extent in pixels.
struct RasterSize {
    std::uint64_t width = 0;
    std::uint64_t height = 0;

    bool IsEmpty() const { return width == 0 || height == 0; }

    /// Pixel count, or 0 when the size is empty. Saturates rather than
    /// overflowing: a malformed header claiming 2^40 by 2^40 must produce a
    /// diagnosable size, not a small wrapped number that then allocates
    /// successfully.
    std::uint64_t GetPixelCount() const;
};

inline bool operator==(const RasterSize& left, const RasterSize& right) {
    return left.width == right.width && left.height == right.height;
}

inline bool operator!=(const RasterSize& left, const RasterSize& right) {
    return !(left == right);
}

/// Sample formats this repository decodes.
///
/// Sub-byte formats (1, 2, and 4 bits) are deliberately absent. An unused
/// enumerator is a contract nothing tests, and they enter when a format needs
/// them; see the outstanding questions in
/// docs/roadmap/phase-1-raster-core.md.
enum class RasterDataType {
    UInt8,
    Int8,
    UInt16,
    Int16,
    UInt32,
    Int32,
    Float32,
    Float64
};

/// Bytes per sample.
std::uint32_t GetDataTypeSize(RasterDataType type);

const char* GetDataTypeName(RasterDataType type);

bool IsFloatingPoint(RasterDataType type);
bool IsSigned(RasterDataType type);

/// Whether every value of `from` is exactly representable in `to`.
///
/// Used to decide whether a conversion is lossy and therefore needs a
/// `LossyConversion` diagnostic and a metadata record. Invariant 8 of the
/// workspace contract requires lossy steps to be recorded, and this is what
/// tells the reader that a step was one.
///
/// Note `Int32` -> `Float32` is lossy: a 24-bit significand cannot hold every
/// 32-bit integer, which is the case a "float is wider, so it is safe" reading
/// gets wrong.
bool IsExactlyRepresentable(RasterDataType from, RasterDataType to);

}  // namespace usdraster

#endif  // USDRASTER_RASTERTYPES_H
