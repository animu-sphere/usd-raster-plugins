#ifndef USDRASTERAUTHORING_MESHAUTHORING_H
#define USDRASTERAUTHORING_MESHAUTHORING_H

#include "pxr/pxr.h"

#include <usdraster/RasterGrid.h>
#include <usdraster/RasterMetadata.h>

#include <usdgeo/Diagnostic.h>

#include <string>

PXR_NAMESPACE_OPEN_SCOPE
class SdfLayer;
PXR_NAMESPACE_CLOSE_SCOPE

namespace usdrasterauthoring {

struct MeshAuthoringOptions {
    double heightScale = 1.0;
    double heightOffset = 0.0;
    usdraster::NoDataPolicy noDataPolicy = usdraster::NoDataPolicy::Skip;
    double fillValue = 0.0;
};

/// Authors a regular-grid Mesh at /Raster from one decoded scalar band.
bool AuthorMesh(pxr::SdfLayer* layer,
                const usdraster::RasterMetadata& metadata,
                const usdraster::RasterGrid& grid,
                const MeshAuthoringOptions& options,
                const std::string& pixelAnchorSource,
                const std::string& sourceIdentifier,
                usdgeo::DiagnosticSink* diagnostics);

}  // namespace usdrasterauthoring

#endif  // USDRASTERAUTHORING_MESHAUTHORING_H