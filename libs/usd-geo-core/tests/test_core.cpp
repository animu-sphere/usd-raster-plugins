// usdGeoCore unit tests.
//
// No test framework: the core lane must build and run with no OpenUSD and no
// package manager present, so the assertions are a function and an abort. A
// failure aborts with a non-zero status, which is what CTest reads.

#include "usdgeo/CacheKey.h"
#include "usdgeo/CrsDescription.h"
#include "usdgeo/Diagnostic.h"
#include "usdgeo/GeoBounds.h"
#include "usdgeo/LocalOrigin.h"
#include "usdgeo/TileId.h"
#include "usdgeo/Vec3d.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <set>
#include <string>
#include <unordered_set>

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

bool NearlyEqual(double left, double right, double tolerance = 1e-12) {
    return std::fabs(left - right) <= tolerance;
}

// --- GeoBounds -------------------------------------------------------------

void TestBoundsAccumulation() {
    auto bounds = usdgeo::GeoBounds::Empty();
    CHECK(!bounds.IsValid());

    // A default-constructed GeoBounds is the degenerate box at the origin,
    // which is valid. Only Empty() is the accumulator's starting point.
    CHECK(usdgeo::GeoBounds().IsValid());

    bounds.Expand({300000.25, 4400000.0, 10.0});
    CHECK(bounds.IsValid());
    bounds.Expand({300010.25, 4399990.0, 30.0});

    CHECK(NearlyEqual(bounds.min.x, 300000.25));
    CHECK(NearlyEqual(bounds.max.x, 300010.25));
    CHECK(NearlyEqual(bounds.min.y, 4399990.0));
    CHECK(NearlyEqual(bounds.max.y, 4400000.0));

    const auto size = bounds.Size();
    CHECK(NearlyEqual(size.x, 10.0));
    CHECK(NearlyEqual(size.y, 10.0));
    CHECK(NearlyEqual(size.z, 20.0));

    const auto center = bounds.Center();
    CHECK(NearlyEqual(center.x, 300005.25));
    CHECK(NearlyEqual(center.y, 4399995.0));
    CHECK(NearlyEqual(center.z, 20.0));
}

void TestBoundsIgnoresNonFinite() {
    // A single NaN elevation must not poison the extent. NoData is where a
    // sentinel elevation is handled, not here.
    auto bounds = usdgeo::GeoBounds::Empty();
    bounds.Expand({0.0, 0.0, 5.0});
    bounds.Expand({10.0, 10.0, 15.0});
    const auto before = bounds;

    bounds.Expand({std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0});
    bounds.Expand({0.0, std::numeric_limits<double>::infinity(), 0.0});
    bounds.Expand({0.0, 0.0, -std::numeric_limits<double>::infinity()});
    CHECK(bounds == before);

    // An invalid extent contributes nothing when merged.
    bounds.Union(usdgeo::GeoBounds::Empty());
    CHECK(bounds == before);
}

void TestBoundsSizeOfInvalidIsZero() {
    const auto size = usdgeo::GeoBounds::Empty().Size();
    CHECK(size.x == 0.0 && size.y == 0.0 && size.z == 0.0);
}

// --- LocalOrigin -----------------------------------------------------------

void TestLocalOriginRoundTrip() {
    // The precision round-trip the milestone exists to prove: a UTM coordinate
    // near 4.4e6 must survive the narrowing to float that UsdGeomMesh points
    // impose, and come back within a tolerance the contract can state.
    auto bounds = usdgeo::GeoBounds::Empty();
    bounds.Expand({300000.0, 4400000.0, 10.0});
    bounds.Expand({300100.0, 4400100.0, 40.0});

    const auto origin = usdgeo::LocalOrigin::FromBounds(bounds);
    CHECK(NearlyEqual(origin.GetValue().x, 300050.0));
    CHECK(NearlyEqual(origin.GetValue().y, 4400050.0));
    CHECK(NearlyEqual(origin.GetValue().z, 25.0));

    const usdgeo::Vec3d source{300012.375, 4400087.25, 33.5};
    const usdgeo::Vec3d local = origin.ToStageLocal(source);

    // Narrow the way the authoring library will, then recover.
    const usdgeo::Vec3d narrowed{static_cast<double>(static_cast<float>(local.x)),
                                 static_cast<double>(static_cast<float>(local.y)),
                                 static_cast<double>(static_cast<float>(local.z))};
    const usdgeo::Vec3d recovered = origin.ToSource(narrowed);

    // One millimetre, and in fact exact here: the offsets are small enough
    // that float represents them without loss. The assertion is the contract,
    // not the observation.
    CHECK(NearlyEqual(recovered.x, source.x, 1e-3));
    CHECK(NearlyEqual(recovered.y, source.y, 1e-3));
    CHECK(NearlyEqual(recovered.z, source.z, 1e-3));
}

