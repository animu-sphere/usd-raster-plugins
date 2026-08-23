#include "usdgeo/CacheKey.h"

#include <cmath>
#include <cstring>

namespace usdgeo {

namespace {

// Lane 1 is textbook FNV-1a/64. Lane 2 uses a different offset basis and a
// different odd multiplier -- the 64-bit golden-ratio constant from
// splitmix64 -- so the two lanes do not move together, which two lanes sharing
// a multiplier would.
constexpr std::uint64_t kFnvOffsetLow = 0xcbf29ce484222325ull;
constexpr std::uint64_t kFnvPrimeLow = 0x100000001b3ull;
constexpr std::uint64_t kFnvOffsetHigh = 0x84222325cbf29ce4ull;
constexpr std::uint64_t kFnvPrimeHigh = 0x9e3779b97f4a7c15ull;

/// Serialize an unsigned value little-endian regardless of host endianness.
/// Reinterpreting the object representation would make the digest differ
/// between a little-endian and a big-endian host, which is exactly the
/// portability property this class promises.
void PackLittleEndian(std::uint64_t value, std::uint8_t out[8]) {
    for (int i = 0; i < 8; ++i) {
        out[i] = static_cast<std::uint8_t>((value >> (8 * i)) & 0xffu);
    }
}

std::uint64_t DoubleToBits(double value) {
    static_assert(sizeof(double) == sizeof(std::uint64_t),
                  "the digest assumes an IEEE-754 binary64 double");
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

}  // namespace

CacheKey::CacheKey() : _low(kFnvOffsetLow), _high(kFnvOffsetHigh) {}

void CacheKey::Mix(std::uint8_t byte) {
    _low = (_low ^ byte) * kFnvPrimeLow;
    _high = (_high ^ byte) * kFnvPrimeHigh;
}

void CacheKey::MixTag(char tag) {
    Mix(static_cast<std::uint8_t>(tag));
}

void CacheKey::MixBytes(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        Mix(bytes[i]);
    }
}

void CacheKey::AddString(const std::string& value) {
    MixTag('s');
    std::uint8_t packed[8];
    PackLittleEndian(static_cast<std::uint64_t>(value.size()), packed);
    MixBytes(packed, sizeof(packed));
    MixBytes(value.data(), value.size());
}

void CacheKey::AddUInt(std::uint64_t value) {
    MixTag('u');
    std::uint8_t packed[8];
    PackLittleEndian(value, packed);
    MixBytes(packed, sizeof(packed));
}

void CacheKey::AddInt(std::int64_t value) {
    MixTag('i');
    // Two's-complement reinterpretation, which is what C++20 mandates and what
    // every platform this builds on already does. The tag keeps it distinct
    // from the unsigned lane, so -1 and 0xffffffffffffffff do not collide.
    std::uint8_t packed[8];
    PackLittleEndian(static_cast<std::uint64_t>(value), packed);
    MixBytes(packed, sizeof(packed));
}

void CacheKey::AddBool(bool value) {
    MixTag('b');
    Mix(value ? 1u : 0u);
}

void CacheKey::AddDouble(double value) {
    MixTag('d');
    std::uint64_t bits;
    if (std::isnan(value)) {
        // The canonical positive quiet NaN, written as its bits rather than
        // taken from quiet_NaN(): the sign bit and payload of a NaN are not
        // fixed by the standard, and a digest that varied with them would key
        // two identical requests to two entries.
        bits = 0x7ff8000000000000ull;
    } else if (value == 0.0) {
        // Negative zero compares equal to positive zero, so two requests no
        // consumer can tell apart must not key to two entries either.
        bits = 0;
    } else {
        bits = DoubleToBits(value);
    }
    std::uint8_t packed[8];
    PackLittleEndian(bits, packed);
    MixBytes(packed, sizeof(packed));
}

void CacheKey::AddNull() {
    MixTag('n');
}

std::string CacheKey::GetHexDigest() const {
    static const char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(32);
    const std::uint64_t lanes[2] = {_high, _low};
    for (std::uint64_t lane : lanes) {
        for (int shift = 60; shift >= 0; shift -= 4) {
            out.push_back(kHex[(lane >> shift) & 0xfu]);
        }
    }
    return out;
}

}  // namespace usdgeo
