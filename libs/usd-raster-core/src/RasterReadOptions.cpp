#include "usdraster/RasterReadOptions.h"

namespace usdraster {

void RasterReadOptions::ContributeToCacheKey(usdgeo::CacheKey& key) const {
    key.AddUInt(band);
    if (overviewLevel.has_value()) {
        key.AddUInt(*overviewLevel);
    } else {
        // Distinct from any explicit level: "let the reader choose" is a
        // different request from "use level 0", even when they resolve to the
        // same level today.
        key.AddNull();
    }
    key.AddUInt(samplingStep);
    // The type's stable NAME, not its enumerator ordinal. `RasterTypes.h`
    // anticipates new enumerators -- sub-byte sample formats enter when a
    // format needs them -- and inserting one anywhere but the end would
    // renumber the values after it. Every cache entry keyed by the old
    // ordinal would then silently resolve to a different output type, which
    // is a wrong-data bug that no test of the new format would catch.
    key.AddString(GetDataTypeName(outputType));
}

}  // namespace usdraster
