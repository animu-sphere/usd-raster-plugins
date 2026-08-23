// usdRasterCore unit tests.
//
// The expected values here are hand-computed from the formulas in
// docs/architecture/COORDINATE_MODEL.md, not captured from a previous run of
// this code. A golden value produced by the implementation under test only
// proves the implementation is stable, not that it is right.

#include "usdraster/NoData.h"
#include "usdraster/RandomAccessSource.h"
#include "usdraster/RasterGeoTransform.h"
#include "usdraster/RasterGrid.h"
#include "usdraster/RasterMetadata.h"
#include "usdraster/RasterReadOptions.h"
#include "usdraster/RasterTypes.h"
#include "usdraster/RasterWindow.h"

#include <usdgeo/CacheKey.h>
#include <usdgeo/LocalOrigin.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <set>
#include <string>
#include <vector>

namespace {

int g_checks = 0;

void Check(bool condition, const char* what) {
    ++g_checks;
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", what);
        std::abort();
    }
}

#define CHECK(expr) Check((expr), #expr)

bool NearlyEqual(double left, double right, double tolerance = 1e-9) {
    return std::fabs(left - right) <= tolerance;
}

// The vertical-slice georeferencing: UTM 54N, 2 m pixels, north-up, tiepoint
// at the upper-left corner of pixel (0, 0). Matches the fixtures that
// tools/generate_fixtures.py emits.
constexpr double kEasting = 300000.0;
constexpr double kNorthing = 4400000.0;

usdraster::RasterGeoTransform NorthUp() {
    return usdraster::RasterGeoTransform::FromPixelScaleAndTiepoint(
        2.0, 2.0, 0.0, 0.0, kEasting, kNorthing);
}

// --- RasterTypes -----------------------------------------------------------

void TestDataTypes() {
    using usdraster::RasterDataType;
    CHECK(GetDataTypeSize(RasterDataType::UInt8) == 1);
    CHECK(GetDataTypeSize(RasterDataType::Int16) == 2);
    CHECK(GetDataTypeSize(RasterDataType::Float32) == 4);
    CHECK(GetDataTypeSize(RasterDataType::Float64) == 8);
    CHECK(std::string(GetDataTypeName(RasterDataType::Float32)) == "float32");

    CHECK(IsFloatingPoint(RasterDataType::Float32));
    CHECK(!IsFloatingPoint(RasterDataType::Int32));
    CHECK(IsSigned(RasterDataType::Int16));
    CHECK(!IsSigned(RasterDataType::UInt16));
}

void TestExactRepresentability() {
    using usdraster::RasterDataType;
    // The case a "float is wider, so it is safe" reading gets wrong: a 24-bit
    // significand cannot hold every 32-bit integer.
    CHECK(!IsExactlyRepresentable(RasterDataType::Int32,
                                  RasterDataType::Float32));
    CHECK(!IsExactlyRepresentable(RasterDataType::UInt32,
                                  RasterDataType::Float32));
    CHECK(IsExactlyRepresentable(RasterDataType::Int32,
                                 RasterDataType::Float64));
    CHECK(IsExactlyRepresentable(RasterDataType::UInt16,
                                 RasterDataType::Float32));

    CHECK(IsExactlyRepresentable(RasterDataType::UInt8,
                                 RasterDataType::UInt16));
    CHECK(!IsExactlyRepresentable(RasterDataType::UInt16,
                                  RasterDataType::UInt8));
    // A signed source has negatives an unsigned destination cannot hold, even
    // when the destination is wider.
    CHECK(!IsExactlyRepresentable(RasterDataType::Int8,
                                  RasterDataType::UInt32));
    // Unsigned into signed loses a bit to the sign, so equal widths are lossy.
    CHECK(!IsExactlyRepresentable(RasterDataType::UInt16,
                                  RasterDataType::Int16));
    CHECK(IsExactlyRepresentable(RasterDataType::UInt8,
                                 RasterDataType::Int16));
    // A float never lands exactly in an integer type.
    CHECK(!IsExactlyRepresentable(RasterDataType::Float32,
                                  RasterDataType::Int32));
    CHECK(IsExactlyRepresentable(RasterDataType::Float32,
                                 RasterDataType::Float32));
}

void TestPixelCountSaturates() {
    // A malformed header claiming an enormous size must produce a diagnosable
    // number, not a small wrapped one that then allocates successfully.
    const usdraster::RasterSize huge{1ull << 40, 1ull << 40};
    CHECK(huge.GetPixelCount() == std::numeric_limits<std::uint64_t>::max());
    CHECK((usdraster::RasterSize{4, 3}).GetPixelCount() == 12);
    CHECK((usdraster::RasterSize{0, 5}).GetPixelCount() == 0);
    CHECK((usdraster::RasterSize{0, 5}).IsEmpty());
}

// --- RasterWindow ----------------------------------------------------------

void TestWindowBasics() {
    const usdraster::RasterWindow window{10, 20, 30, 40};
    CHECK(window.GetEndX() == 40);
    CHECK(window.GetEndY() == 60);
    CHECK(window.GetPixelCount() == 1200);
    CHECK(!window.IsEmpty());

    CHECK(window.Contains(10, 20));
    CHECK(window.Contains(39, 59));
    CHECK(!window.Contains(40, 59));  // end is exclusive
    CHECK(!window.Contains(9, 20));

    CHECK(window.Contains(usdraster::RasterWindow{10, 20, 5, 5}));
    CHECK(!window.Contains(usdraster::RasterWindow{10, 20, 31, 5}));

    CHECK(usdraster::RasterWindow::FromSize(usdraster::RasterSize{8, 6}) ==
          (usdraster::RasterWindow{0, 0, 8, 6}));
}