void TestLocalOriginIsWhatMakesFloatViable() {
    // The counter-case that justifies the type. Narrowing the raw source
    // coordinate loses far more than a millimetre, which is the wobble a user
    // sees on a terrain surface.
    const double source = 4400087.25;
    const double narrowedDirectly = static_cast<double>(static_cast<float>(source));
    CHECK(std::fabs(narrowedDirectly - source) > 1e-3);
}

void TestLocalOriginIsStableUnderWindowChange() {
    // Quantization is the point: an origin derived from a slightly different
    // extent must not move, or every cached tile is invalidated and every
    // already-authored position shifts.
    auto wide = usdgeo::GeoBounds::Empty();
    wide.Expand({300000.0, 4400000.0, 0.0});
    wide.Expand({300100.0, 4400100.0, 0.0});

    auto narrow = usdgeo::GeoBounds::Empty();
    narrow.Expand({300000.4, 4400000.2, 0.0});
    narrow.Expand({300100.1, 4400099.9, 0.0});

    CHECK(usdgeo::LocalOrigin::FromBounds(wide) ==
          usdgeo::LocalOrigin::FromBounds(narrow));
}

void TestLocalOriginQuantum() {
    auto bounds = usdgeo::GeoBounds::Empty();
    bounds.Expand({1234.0, 5678.0, 0.0});
    bounds.Expand({1236.0, 5680.0, 0.0});
    // Centre is (1235, 5679); a 100 m quantum rounds it to (1200, 5700).
    const auto origin = usdgeo::LocalOrigin::FromBounds(bounds, 100.0);
    CHECK(NearlyEqual(origin.GetValue().x, 1200.0));
    CHECK(NearlyEqual(origin.GetValue().y, 5700.0));

    // An invalid extent yields the origin at zero: nothing is known to offset.
    const auto empty = usdgeo::LocalOrigin::FromBounds(usdgeo::GeoBounds::Empty());
    CHECK(empty.GetValue() == usdgeo::Vec3d{});
}

// --- CrsDescription --------------------------------------------------------

void TestCrsDescription() {
    usdgeo::CrsDescription crs;
    CHECK(crs.IsEmpty());
    CHECK(!crs.IsMetric());

    crs.kind = usdgeo::CrsKind::Projected;
    crs.epsgCode = 32654;
    crs.linearUnit = usdgeo::LinearUnit::Metre;
    crs.linearUnitMetres = 1.0;
    CHECK(!crs.IsEmpty());
    CHECK(crs.IsMetric());
    CHECK(!crs.HasMixedUnits());

    // A geographic CRS is never metric: degrees are not metres, and authoring
    // one into a metric stage takes an explicit, recorded choice.
    usdgeo::CrsDescription geographic;
    geographic.kind = usdgeo::CrsKind::Geographic;
    geographic.epsgCode = 4326;
    geographic.angularUnit = usdgeo::AngularUnit::Degree;
    CHECK(!geographic.IsMetric());

    // Mixed horizontal and vertical units are detected, not silently mixed.
    crs.verticalUnit = usdgeo::LinearUnit::USSurveyFoot;
    CHECK(crs.HasMixedUnits());

    CHECK(GetMetresPerUnit(usdgeo::LinearUnit::Metre) == 1.0);
    CHECK(GetMetresPerUnit(usdgeo::LinearUnit::Foot) == 0.3048);
    CHECK(GetMetresPerUnit(usdgeo::LinearUnit::Unknown) == 0.0);
    // The US survey foot differs from the international foot in the eighth
    // decimal place. At a state-plane easting that is centimetres.
    CHECK(GetMetresPerUnit(usdgeo::LinearUnit::USSurveyFoot) !=
          GetMetresPerUnit(usdgeo::LinearUnit::Foot));
    CHECK(NearlyEqual(GetMetresPerUnit(usdgeo::LinearUnit::USSurveyFoot),
                      0.3048006096012192, 1e-15));
}

// --- TileId ----------------------------------------------------------------

