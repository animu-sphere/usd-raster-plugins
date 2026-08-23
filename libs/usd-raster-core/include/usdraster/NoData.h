#ifndef USDRASTER_NODATA_H
#define USDRASTER_NODATA_H

#include <optional>

namespace usdraster {

/// What an authoring path does with a cell that carries no elevation.
///
/// The policy is explicit and selected by argument, never inferred, and it is
/// recorded in authored metadata because two of the three are lossy. See
/// section 9 of docs/architecture/COORDINATE_MODEL.md.
enum class NoDataPolicy {
    /// Any quad with at least one NoData corner produces no face. The default:
    /// a hole in the terrain is what the source actually says.
    Skip,
    /// NoData cells take an explicit fill value supplied by argument.
    Fill,
    /// The raw NoData value is written as an elevation. Useful for inspection,
    /// and recorded as lossy -- a -9999 sentinel authored as a height is a
    /// ten-kilometre spike, not terrain.
    Keep
};

const char* GetNoDataPolicyName(NoDataPolicy policy);

/// The NoData value in force for a band, and the comparison against it.
///
/// **Comparison is exact.** Bit equality when the NoData value is NaN, value
/// equality otherwise. A tolerance is not applied and is not configurable: a
/// tolerance silently deletes legitimate data at its boundary, and a DEM whose
/// real elevations approach the sentinel is exactly where that happens.
///
/// The NaN case needs its own path because `NaN == NaN` is false in IEEE-754,
/// so the obvious comparison would match nothing and every NoData cell would
/// be authored as terrain.
class NoDataValue {
public:
    NoDataValue() = default;
    explicit NoDataValue(double value) : _value(value) {}

    static NoDataValue None() { return NoDataValue(); }

    bool IsSet() const { return _value.has_value(); }
    const std::optional<double>& Get() const { return _value; }

    /// True when `sample` is the NoData value. False when no NoData value is
    /// set, which is the case for a source that declares none.
    bool Matches(double sample) const;

private:
    std::optional<double> _value;
};

bool operator==(const NoDataValue& left, const NoDataValue& right);

}  // namespace usdraster

#endif  // USDRASTER_NODATA_H