void TestWindowIntersectDoesNotWrap() {
    const usdraster::RasterWindow left{0, 0, 10, 10};
    const usdraster::RasterWindow right{100, 100, 10, 10};

    // Disjoint. Written the obvious way, `right - left` on unsigned values
    // wraps to an enormous width that then allocates successfully.
    const auto disjoint = left.Intersect(right);
    CHECK(disjoint.IsEmpty());
    CHECK(disjoint.width == 0 && disjoint.height == 0);

    const auto overlap =
        left.Intersect(usdraster::RasterWindow{5, 5, 10, 10});
    CHECK(overlap == (usdraster::RasterWindow{5, 5, 5, 5}));

    // Touching edges do not overlap: the end is exclusive.
    CHECK(left.Intersect(usdraster::RasterWindow{10, 0, 5, 5}).IsEmpty());
}

void TestWindowClip() {
    const usdraster::RasterSize size{100, 80};

    CHECK((usdraster::RasterWindow{90, 70, 50, 50}).ClipTo(size) ==
          (usdraster::RasterWindow{90, 70, 10, 10}));

    // Entirely past the extent clips to empty rather than wrapping.
    CHECK((usdraster::RasterWindow{200, 200, 10, 10}).ClipTo(size).IsEmpty());

    // Entirely inside is unchanged.
    const usdraster::RasterWindow inside{10, 10, 20, 20};
    CHECK(inside.ClipTo(size) == inside);
}

void TestWindowSubdivideCoversExactly() {
    // 20x20 in 16x16 tiles: a four-pixel remainder on both edges. Sizes that
    // do not divide evenly are the normal case, not the exception.
    const usdraster::RasterWindow window{0, 0, 20, 20};
    const auto tiles = window.Subdivide(16, 16);
    CHECK(tiles.size() == 4);

    // Row-major order, edge tiles truncated rather than padded.
    CHECK(tiles[0] == (usdraster::RasterWindow{0, 0, 16, 16}));
    CHECK(tiles[1] == (usdraster::RasterWindow{16, 0, 4, 16}));
    CHECK(tiles[2] == (usdraster::RasterWindow{0, 16, 16, 4}));
    CHECK(tiles[3] == (usdraster::RasterWindow{16, 16, 4, 4}));

    // No gap: the areas sum to the window's.
    std::uint64_t area = 0;
    for (const auto& tile : tiles) {
        area += tile.GetPixelCount();
        CHECK(window.Contains(tile));
    }
    CHECK(area == window.GetPixelCount());

    // No overlap: every pixel is claimed by exactly one tile.
    std::set<std::pair<std::uint64_t, std::uint64_t>> claimed;
    for (const auto& tile : tiles) {
        for (std::uint64_t j = tile.y; j < tile.GetEndY(); ++j) {
            for (std::uint64_t i = tile.x; i < tile.GetEndX(); ++i) {
                CHECK(claimed.insert({i, j}).second);
            }
        }
    }
    CHECK(claimed.size() == window.GetPixelCount());
}

void TestWindowSubdivideOffsetAndDegenerate() {
    // A subdivision of an offset window stays anchored to that window, not to
    // the raster origin.
    const usdraster::RasterWindow offset{5, 7, 20, 10};
    const auto tiles = offset.Subdivide(16, 16);
    CHECK(tiles.size() == 2);
    CHECK(tiles[0] == (usdraster::RasterWindow{5, 7, 16, 10}));
    CHECK(tiles[1] == (usdraster::RasterWindow{21, 7, 4, 10}));

    std::uint64_t area = 0;
    for (const auto& tile : tiles) {
        area += tile.GetPixelCount();
    }
    CHECK(area == offset.GetPixelCount());

    // A tile larger than the window yields the window itself.
    const auto single = offset.Subdivide(100, 100);
    CHECK(single.size() == 1);
    CHECK(single[0] == offset);

    // Degenerate inputs yield nothing rather than dividing by zero.
    CHECK(offset.Subdivide(0, 16).empty());
    CHECK((usdraster::RasterWindow{}).Subdivide(16, 16).empty());
}

void TestWindowOverviewCoversRatherThanTruncates() {
    // Exact division.
    CHECK((usdraster::RasterWindow{1000, 1000, 512, 512}).ToOverview(2) ==
          (usdraster::RasterWindow{500, 500, 256, 256}));

    // Inexact. Rounding the extent down would drop the last row and column --
    // a one-pixel seam between adjacent tiles, which only becomes visible once
    // tiling exists and is expensive to find then.
    const usdraster::RasterWindow window{3, 3, 5, 5};  // covers 3..7
    const auto overview = window.ToOverview(2);
    CHECK(overview == (usdraster::RasterWindow{1, 1, 3, 3}));  // covers 2..7
    CHECK(overview.FromOverview(2).Contains(window));

    // Factor 1 and empty windows are identities.
    CHECK(window.ToOverview(1) == window);
    CHECK(window.ToOverview(0) == window);
    CHECK((usdraster::RasterWindow{}).ToOverview(4).IsEmpty());

    // The round trip widens rather than inverting: an overview pixel covers
    // `factor` full-resolution ones.
    CHECK((usdraster::RasterWindow{2, 2, 4, 4}).FromOverview(2) ==
          (usdraster::RasterWindow{4, 4, 8, 8}));
}