void TestTileId() {
    const usdgeo::TileId id{0, 3, 7};
    CHECK(id.ToString() == "L0_X3_Y7");
    CHECK((usdgeo::TileId{2, 1000, 0}).ToString() == "L2_X1000_Y0");

    CHECK(id == (usdgeo::TileId{0, 3, 7}));
    CHECK(id != (usdgeo::TileId{1, 3, 7}));
    CHECK(id != (usdgeo::TileId{0, 7, 3}));

    // Ordering is level, then row, then column: row-major within a level, so
    // an ordered walk reads the source most nearly sequentially.
    std::set<usdgeo::TileId> ordered{{0, 1, 1}, {0, 0, 1}, {0, 1, 0}, {1, 0, 0}};
    auto it = ordered.begin();
    CHECK(*it++ == (usdgeo::TileId{0, 1, 0}));
    CHECK(*it++ == (usdgeo::TileId{0, 0, 1}));
    CHECK(*it++ == (usdgeo::TileId{0, 1, 1}));
    CHECK(*it++ == (usdgeo::TileId{1, 0, 0}));

    std::unordered_set<usdgeo::TileId> hashed;
    hashed.insert({0, 3, 7});
    hashed.insert({0, 3, 7});
    hashed.insert({0, 7, 3});
    CHECK(hashed.size() == 2);

    // The coordinates must not be interchangeable in the hash, or a tile grid
    // collides along its diagonal.
    CHECK((usdgeo::TileId{0, 3, 7}).Hash() != (usdgeo::TileId{0, 7, 3}).Hash());
}

// --- CacheKey --------------------------------------------------------------

