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
    key.AddUInt(static_cast<std::uint64_t>(outputType));
}

}  // namespace usdraster