void TestSampledExtent() {
    // ceil(extent / step). Ten pixels at step 3 samples 0, 3, 6, 9 -- four.
    CHECK(usdraster::GetSampledExtent(10, 3) == 4);
    CHECK(usdraster::GetSampledExtent(9, 3) == 3);
    CHECK(usdraster::GetSampledExtent(1, 3) == 1);
    CHECK(usdraster::GetSampledExtent(0, 3) == 0);
    CHECK(usdraster::GetSampledExtent(10, 1) == 10);
    CHECK(usdraster::GetSampledExtent(10, 0) == 10);

    const auto sampled =
        usdraster::GetSampledSize(usdraster::RasterWindow{0, 0, 10, 7}, 3);
    CHECK(sampled == (usdraster::RasterSize{4, 3}));
}

void TestWindowAnchorConversion() {
    const usdraster::RasterWindow window{100, 200, 512, 256};
    const auto anchor = window.ToAnchor();
    CHECK(anchor.x == 100 && anchor.y == 200);
    CHECK(anchor.width == 512 && anchor.height == 256);
}

// --- RasterGeoTransform ----------------------------------------------------

void TestNorthUpConstruction() {
    const auto transform = NorthUp();
    CHECK(transform.a0 == kEasting);
    CHECK(transform.a1 == 2.0);
    CHECK(transform.a2 == 0.0);
    CHECK(transform.b0 == kNorthing);
    CHECK(transform.b1 == 0.0);
    // The tag carries an unsigned magnitude; the downward row direction is
    // made explicit exactly here.
    CHECK(transform.b2 == -2.0);

    CHECK(transform.IsNorthUp());
    CHECK(!transform.IsRotated());
    CHECK(!transform.IsSouthUp());
    CHECK(NearlyEqual(transform.GetPixelWidth(), 2.0));
    CHECK(NearlyEqual(transform.GetPixelHeight(), 2.0));

    // Negative determinant: the map flips handedness, which is what the
    // authoring library compensates for when winding faces.
    CHECK(transform.GetDeterminant() == -4.0);
    CHECK(transform.IsInvertible());
}

void TestPixelAnchoringIsNeverGuessed() {
    const auto transform = NorthUp();
    double sourceX = 0.0;
    double sourceY = 0.0;

    // Unknown anchoring cannot produce a plausible answer. This is the whole
    // reason Unknown is a value rather than an absence.
    CHECK(!transform.TryPixelToSource(0, 0, usdraster::PixelAnchor::Unknown,
                                      sourceX, sourceY));

    usdgeo::GeoBounds bounds;
    CHECK(!TryGetWindowBounds(transform, usdraster::RasterWindow{0, 0, 2, 2},
                              usdraster::PixelAnchor::Unknown, bounds));

    usdraster::RasterSize size{2, 2};
    std::uint64_t i = 0;
    std::uint64_t j = 0;
    CHECK(!transform.TrySourceToPixel(kEasting, kNorthing,
                                      usdraster::PixelAnchor::Unknown, size, i,
                                      j));
}

void TestPixelToSourceArea() {
    // pixel-is-area: the transform maps the upper-left CORNER of pixel (0, 0),
    // and a sample sits at the pixel centre, so px = i + 0.5.
    const auto transform = NorthUp();
    const auto area = usdraster::PixelAnchor::Area;
    double x = 0.0;
    double y = 0.0;

    CHECK(transform.TryPixelToSource(0, 0, area, x, y));
    CHECK(NearlyEqual(x, kEasting + 1.0));    // 300000 + 2*0.5
    CHECK(NearlyEqual(y, kNorthing - 1.0));   // 4400000 - 2*0.5

    CHECK(transform.TryPixelToSource(1, 0, area, x, y));
    CHECK(NearlyEqual(x, kEasting + 3.0));
    CHECK(NearlyEqual(y, kNorthing - 1.0));

    // The row index increases southward: Y decreases.
    CHECK(transform.TryPixelToSource(0, 1, area, x, y));
    CHECK(NearlyEqual(x, kEasting + 1.0));
    CHECK(NearlyEqual(y, kNorthing - 3.0));
}

void TestPixelToSourcePoint() {
    // pixel-is-point: the transform maps the CENTRE of pixel (0, 0), so
    // px = i. The difference from Area is exactly half a pixel, which is the
    // shift that looks plausible everywhere and is wrong everywhere.
    const auto transform = NorthUp();
    const auto point = usdraster::PixelAnchor::Point;
    double x = 0.0;
    double y = 0.0;

    CHECK(transform.TryPixelToSource(0, 0, point, x, y));
    CHECK(NearlyEqual(x, kEasting));
    CHECK(NearlyEqual(y, kNorthing));

    CHECK(transform.TryPixelToSource(1, 1, point, x, y));
    CHECK(NearlyEqual(x, kEasting + 2.0));
    CHECK(NearlyEqual(y, kNorthing - 2.0));

    // Half a pixel apart, in both axes, for the same pixel index.
    double pointX = 0.0;
    double pointY = 0.0;
    CHECK(transform.TryPixelToSource(0, 0, point, pointX, pointY));
    double areaX = 0.0;
    double areaY = 0.0;
    CHECK(transform.TryPixelToSource(0, 0, usdraster::PixelAnchor::Area, areaX,
                                     areaY));
    CHECK(NearlyEqual(areaX - pointX, 1.0));
    CHECK(NearlyEqual(areaY - pointY, -1.0));
}