void TestCacheKeyDeterminism() {
    usdgeo::CacheKey first;
    first.AddString("terrain.tif");
    first.AddUInt(1);
    first.AddDouble(2.5);

    usdgeo::CacheKey second;
    second.AddString("terrain.tif");
    second.AddUInt(1);
    second.AddDouble(2.5);

    CHECK(first.GetDigest() == second.GetDigest());
    CHECK(first.GetHexDigest() == second.GetHexDigest());
    CHECK(first.GetHexDigest().size() == 32);
    for (char c : first.GetHexDigest()) {
        CHECK((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }
}

void TestCacheKeyFraming() {
    // The classic collision a raw-byte key permits: "ab" + "c" against
    // "a" + "bc". Length prefixing is what prevents it.
    usdgeo::CacheKey split;
    split.AddString("ab");
    split.AddString("c");

    usdgeo::CacheKey other;
    other.AddString("a");
    other.AddString("bc");
    CHECK(split.GetDigest() != other.GetDigest());

    // A type tag keeps lanes distinct: the unsigned 1 and the boolean true are
    // different requests.
    usdgeo::CacheKey asUInt;
    asUInt.AddUInt(1);
    usdgeo::CacheKey asBool;
    asBool.AddBool(true);
    CHECK(asUInt.GetDigest() != asBool.GetDigest());

    // -1 as a signed value must not collide with its unsigned reinterpretation.
    usdgeo::CacheKey negative;
    negative.AddInt(-1);
    usdgeo::CacheKey wrapped;
    wrapped.AddUInt(0xffffffffffffffffull);
    CHECK(negative.GetDigest() != wrapped.GetDigest());

    // An absent optional is not a defaulted one.
    usdgeo::CacheKey absent;
    absent.AddNull();
    usdgeo::CacheKey defaulted;
    defaulted.AddUInt(0);
    CHECK(absent.GetDigest() != defaulted.GetDigest());

    // Field order is part of the request.
    usdgeo::CacheKey forward;
    forward.AddUInt(1);
    forward.AddUInt(2);
    usdgeo::CacheKey reversed;
    reversed.AddUInt(2);
    reversed.AddUInt(1);
    CHECK(forward.GetDigest() != reversed.GetDigest());
}

void TestCacheKeyDoubleCanonicalization() {
    // Negative zero and positive zero are the same request to every consumer,
    // so they must not key to two entries.
    usdgeo::CacheKey positive;
    positive.AddDouble(0.0);
    usdgeo::CacheKey negative;
    negative.AddDouble(-0.0);
    CHECK(positive.GetDigest() == negative.GetDigest());

    // So are two NaNs with different payloads.
    usdgeo::CacheKey nan1;
    nan1.AddDouble(std::numeric_limits<double>::quiet_NaN());
    usdgeo::CacheKey nan2;
    nan2.AddDouble(-std::numeric_limits<double>::quiet_NaN());
    CHECK(nan1.GetDigest() == nan2.GetDigest());

    // Values that differ only below decimal spelling must still differ: this
    // is why the digest takes the bit pattern rather than a formatted string.
    usdgeo::CacheKey a;
    a.AddDouble(0.1);
    usdgeo::CacheKey b;
    b.AddDouble(0.1 + 1e-17);
    CHECK((a.GetDigest() == b.GetDigest()) == (0.1 == 0.1 + 1e-17));
}

// --- Diagnostics -----------------------------------------------------------

void TestDiagnosticCodeNames() {
    // Names are identifiers that appear in messages and tests, so they are
    // pinned rather than derived.
    CHECK(std::string(GetDiagnosticCodeName(
              usdgeo::DiagnosticCode::MissingGeoreference)) ==
          "MissingGeoreference");
    CHECK(std::string(GetDiagnosticCodeName(
              usdgeo::DiagnosticCode::UnknownPixelAnchor)) ==
          "UnknownPixelAnchor");
    CHECK(std::string(GetDiagnosticCodeName(usdgeo::DiagnosticCode::Cancelled)) ==
          "Cancelled");

    // Every code has a name. A code added without one is a build-time
    // omission, and this is what turns it into a test failure.
    for (int i = 0;
         i <= static_cast<int>(usdgeo::DiagnosticCode::VertexBudgetExceeded);
         ++i) {
        const auto code = static_cast<usdgeo::DiagnosticCode>(i);
        CHECK(GetDiagnosticCodeName(code)[0] != '\0');
    }
}

void TestDiagnosticSink() {
    usdgeo::DiagnosticSink sink;
    CHECK(sink.IsEmpty());
    CHECK(!sink.HasError());

    // A warning leaves a usable result, so it does not set the error flag.
    sink.AddWarning(usdgeo::DiagnosticCode::LossyConversion,
                    "vertical exaggeration 2.0 applied");
    CHECK(!sink.HasError());
    CHECK(sink.GetWarningCount() == 1);

    // A sink accumulates, so one pass reports every problem with a source
    // rather than only the first.
    sink.AddError(usdgeo::DiagnosticCode::TruncatedData, "strip 3 truncated");
    sink.AddError(usdgeo::DiagnosticCode::InvalidOffset, "IFD offset past end");
    CHECK(sink.HasError());
    CHECK(sink.GetErrorCount() == 2);
    CHECK(sink.GetDiagnostics().size() == 3);

    sink.Clear();
    CHECK(sink.IsEmpty());
    CHECK(!sink.HasError());
    CHECK(sink.GetErrorCount() == 0);
}

void TestDiagnosticAnchors() {
    // An anchor is present only when it is meaningful: a truncated header has
    // a byte offset and no pixel; a rejected window has a window and no offset.
    usdgeo::Diagnostic truncated;
    truncated.code = usdgeo::DiagnosticCode::TruncatedHeader;
    truncated.severity = usdgeo::Severity::Error;
    truncated.byteOffset = 8;
    CHECK(truncated.IsError());
    CHECK(truncated.byteOffset.has_value());
    CHECK(!truncated.window.has_value());
    CHECK(!truncated.pixelX.has_value());

    usdgeo::Diagnostic outOfBounds;
    outOfBounds.code = usdgeo::DiagnosticCode::WindowOutOfBounds;
    outOfBounds.window = usdgeo::PixelWindow{100, 100, 512, 512};
    outOfBounds.band = 1;
    CHECK(outOfBounds.window.has_value());
    CHECK(*outOfBounds.window == (usdgeo::PixelWindow{100, 100, 512, 512}));
    CHECK(!outOfBounds.byteOffset.has_value());
}

}  // namespace

int main() {
    TestBoundsAccumulation();
    TestBoundsIgnoresNonFinite();
    TestBoundsSizeOfInvalidIsZero();

    TestLocalOriginRoundTrip();
    TestLocalOriginIsWhatMakesFloatViable();
    TestLocalOriginIsStableUnderWindowChange();
    TestLocalOriginQuantum();

    TestCrsDescription();
    TestTileId();

    TestCacheKeyDeterminism();
    TestCacheKeyFraming();
    TestCacheKeyDoubleCanonicalization();

    TestDiagnosticCodeNames();
    TestDiagnosticSink();
    TestDiagnosticAnchors();

    std::printf("usdGeoCore: %d checks passed\n", g_checks);
    return 0;
}
