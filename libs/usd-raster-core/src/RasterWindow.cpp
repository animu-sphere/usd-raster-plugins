#include "usdraster/RasterWindow.h"

#include <algorithm>

namespace usdraster {

bool RasterWindow::Contains(std::uint64_t px, std::uint64_t py) const {
    if (IsEmpty()) {
        return false;
    }
    return px >= x && px < GetEndX() && py >= y && py < GetEndY();
}

bool RasterWindow::Contains(const RasterWindow& other) const {
    if (other.IsEmpty()) {
        return true;
    }
    if (IsEmpty()) {
        return false;
    }
    return other.x >= x && other.y >= y && other.GetEndX() <= GetEndX() &&
           other.GetEndY() <= GetEndY();
}

RasterWindow RasterWindow::Intersect(const RasterWindow& other) const {
    const std::uint64_t left = std::max(x, other.x);
    const std::uint64_t top = std::max(y, other.y);
    const std::uint64_t right = std::min(GetEndX(), other.GetEndX());
    const std::uint64_t bottom = std::min(GetEndY(), other.GetEndY());

    // Compared before subtracting. `right - left` on disjoint windows would
    // wrap to an enormous unsigned width rather than producing an empty
    // rectangle, and the wrapped value allocates successfully.
    if (right <= left || bottom <= top) {
        return RasterWindow{};
    }
    return RasterWindow{left, top, right - left, bottom - top};
}

RasterWindow RasterWindow::ClipTo(const RasterSize& size) const {
    return Intersect(RasterWindow::FromSize(size));
}

std::vector<RasterWindow> RasterWindow::Subdivide(
    std::uint64_t tileWidth, std::uint64_t tileHeight) const {
    std::vector<RasterWindow> tiles;
    if (IsEmpty() || tileWidth == 0 || tileHeight == 0) {
        return tiles;
    }

    const std::uint64_t across = (width + tileWidth - 1) / tileWidth;
    const std::uint64_t down = (height + tileHeight - 1) / tileHeight;
    tiles.reserve(static_cast<std::size_t>(
        RasterSize{across, down}.GetPixelCount()));

    for (std::uint64_t row = 0; row < down; ++row) {
        const std::uint64_t top = y + row * tileHeight;
        // Truncated, not padded: the last row and column of tiles are shorter
        // when the window does not divide evenly, and together they cover the
        // window exactly.
        const std::uint64_t tall = std::min(tileHeight, GetEndY() - top);
        for (std::uint64_t column = 0; column < across; ++column) {
            const std::uint64_t left = x + column * tileWidth;
            const std::uint64_t wide = std::min(tileWidth, GetEndX() - left);
            tiles.push_back(RasterWindow{left, top, wide, tall});
        }
    }
    return tiles;
}

RasterWindow RasterWindow::ToOverview(std::uint64_t factor) const {
    if (factor <= 1) {
        return *this;
    }
    if (IsEmpty()) {
        return RasterWindow{};
    }
    const std::uint64_t left = x / factor;
    const std::uint64_t top = y / factor;
    const std::uint64_t right = (GetEndX() + factor - 1) / factor;
    const std::uint64_t bottom = (GetEndY() + factor - 1) / factor;
    return RasterWindow{left, top, right - left, bottom - top};
}

RasterWindow RasterWindow::FromOverview(std::uint64_t factor) const {
    if (factor <= 1 || IsEmpty()) {
        return *this;
    }
    return RasterWindow{x * factor, y * factor, width * factor,
                        height * factor};
}

usdgeo::PixelWindow RasterWindow::ToAnchor() const {
    return usdgeo::PixelWindow{x, y, width, height};
}

bool operator==(const RasterWindow& left, const RasterWindow& right) {
    // An empty window has no position worth comparing: a read of nothing at
    // (0,0) and a read of nothing at (500,500) are the same request, and
    // treating them as different would make an intersection result depend on
    // which pair of disjoint windows produced it.
    if (left.IsEmpty() && right.IsEmpty()) {
        return true;
    }
    return left.x == right.x && left.y == right.y &&
           left.width == right.width && left.height == right.height;
}

bool operator!=(const RasterWindow& left, const RasterWindow& right) {
    return !(left == right);
}

std::uint64_t GetSampledExtent(std::uint64_t extent, std::uint64_t step) {
    if (extent == 0) {
        return 0;
    }
    if (step <= 1) {
        return extent;
    }
    return (extent + step - 1) / step;
}

RasterSize GetSampledSize(const RasterWindow& window, std::uint64_t step) {
    return RasterSize{GetSampledExtent(window.width, step),
                      GetSampledExtent(window.height, step)};
}

}  // namespace usdraster