void TestSouthUp() {
    // A positive b2 is legal: the raster is stored bottom-to-top. Handled and
    // recorded, not rejected.
    usdraster::RasterGeoTransform transform;
    transform.a0 = kEasting;
    transform.a1 = 2.0;
    transform.b0 = kNorthing;
    transform.b2 = 2.0;

    CHECK(transform.IsSouthUp());
    CHECK(!transform.IsNorthUp());
    CHECK(!transform.IsRotated());
    CHECK(transform.GetDeterminant() == 4.0);

    double x = 0.0;
    double y = 0.0;
    CHECK(transform.TryPixelToSource(0, 1, usdraster::PixelAnchor::Point, x, y));
    CHECK(NearlyEqual(x, kEasting));
    // Y increases with the row index here, which is the whole distinction.
    CHECK(NearlyEqual(y, kNorthing + 2.0));
}

void TestRotated() {
    // 30 degrees, 2 m pixels. A rotation that is ignored produces geometry
    // that is silently in the wrong place, which is worse than geometry that
    // fails to load -- so rotation is supported from the first line of code.
    const double cos30 = 0.8660254037844387;
    const double sin30 = 0.5;
    usdraster::RasterGeoTransform transform;
    transform.a0 = kEasting;
    transform.a1 = 2.0 * cos30;
    transform.a2 = -2.0 * sin30;
    transform.b0 = kNorthing;
    transform.b1 = 2.0 * sin30;
    transform.b2 = 2.0 * cos30;

    CHECK(transform.IsRotated());
    CHECK(!transform.IsNorthUp());

    // The true sample spacing is the column length, not a1 or b2: taking a1
    // alone would understate it by the cosine of the rotation.
    CHECK(NearlyEqual(transform.GetPixelWidth(), 2.0));
    CHECK(NearlyEqual(transform.GetPixelHeight(), 2.0));
    CHECK(NearlyEqual(transform.a1, 1.7320508075688772));
    CHECK(NearlyEqual(transform.GetDeterminant(), 4.0));

    double x = 0.0;
    double y = 0.0;
    const auto point = usdraster::PixelAnchor::Point;
    CHECK(transform.TryPixelToSource(1, 0, point, x, y));
    CHECK(NearlyEqual(x, kEasting + 1.7320508075688772));
    CHECK(NearlyEqual(y, kNorthing + 1.0));

    CHECK(transform.TryPixelToSource(0, 1, point, x, y));
    CHECK(NearlyEqual(x, kEasting - 1.0));
    CHECK(NearlyEqual(y, kNorthing + 1.7320508075688772));
}

void TestInverseRoundTrip() {
    const double cos30 = 0.8660254037844387;
    const double sin30 = 0.5;
    usdraster::RasterGeoTransform rotated;
    rotated.a0 = kEasting;
    rotated.a1 = 2.0 * cos30;
    rotated.a2 = -2.0 * sin30;
    rotated.b0 = kNorthing;
    rotated.b1 = 2.0 * sin30;
    rotated.b2 = 2.0 * cos30;

    const usdraster::RasterGeoTransform transforms[2] = {NorthUp(), rotated};
    const usdraster::RasterSize size{64, 64};

    for (const auto& transform : transforms) {
        for (const auto anchor : {usdraster::PixelAnchor::Area,
                                  usdraster::PixelAnchor::Point}) {
            for (std::uint64_t j : {std::uint64_t{0}, std::uint64_t{7},
                                    std::uint64_t{63}}) {
                for (std::uint64_t i : {std::uint64_t{0}, std::uint64_t{13},
                                        std::uint64_t{63}}) {
                    double x = 0.0;
                    double y = 0.0;
                    CHECK(transform.TryPixelToSource(i, j, anchor, x, y));

                    std::uint64_t backI = 0;
                    std::uint64_t backJ = 0;
                    CHECK(transform.TrySourceToPixel(x, y, anchor, size, backI,
                                                     backJ));
                    // The inverse is computed from the 2x2 rather than through
                    // a north-up special case, which is what makes the rotated
                    // case land on the same pixel it came from.
                    CHECK(backI == i);
                    CHECK(backJ == j);
                }
            }
        }
    }
}

void TestSourceToPixelRejectsOutOfBounds() {
    const auto transform = NorthUp();
    const usdraster::RasterSize size{2, 2};
    const auto area = usdraster::PixelAnchor::Area;
    std::uint64_t i = 0;
    std::uint64_t j = 0;

    // Well outside, in both directions. Note the north-up Y axis: a position
    // north of the tiepoint is a negative row.
    CHECK(!transform.TrySourceToPixel(kEasting - 100.0, kNorthing, area, size,
                                      i, j));
    CHECK(!transform.TrySourceToPixel(kEasting + 100.0, kNorthing, area, size,
                                      i, j));
    CHECK(!transform.TrySourceToPixel(kEasting, kNorthing + 100.0, area, size,
                                      i, j));
    // Inside is accepted.
    CHECK(transform.TrySourceToPixel(kEasting + 1.0, kNorthing - 1.0, area,
                                     size, i, j));
    CHECK(i == 0 && j == 0);
}

void TestDegenerateTransformIsNotInvertible() {
    // A zero pixel size collapses an axis. Inverting it gives coordinates that
    // are arithmetically defined and physically meaningless, so it is rejected
    // rather than producing geometry several astronomical units across.
    usdraster::RasterGeoTransform collapsed;
    collapsed.a1 = 0.0;
    collapsed.b2 = 0.0;
    CHECK(!collapsed.IsInvertible());

    usdraster::RasterGeoTransform parallel;
    parallel.a1 = 1.0;
    parallel.a2 = 2.0;
    parallel.b1 = 2.0;
    parallel.b2 = 4.0;  // rows and columns map to the same direction
    CHECK(parallel.GetDeterminant() == 0.0);
    CHECK(!parallel.IsInvertible());

    usdraster::PixelCoord pixel;
    CHECK(!collapsed.TryInverse(0.0, 0.0, pixel));

    usdraster::RasterGeoTransform nonFinite;
    nonFinite.a1 = std::numeric_limits<double>::quiet_NaN();
    CHECK(!nonFinite.IsInvertible());
}

