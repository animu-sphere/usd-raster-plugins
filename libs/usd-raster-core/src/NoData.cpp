#include "usdraster/NoData.h"

#include <cmath>

namespace usdraster {

const char* GetNoDataPolicyName(NoDataPolicy policy) {
    switch (policy) {
        case NoDataPolicy::Skip: return "skip";
        case NoDataPolicy::Fill: return "fill";
        case NoDataPolicy::Keep: return "keep";
    }
    return "skip";
}

bool NoDataValue::Matches(double sample) const {
    if (!_value.has_value()) {
        return false;
    }
    const double noData = *_value;
    if (std::isnan(noData)) {
        // NaN == NaN is false, so a NaN sentinel needs its own path. Any NaN
        // matches any NaN sentinel: a payload difference between two NaNs
        // carries no meaning any producer of a DEM intends.
        return std::isnan(sample);
    }
    // Exact equality, deliberately. Note this also makes -0.0 match a 0.0
    // sentinel, which is correct: they are the same value.
    return sample == noData;
}

bool operator==(const NoDataValue& left, const NoDataValue& right) {
    if (left.IsSet() != right.IsSet()) {
        return false;
    }
    if (!left.IsSet()) {
        return true;
    }
    const double a = *left.Get();
    const double b = *right.Get();
    if (std::isnan(a) || std::isnan(b)) {
        return std::isnan(a) && std::isnan(b);
    }
    return a == b;
}

}  // namespace usdraster
