#include "usdraster/RasterTypes.h"

#include <limits>

namespace usdraster {

std::uint64_t RasterSize::GetPixelCount() const {
    if (IsEmpty()) {
        return 0;
    }
    constexpr std::uint64_t kMax = std::numeric_limits<std::uint64_t>::max();
    if (width > kMax / height) {
        return kMax;
    }
    return width * height;
}

std::uint32_t GetDataTypeSize(RasterDataType type) {
    switch (type) {
        case RasterDataType::UInt8:
        case RasterDataType::Int8: return 1;
        case RasterDataType::UInt16:
        case RasterDataType::Int16: return 2;
        case RasterDataType::UInt32:
        case RasterDataType::Int32:
        case RasterDataType::Float32: return 4;
        case RasterDataType::Float64: return 8;
    }
    return 0;
}

const char* GetDataTypeName(RasterDataType type) {
    switch (type) {
        case RasterDataType::UInt8: return "uint8";
        case RasterDataType::Int8: return "int8";
        case RasterDataType::UInt16: return "uint16";
        case RasterDataType::Int16: return "int16";
        case RasterDataType::UInt32: return "uint32";
        case RasterDataType::Int32: return "int32";
        case RasterDataType::Float32: return "float32";
        case RasterDataType::Float64: return "float64";
    }
    return "";
}

bool IsFloatingPoint(RasterDataType type) {
    return type == RasterDataType::Float32 || type == RasterDataType::Float64;
}

bool IsSigned(RasterDataType type) {
    switch (type) {
        case RasterDataType::Int8:
        case RasterDataType::Int16:
        case RasterDataType::Int32:
        case RasterDataType::Float32:
        case RasterDataType::Float64: return true;
        default: return false;
    }
}

bool IsExactlyRepresentable(RasterDataType from, RasterDataType to) {
    if (from == to) {
        return true;
    }
    if (to == RasterDataType::Float64) {
        // Every integer this enumeration carries is at most 32 bits, and a
        // binary64 significand holds 53, so all of them fit. Float32 fits too.
        return true;
    }
    if (to == RasterDataType::Float32) {
        // 24 significand bits. Integers up to 2^24 are exact; a 32-bit integer
        // type is not, whether signed or unsigned.
        switch (from) {
            case RasterDataType::UInt8:
            case RasterDataType::Int8:
            case RasterDataType::UInt16:
            case RasterDataType::Int16: return true;
            default: return false;
        }
    }
    if (IsFloatingPoint(from)) {
        // A float cannot land exactly in any integer type: fractional values
        // and out-of-range magnitudes both exist.
        return false;
    }

    // Integer to integer. Exact when the destination covers the source range.
    const bool fromSigned = IsSigned(from);
    const bool toSigned = IsSigned(to);
    const std::uint32_t fromBits = GetDataTypeSize(from) * 8;
    const std::uint32_t toBits = GetDataTypeSize(to) * 8;

    if (fromSigned == toSigned) {
        return toBits >= fromBits;
    }
    if (fromSigned) {
        // A signed source has negatives an unsigned destination cannot hold.
        return false;
    }
    // Unsigned into signed: the destination loses one bit to the sign.
    return toBits > fromBits;
}

}  // namespace usdraster