void TestWindowBoundsFollowsAnchoring() {
    const auto transform = NorthUp();
    const usdraster::RasterWindow window{0, 0, 2, 2};
    usdgeo::GeoBounds bounds;

    // Area: the union of cell areas spans `width` cells -- 2 cells of 2 m.
    CHECK(TryGetWindowBounds(transform, window, usdraster::PixelAnchor::Area,
                             bounds));
    CHECK(NearlyEqual(bounds.min.x, kEasting));
    CHECK(NearlyEqual(bounds.max.x, kEasting + 4.0));
    CHECK(NearlyEqual(bounds.min.y, kNorthing - 4.0));
    CHECK(NearlyEqual(bounds.max.y, kNorthing));

    // Point: the outermost SAMPLE positions span `width - 1` intervals -- one
    // interval of 2 m. Reporting the Area extent here would overstate the
    // raster by a pixel, and no visual check catches that.
    CHECK(TryGetWindowBounds(transform, window, usdraster::PixelAnchor::Point,
                             bounds));
    CHECK(NearlyEqual(bounds.min.x, kEasting));
    CHECK(NearlyEqual(bounds.max.x, kEasting + 2.0));
    CHECK(NearlyEqual(bounds.min.y, kNorthing - 2.0));
    CHECK(NearlyEqual(bounds.max.y, kNorthing));

    // Z is the degenerate range at zero: elevation is a band value, not a
    // property of the transform.
    CHECK(bounds.min.z == 0.0 && bounds.max.z == 0.0);
}

void TestRotatedBoundsUsesAllFourCorners() {
    // For a rotated transform the extent is not the image of one corner pair.
    // Taking two corners would understate it, and the understatement grows
    // with the rotation.
    const double cos45 = 0.7071067811865476;
    usdraster::RasterGeoTransform transform;
    transform.a1 = cos45;
    transform.a2 = -cos45;
    transform.b1 = cos45;
    transform.b2 = cos45;

    usdgeo::GeoBounds bounds;
    CHECK(TryGetWindowBounds(transform, usdraster::RasterWindow{0, 0, 2, 2},
                             usdraster::PixelAnchor::Area, bounds));

    // Corners of the pixel-space square (0,0)-(2,2) map to (0,0),
    // (1.414, 1.414), (0, 2.828), (-1.414, 1.414).
    CHECK(NearlyEqual(bounds.min.x, -2.0 * cos45));
    CHECK(NearlyEqual(bounds.max.x, 2.0 * cos45));
    CHECK(NearlyEqual(bounds.min.y, 0.0));
    CHECK(NearlyEqual(bounds.max.y, 4.0 * cos45));
}

void TestFromMatrix() {
    // ModelTransformationTag, row-major 4x4. Only the affine terms are taken;
    // a raster's pixel space is planar, so the z column is dropped.
    const double matrix[16] = {2.0, 0.0, 0.0, kEasting,
                              0.0, -2.0, 0.0, kNorthing,
                              0.0, 0.0, 0.0, 0.0,
                              0.0, 0.0, 0.0, 1.0};
    const auto transform = usdraster::RasterGeoTransform::FromMatrix(matrix);
    CHECK(transform == NorthUp());
}

// --- Precision -------------------------------------------------------------

void TestPrecisionRoundTripThroughFloat() {
    // The measurement this milestone exists to make: a UTM coordinate near
    // 4.4e6 must survive the narrowing to float that UsdGeomMesh points
    // impose. It survives only because of the local origin.
    //
    // The pixel size here is 0.5 m -- a lidar-derived DEM -- and that choice is
    // load-bearing. float holds every integer up to 2^24 exactly, so a 2 m grid
    // anchored on integer coordinates round-trips perfectly at 4.4e6 and would
    // make this test pass while proving nothing. Sub-metre sampling puts
    // fractional coordinates where the ulp is already 0.5 m, which is where the
    // loss actually appears -- and sub-metre DEMs are exactly the data this
    // repository is for.
    const auto transform = usdraster::RasterGeoTransform::
        FromPixelScaleAndTiepoint(0.5, 0.5, 0.0, 0.0, kEasting, kNorthing);
    const auto anchor = usdraster::PixelAnchor::Area;
    const usdraster::RasterWindow window{0, 0, 512, 512};

    usdgeo::GeoBounds bounds;
    CHECK(TryGetWindowBounds(transform, window, anchor, bounds));
    const auto origin = usdgeo::LocalOrigin::FromBounds(bounds);

    double worstWithOrigin = 0.0;
    double worstWithout = 0.0;
    for (std::uint64_t j = 0; j < 512; j += 71) {
        for (std::uint64_t i = 0; i < 512; i += 71) {
            double x = 0.0;
            double y = 0.0;
            CHECK(transform.TryPixelToSource(i, j, anchor, x, y));

            const usdgeo::Vec3d source{x, y, 0.0};
            const usdgeo::Vec3d local = origin.ToStageLocal(source);
            const usdgeo::Vec3d narrowed{
                static_cast<double>(static_cast<float>(local.x)),
                static_cast<double>(static_cast<float>(local.y)), 0.0};
            const usdgeo::Vec3d recovered = origin.ToSource(narrowed);

            worstWithOrigin = std::max(
                worstWithOrigin,
                std::max(std::fabs(recovered.x - source.x),
                         std::fabs(recovered.y - source.y)));
            worstWithout = std::max(
                worstWithout,
                std::fabs(static_cast<double>(static_cast<float>(source.y)) -
                          source.y));
        }
    }

    // One millimetre is the documented tolerance.
    CHECK(worstWithOrigin < 1e-3);
    // And the counter-measurement: without the origin, the same coordinates
    // lose a quarter of a metre -- the ulp of float at 4.4e6 is 0.5 m, so a
    // half-metre grid cannot even be represented. That is visible wobble on a
    // terrain surface, not a rounding detail.
    CHECK(worstWithout > 1e-2);
}

