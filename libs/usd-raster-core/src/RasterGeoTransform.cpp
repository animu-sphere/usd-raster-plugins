#include "usdraster/RasterGeoTransform.h"

#include <cmath>

namespace usdraster {

const char* GetPixelAnchorName(PixelAnchor anchor) {
    switch (anchor) {
        case PixelAnchor::Area: return "area";
        case PixelAnchor::Point: return "point";
        case PixelAnchor::Unknown: return "unknown";
    }
    return "unknown";
}

RasterGeoTransform RasterGeoTransform::FromPixelScaleAndTiepoint(
    double pixelSizeX, double pixelSizeY, double tiepointPixelX,
    double tiepointPixelY, double tiepointSourceX, double tiepointSourceY) {
    RasterGeoTransform transform;
    transform.a1 = pixelSizeX;
    transform.a2 = 0.0;
    transform.b1 = 0.0;
    // The tag stores an unsigned magnitude; the downward row direction is
    // implicit there and explicit here. This is the one place the sign
    // convention is applied.
    transform.b2 = -pixelSizeY;
    transform.a0 = tiepointSourceX - transform.a1 * tiepointPixelX;
    transform.b0 = tiepointSourceY - transform.b2 * tiepointPixelY;
    return transform;
}

RasterGeoTransform RasterGeoTransform::FromMatrix(const double matrix[16]) {
    // Row-major 4x4. Row 0 gives X, row 1 gives Y; the pixel-space z term
    // (columns 2) is dropped because a raster's pixel space is planar.
    RasterGeoTransform transform;
    transform.a1 = matrix[0];
    transform.a2 = matrix[1];
    transform.a0 = matrix[3];
    transform.b1 = matrix[4];
    transform.b2 = matrix[5];
    transform.b0 = matrix[7];
    return transform;
}

void RasterGeoTransform::Apply(const PixelCoord& pixel, double& sourceX,
                               double& sourceY) const {
    sourceX = a0 + a1 * pixel.px + a2 * pixel.py;
    sourceY = b0 + b1 * pixel.px + b2 * pixel.py;
}

PixelCoord RasterGeoTransform::GetPixelCoord(std::uint64_t i, std::uint64_t j,
                                             PixelAnchor anchor) {
    const double px = static_cast<double>(i);
    const double py = static_cast<double>(j);
    if (anchor == PixelAnchor::Area) {
        return PixelCoord{px + 0.5, py + 0.5};
    }
    return PixelCoord{px, py};
}

bool RasterGeoTransform::TryPixelToSource(std::uint64_t i, std::uint64_t j,
                                          PixelAnchor anchor, double& sourceX,
                                          double& sourceY) const {
    if (anchor == PixelAnchor::Unknown) {
        return false;
    }
    Apply(GetPixelCoord(i, j, anchor), sourceX, sourceY);
    return true;
}

double RasterGeoTransform::GetDeterminant() const {
    return a1 * b2 - a2 * b1;
}

bool RasterGeoTransform::IsInvertible() const {
    const double determinant = GetDeterminant();
    if (!std::isfinite(determinant) || determinant == 0.0) {
        return false;
    }
    // A determinant that is finite but denormal-small means an axis has
    // effectively collapsed, and inverting it produces coordinates that are
    // arithmetically defined and physically meaningless. Rejecting it here is
    // what turns a malformed source into a diagnostic instead of into geometry
    // several astronomical units across.
    const double scale = std::fabs(a1) + std::fabs(a2) + std::fabs(b1) +
                         std::fabs(b2);
    if (!std::isfinite(scale) || scale == 0.0) {
        return false;
    }
    return std::fabs(determinant) > scale * scale * 1e-15;
}

bool RasterGeoTransform::TryInverse(double sourceX, double sourceY,
                                    PixelCoord& pixel) const {
    if (!IsInvertible()) {
        return false;
    }
    const double determinant = GetDeterminant();
    const double dx = sourceX - a0;
    const double dy = sourceY - b0;
    // The 2x2 inverse, applied directly rather than through a north-up special
    // case. Window planning and tile mapping both need world-to-pixel, and a
    // north-up shortcut here would place every rotated source wrongly.
    pixel.px = (b2 * dx - a2 * dy) / determinant;
    pixel.py = (a1 * dy - b1 * dx) / determinant;
    return true;
}

bool RasterGeoTransform::TrySourceToPixel(double sourceX, double sourceY,
                                          PixelAnchor anchor,
                                          const RasterSize& size,
                                          std::uint64_t& i,
                                          std::uint64_t& j) const {
    if (anchor == PixelAnchor::Unknown || size.IsEmpty()) {
        return false;
    }
    PixelCoord pixel;
    if (!TryInverse(sourceX, sourceY, pixel)) {
        return false;
    }
    // Undo the anchoring offset so that both conventions land on the index
    // whose sample position is nearest, rather than one convention being
    // consistently half a pixel off.
    double indexX = pixel.px;
    double indexY = pixel.py;
    if (anchor == PixelAnchor::Area) {
        indexX -= 0.5;
        indexY -= 0.5;
    }
    const double roundedX = std::floor(indexX + 0.5);
    const double roundedY = std::floor(indexY + 0.5);
    if (!std::isfinite(roundedX) || !std::isfinite(roundedY) ||
        roundedX < 0.0 || roundedY < 0.0 ||
        roundedX >= static_cast<double>(size.width) ||
        roundedY >= static_cast<double>(size.height)) {
        return false;
    }
    i = static_cast<std::uint64_t>(roundedX);
    j = static_cast<std::uint64_t>(roundedY);
    return true;
}

bool RasterGeoTransform::IsRotated() const {
    return a2 != 0.0 || b1 != 0.0;
}

bool RasterGeoTransform::IsNorthUp() const {
    return !IsRotated() && a1 > 0.0 && b2 < 0.0;
}

bool RasterGeoTransform::IsSouthUp() const {
    return !IsRotated() && b2 > 0.0;
}

double RasterGeoTransform::GetPixelWidth() const {
    // The column length, not `a1`. For a rotated transform the two differ, and
    // `a1` alone would understate the sample spacing by the cosine of the
    // rotation.
    return std::sqrt(a1 * a1 + b1 * b1);
}

double RasterGeoTransform::GetPixelHeight() const {
    return std::sqrt(a2 * a2 + b2 * b2);
}

bool operator==(const RasterGeoTransform& left,
                const RasterGeoTransform& right) {
    return left.a0 == right.a0 && left.a1 == right.a1 && left.a2 == right.a2 &&
           left.b0 == right.b0 && left.b1 == right.b1 && left.b2 == right.b2;
}

bool TryGetWindowBounds(const RasterGeoTransform& transform,
                        const RasterWindow& window, PixelAnchor anchor,
                        usdgeo::GeoBounds& bounds) {
    if (anchor == PixelAnchor::Unknown || window.IsEmpty()) {
        return false;
    }

    // The pixel-space rectangle the extent spans, which is what differs
    // between the conventions:
    //
    //   Area   the union of cell areas: corner (x, y) to (x+w, y+h).
    //   Point  the outermost sample positions: (x, y) to (x+w-1, y+h-1),
    //          which is w-1 intervals, not w.
    double left = static_cast<double>(window.x);
    double top = static_cast<double>(window.y);
    double right;
    double bottom;
    if (anchor == PixelAnchor::Area) {
        right = static_cast<double>(window.GetEndX());
        bottom = static_cast<double>(window.GetEndY());
    } else {
        right = static_cast<double>(window.GetEndX() - 1);
        bottom = static_cast<double>(window.GetEndY() - 1);
    }

    // All four corners. For a rotated transform the extent is not the image of
    // one corner pair, and taking only two would understate it.
    const PixelCoord corners[4] = {
        {left, top}, {right, top}, {right, bottom}, {left, bottom}};

    // Z comes out as the degenerate range at zero. Elevation is a band value,
    // not a property of the transform, so a caller that needs a vertical
    // extent expands this with the values it read.
    bounds = usdgeo::GeoBounds::Empty();
    for (const PixelCoord& corner : corners) {
        double sourceX = 0.0;
        double sourceY = 0.0;
        transform.Apply(corner, sourceX, sourceY);
        bounds.Expand(usdgeo::Vec3d{sourceX, sourceY, 0.0});
    }
    // A non-finite transform coefficient leaves every corner rejected, and the
    // extent invalid. That is a malformed source, and reporting it is the
    // caller's next step.
    return bounds.IsValid();
}

}  // namespace usdraster
