#ifndef USDGEO_TILEID_H
#define USDGEO_TILEID_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace usdgeo {

/// Deterministic identity for one spatial tile.
///
/// Anchored at source pixel `(0, 0)`, not at the extent's corner and not at
/// the window a particular conversion happened to request. That anchoring is
/// what makes an id reproducible: convert a sub-window today and the whole
/// raster tomorrow, and the tiles that overlap carry the same ids, so a
/// generated cache entry from the first run is still valid for the second.
///
/// `level` is the tiling level, 0 being the finest. It is spatial partitioning
/// depth, not sampling density -- those stay separate concepts, which is
/// invariant 9 of docs/architecture/WORKSPACE.md.
struct TileId {
    std::uint32_t level = 0;
    std::uint64_t x = 0;  ///< tile column at `level`
    std::uint64_t y = 0;  ///< tile row at `level`

    /// The stable spelling, e.g. `L0_X3_Y7`. Used for prim names, tile asset
    /// filenames, and manifest entries, so it is fixed rather than incidental:
    /// changing it changes every generated path.
    std::string ToString() const;

    /// A stable hash for use in unordered containers. Not a cache key -- the
    /// cache key is built by `CacheKey` from normalized inputs, and this is a
    /// container detail that may not outlive the process.
    std::uint64_t Hash() const;
};

bool operator==(const TileId& left, const TileId& right);
bool operator!=(const TileId& left, const TileId& right);

/// Total order: level, then row, then column. Row-major within a level, so
/// that iterating an ordered set of tiles walks them in the order a raster is
/// stored, which is the order that reads the source most nearly sequentially.
bool operator<(const TileId& left, const TileId& right);

}  // namespace usdgeo

namespace std {
template <>
struct hash<usdgeo::TileId> {
    std::size_t operator()(const usdgeo::TileId& id) const noexcept {
        return static_cast<std::size_t>(id.Hash());
    }
};
}  // namespace std

#endif  // USDGEO_TILEID_H