// --- NoData ----------------------------------------------------------------

void TestNoDataExactComparison() {
    const usdraster::NoDataValue sentinel(-9999.0);
    CHECK(sentinel.IsSet());
    CHECK(sentinel.Matches(-9999.0));
    CHECK(!sentinel.Matches(-9998.0));

    // No tolerance, deliberately. A tolerance silently deletes legitimate data
    // at its boundary, and a DEM whose real elevations approach the sentinel is
    // exactly where that happens.
    CHECK(!sentinel.Matches(-9999.0001));
    CHECK(!sentinel.Matches(-9998.9999));

    // A source that declares no NoData matches nothing.
    const auto none = usdraster::NoDataValue::None();
    CHECK(!none.IsSet());
    CHECK(!none.Matches(0.0));
    CHECK(!none.Matches(std::numeric_limits<double>::quiet_NaN()));
}

void TestNoDataNaN() {
    // NaN == NaN is false in IEEE-754, so the obvious comparison would match
    // nothing and every NoData cell would be authored as terrain.
    const usdraster::NoDataValue nanSentinel(
        std::numeric_limits<double>::quiet_NaN());
    CHECK(nanSentinel.IsSet());
    CHECK(nanSentinel.Matches(std::numeric_limits<double>::quiet_NaN()));
    CHECK(nanSentinel.Matches(-std::numeric_limits<double>::quiet_NaN()));
    CHECK(nanSentinel.Matches(std::numeric_limits<double>::signaling_NaN()));
    CHECK(!nanSentinel.Matches(0.0));
    CHECK(!nanSentinel.Matches(std::numeric_limits<double>::infinity()));

    // A numeric sentinel does not swallow NaN samples.
    CHECK(!usdraster::NoDataValue(0.0).Matches(
        std::numeric_limits<double>::quiet_NaN()));

    // Equality between two NoData values follows the same rule.
    CHECK(nanSentinel ==
          usdraster::NoDataValue(std::numeric_limits<double>::quiet_NaN()));
    CHECK(!(nanSentinel == usdraster::NoDataValue(0.0)));
    CHECK(usdraster::NoDataValue::None() == usdraster::NoDataValue::None());
    CHECK(!(usdraster::NoDataValue::None() == usdraster::NoDataValue(0.0)));
}

void TestNoDataPolicyNames() {
    CHECK(std::string(GetNoDataPolicyName(usdraster::NoDataPolicy::Skip)) ==
          "skip");
    CHECK(std::string(GetNoDataPolicyName(usdraster::NoDataPolicy::Fill)) ==
          "fill");
    CHECK(std::string(GetNoDataPolicyName(usdraster::NoDataPolicy::Keep)) ==
          "keep");
}

// --- RasterGrid ------------------------------------------------------------

void TestRasterGrid() {
    const usdraster::RasterWindow window{10, 20, 4, 3};
    usdraster::RasterGrid grid(window, 1, 1, usdraster::RasterDataType::Float32,
                               usdraster::NoDataValue(-9999.0));

    CHECK(grid.GetSize() == (usdraster::RasterSize{4, 3}));
    CHECK(grid.GetSampleCount() == 12);
    CHECK(grid.GetWindow() == window);
    CHECK(grid.GetSourceType() == usdraster::RasterDataType::Float32);

    grid.SetSample(0, 0, 10.0);
    grid.SetSample(3, 2, 40.0);
    grid.SetSample(1, 1, -9999.0);
    CHECK(grid.GetSample(0, 0) == 10.0);
    CHECK(grid.GetSample(3, 2) == 40.0);
    CHECK(grid.IsNoData(1, 1));
    CHECK(!grid.IsNoData(0, 0));

    // Out of range reads return zero rather than reading past the buffer.
    CHECK(grid.GetSample(4, 0) == 0.0);
    CHECK(grid.GetSample(0, 3) == 0.0);
    CHECK(!grid.IsNoData(99, 99));
    grid.SetSample(99, 99, 1.0);  // ignored, not a write past the end

    // Sampled-grid coordinates are not source pixel coordinates. This is the
    // conversion that keeps the two from being confused at a call site.
    std::uint64_t i = 0;
    std::uint64_t j = 0;
    grid.GetSourcePixel(0, 0, i, j);
    CHECK(i == 10 && j == 20);
    grid.GetSourcePixel(3, 2, i, j);
    CHECK(i == 13 && j == 22);
}

