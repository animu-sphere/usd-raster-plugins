#ifndef USDGEO_CRSDESCRIPTION_H
#define USDGEO_CRSDESCRIPTION_H

#include <optional>
#include <string>

namespace usdgeo {

enum class CrsKind {
    Unknown,
    Projected,   ///< X east, Y north, linear units
    Geographic   ///< longitude, latitude, angular units
};

enum class LinearUnit {
    Unknown,
    Metre,
    Foot,          ///< international foot, exactly 0.3048 m
    USSurveyFoot   ///< 1200/3937 m; differs from the international foot in the
                   ///< eighth decimal place, which matters at state-plane
                   ///< eastings
};

enum class AngularUnit { Unknown, Degree, Radian };

/// Metres per unit, or zero when the unit is unknown.
double GetMetresPerUnit(LinearUnit unit);

const char* GetLinearUnitName(LinearUnit unit);
const char* GetAngularUnitName(AngularUnit unit);
const char* GetCrsKindName(CrsKind kind);

/// What the source said about its coordinate reference system.
///
/// Every field is optional and every field is preserved as read. This
/// repository reads, preserves, and reports CRS values; it does not reproject,
/// and it does not transform vertical datums. Interpretation beyond the affine
/// mapping belongs to the caller or to a future explicit projection step. See
/// section 11 of docs/architecture/COORDINATE_MODEL.md and
/// ADR-0007 for why PROJ and GDAL are not sitting behind this type.
///
/// Unknown metadata is preserved or reported, never silently discarded; that
/// is invariant 7 of the workspace contract. `wkt` and `citation` exist so a
/// CRS this code cannot classify still survives to authored metadata.
struct CrsDescription {
    CrsKind kind = CrsKind::Unknown;

    /// The horizontal CRS EPSG code, when the source named one.
    std::optional<int> epsgCode;

    /// The horizontal CRS as WKT, when the source carried it. Preserved
    /// verbatim -- not normalized, not reformatted -- because a round-trip
    /// through a parser this repository does not own would lose whatever the
    /// parser did not model.
    std::string wkt;

    /// Human-readable identification from the source, when present.
    std::string citation;

    LinearUnit linearUnit = LinearUnit::Unknown;

    /// Metres per horizontal unit as the source stated it. Kept separately
    /// from `linearUnit` because a source may define a unit by its conversion
    /// factor alone, with no code this enumeration covers. Zero when absent.
    double linearUnitMetres = 0.0;

    AngularUnit angularUnit = AngularUnit::Unknown;

    std::optional<int> verticalEpsgCode;
    LinearUnit verticalUnit = LinearUnit::Unknown;
    std::string verticalCitation;

    bool IsEmpty() const;

    /// True when the horizontal coordinates are linear and their unit is
    /// known. A geographic CRS is not metric, and authoring one into a metric
    /// stage requires an explicit, recorded choice rather than treating
    /// degrees as metres. See section 7 of the coordinate contract.
    bool IsMetric() const;

    /// True when the vertical unit is known and differs from the horizontal
    /// unit. Such a source is never silently mixed: the units are recorded,
    /// and a conversion, if requested, is explicit.
    bool HasMixedUnits() const;
};

bool operator==(const CrsDescription& left, const CrsDescription& right);

}  // namespace usdgeo

#endif  // USDGEO_CRSDESCRIPTION_H
