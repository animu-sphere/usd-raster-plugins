#include "usdgeo/TileId.h"

#include "usdgeo/CacheKey.h"

namespace usdgeo {

std::string TileId::ToString() const {
    // Hand-built rather than through a stream. `std::ostringstream` obeys the
    // global locale, and a locale that groups digits would turn L0_X1000_Y0
    // into L0_X1,000_Y0 -- a generated filename that differs by host.
    std::string out = "L";
    out += std::to_string(level);
    out += "_X";
    out += std::to_string(x);
    out += "_Y";
    out += std::to_string(y);
    return out;
}

std::uint64_t TileId::Hash() const {
    CacheKey key;
    key.AddUInt(level);
    key.AddUInt(x);
    key.AddUInt(y);
    return key.GetDigest();
}

bool operator==(const TileId& left, const TileId& right) {
    return left.level == right.level && left.x == right.x && left.y == right.y;
}

bool operator!=(const TileId& left, const TileId& right) {
    return !(left == right);
}

bool operator<(const TileId& left, const TileId& right) {
    if (left.level != right.level) return left.level < right.level;
    if (left.y != right.y) return left.y < right.y;
    return left.x < right.x;
}

}  // namespace usdgeo