void TestRasterGridDecimated() {
    // A step never re-anchors the grid: sample (0, 0) is always the window's
    // first pixel, which is what keeps a coarse mesh registered with a fine one.
    const usdraster::RasterWindow window{100, 200, 10, 7};
    usdraster::RasterGrid grid(window, 3, 1,
                               usdraster::RasterDataType::UInt16,
                               usdraster::NoDataValue::None());

    CHECK(grid.GetSamplingStep() == 3);
    CHECK(grid.GetSize() == (usdraster::RasterSize{4, 3}));

    std::uint64_t i = 0;
    std::uint64_t j = 0;
    grid.GetSourcePixel(0, 0, i, j);
    CHECK(i == 100 && j == 200);
    grid.GetSourcePixel(1, 1, i, j);
    CHECK(i == 103 && j == 203);
    grid.GetSourcePixel(3, 2, i, j);
    CHECK(i == 109 && j == 206);

    // A zero step is normalized to one rather than dividing by zero.
    usdraster::RasterGrid zeroStep(window, 0, 1,
                                   usdraster::RasterDataType::UInt16,
                                   usdraster::NoDataValue::None());
    CHECK(zeroStep.GetSamplingStep() == 1);
    CHECK(zeroStep.GetSize() == (usdraster::RasterSize{10, 7}));
}

// --- RasterMetadata --------------------------------------------------------

void TestRasterMetadata() {
    usdraster::RasterMetadata metadata;
    metadata.size = usdraster::RasterSize{1024, 768};
    metadata.geoTransform = NorthUp();
    metadata.pixelAnchor = usdraster::PixelAnchor::Area;
    metadata.hasGeoTransform = true;

    usdraster::RasterBandInfo band;
    band.index = 1;
    band.dataType = usdraster::RasterDataType::Float32;
    band.noData = usdraster::NoDataValue(-9999.0);
    band.scale = 0.01;
    band.offset = 100.0;
    metadata.bands.push_back(band);

    CHECK(metadata.GetBandCount() == 1);
    // 1-based, matching every external tool a user cross-checks against.
    CHECK(metadata.FindBand(1) != nullptr);
    CHECK(metadata.FindBand(0) == nullptr);
    CHECK(metadata.FindBand(2) == nullptr);

    // scale * raw + offset, through double.
    CHECK(NearlyEqual(metadata.FindBand(1)->ApplyScaleAndOffset(500.0), 105.0));

    usdraster::RasterBandInfo plain;
    CHECK(plain.ApplyScaleAndOffset(42.0) == 42.0);

    CHECK(!metadata.IsTiled());
    metadata.nativeTileSize = usdraster::RasterSize{256, 256};
    CHECK(metadata.IsTiled());
}

void TestOverviewFactorRounds() {
    usdraster::RasterMetadata metadata;
    metadata.size = usdraster::RasterSize{4096, 4096};
    metadata.overviewSizes.push_back(usdraster::RasterSize{2048, 2048});
    metadata.overviewSizes.push_back(usdraster::RasterSize{1024, 1024});
    CHECK(metadata.GetOverviewFactor(0) == 2);
    CHECK(metadata.GetOverviewFactor(1) == 4);
    CHECK(metadata.GetOverviewFactor(2) == 0);  // no such level

    // The odd case: truncating 4001 / 2001 gives 1, which would report the
    // half-resolution level as full resolution and read the wrong pixels for
    // every window.
    usdraster::RasterMetadata odd;
    odd.size = usdraster::RasterSize{4001, 4001};
    odd.overviewSizes.push_back(usdraster::RasterSize{2001, 2001});
    CHECK(odd.GetOverviewFactor(0) == 2);
}

// --- RasterReadOptions -----------------------------------------------------

void TestReadOptionsCacheKey() {
    usdraster::RasterReadOptions base;
    base.band = 1;
    base.samplingStep = 2;

    usdgeo::CacheKey baseKey;
    base.ContributeToCacheKey(baseKey);

    // Options that change decoded values participate in identity.
    usdraster::RasterReadOptions otherBand = base;
    otherBand.band = 2;
    usdgeo::CacheKey otherBandKey;
    otherBand.ContributeToCacheKey(otherBandKey);
    CHECK(baseKey.GetDigest() != otherBandKey.GetDigest());

    usdraster::RasterReadOptions otherStep = base;
    otherStep.samplingStep = 4;
    usdgeo::CacheKey otherStepKey;
    otherStep.ContributeToCacheKey(otherStepKey);
    CHECK(baseKey.GetDigest() != otherStepKey.GetDigest());

    // Options that only change scheduling do not: including them would key two
    // identical results to two cache entries.
    usdraster::RasterReadOptions scheduled = base;
    scheduled.memoryBudgetBytes = 64u * 1024u * 1024u;
    scheduled.isCancelled = [] { return false; };
    usdgeo::CacheKey scheduledKey;
    scheduled.ContributeToCacheKey(scheduledKey);
    CHECK(baseKey.GetDigest() == scheduledKey.GetDigest());

    // "Let the reader choose" is a different request from "use level 0", even
    // when they resolve to the same level today.
    usdraster::RasterReadOptions explicitLevel = base;
    explicitLevel.overviewLevel = 0;
    usdgeo::CacheKey explicitKey;
    explicitLevel.ContributeToCacheKey(explicitKey);
    CHECK(baseKey.GetDigest() != explicitKey.GetDigest());

    CHECK(!base.IsCancelled());
    usdraster::RasterReadOptions cancelled = base;
    cancelled.isCancelled = [] { return true; };
    CHECK(cancelled.IsCancelled());
}

// --- Sources ---------------------------------------------------------------

void TestMemorySource() {
    const std::uint8_t bytes[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    usdraster::MemorySource source(bytes, sizeof(bytes), "fixture");

    CHECK(source.GetSize() == 8);
    CHECK(source.GetIdentifier() == "fixture");

    std::uint8_t out[4] = {};
    auto result = source.Read(2, 4, out);
    CHECK(result.IsOk());
    CHECK(result.bytesRead == 4);
    CHECK(out[0] == 2 && out[3] == 5);

    // A truncated tail is a short read, reported rather than retried: retry
    // policy is transport policy, and transport belongs to the resolver.
    result = source.Read(6, 4, out);
    CHECK(result.status == usdraster::ReadStatus::ShortRead);
    CHECK(result.bytesRead == 2);
    CHECK(out[0] == 6 && out[1] == 7);

    CHECK(source.Read(8, 1, out).status == usdraster::ReadStatus::EndOfSource);
    CHECK(source.Read(99, 1, out).status == usdraster::ReadStatus::EndOfSource);

    // A zero-length read at a valid offset succeeds, so a reader that plans an
    // empty range need not special-case it.
    CHECK(source.Read(8, 0, out).IsOk());
    CHECK(source.Read(0, 0, out).IsOk());

    CHECK(source.Read(0, 1, nullptr).status == usdraster::ReadStatus::Failed);
    CHECK(std::string(GetReadStatusName(usdraster::ReadStatus::ShortRead)) ==
          "shortRead");
}

void TestRecordingSourceProvesSelectivity() {
    // The assertion every later selectivity claim rests on: a bounded read
    // must request only the ranges it needs, and that is only observable if
    // the ranges are recorded.
    std::vector<std::uint8_t> bytes(1024);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<std::uint8_t>(i & 0xff);
    }
    usdraster::MemorySource inner(bytes.data(), bytes.size(), "fixture");
    usdraster::RecordingSource recording(inner);

    std::uint8_t out[64] = {};
    CHECK(recording.Read(256, 64, out).IsOk());
    CHECK(recording.Read(512, 64, out).IsOk());

    CHECK(recording.GetReadCount() == 2);
    CHECK(recording.GetBytesRead() == 128);
    CHECK(recording.GetDistinctBytesRead() == 128);

    // The bytes the read needed were requested...
    CHECK(recording.DidReadRange(256, 64));
    CHECK(recording.DidReadRange(300, 1));
    // ...and the bytes it did not need were not. This is what a "touches only
    // the intersecting tiles" claim actually means.
    CHECK(!recording.DidReadRange(0, 256));
    CHECK(!recording.DidReadRange(320, 192));
    CHECK(!recording.DidReadRange(576, 448));

    // Delegation is transparent.
    CHECK(recording.GetSize() == 1024);
    CHECK(recording.GetIdentifier() == "fixture");
    CHECK(out[0] == static_cast<std::uint8_t>(512 & 0xff));

    recording.Reset();
    CHECK(recording.GetReadCount() == 0);
    CHECK(recording.GetBytesRead() == 0);
    CHECK(recording.GetDistinctBytesRead() == 0);
}

void TestRecordingSourceDetectsRefetch() {
    std::vector<std::uint8_t> bytes(1024, 0);
    usdraster::MemorySource inner(bytes.data(), bytes.size());
    usdraster::RecordingSource recording(inner);

    std::uint8_t out[128] = {};
    recording.Read(0, 100, out);
    recording.Read(50, 100, out);   // overlaps the first by 50
    recording.Read(0, 100, out);    // an outright refetch

    // Requested bytes count every request; distinct bytes count each byte
    // once. The gap between them is a read plan fetching the same bytes twice,
    // which on a remote source is paid for in latency as well as bandwidth.
    CHECK(recording.GetBytesRead() == 300);
    CHECK(recording.GetDistinctBytesRead() == 150);
    CHECK(recording.GetReadCount() == 3);

    // Adjacent-but-not-overlapping ranges are counted once each.
    recording.Reset();
    recording.Read(0, 64, out);
    recording.Read(64, 64, out);
    CHECK(recording.GetBytesRead() == 128);
    CHECK(recording.GetDistinctBytesRead() == 128);
}

}  // namespace

int main() {
    TestDataTypes();
    TestExactRepresentability();
    TestPixelCountSaturates();

    TestWindowBasics();
    TestWindowIntersectDoesNotWrap();
    TestWindowClip();
    TestWindowSubdivideCoversExactly();
    TestWindowSubdivideOffsetAndDegenerate();
    TestWindowOverviewCoversRatherThanTruncates();
    TestSampledExtent();
    TestWindowAnchorConversion();

    TestNorthUpConstruction();
    TestPixelAnchoringIsNeverGuessed();
    TestPixelToSourceArea();
    TestPixelToSourcePoint();
    TestSouthUp();
    TestRotated();
    TestInverseRoundTrip();
    TestSourceToPixelRejectsOutOfBounds();
    TestDegenerateTransformIsNotInvertible();
    TestWindowBoundsFollowsAnchoring();
    TestRotatedBoundsUsesAllFourCorners();
    TestFromMatrix();

    TestPrecisionRoundTripThroughFloat();

    TestNoDataExactComparison();
    TestNoDataNaN();
    TestNoDataPolicyNames();

    TestRasterGrid();
    TestRasterGridDecimated();

    TestRasterMetadata();
    TestOverviewFactorRounds();

    TestReadOptionsCacheKey();

    TestMemorySource();
    TestRecordingSourceProvesSelectivity();
    TestRecordingSourceDetectsRefetch();

    std::printf("usdRasterCore: %d checks passed\n", g_checks);
    return 0;
}
