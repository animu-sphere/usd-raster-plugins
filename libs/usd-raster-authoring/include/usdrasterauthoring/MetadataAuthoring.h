#ifndef USDRASTERAUTHORING_METADATAAUTHORING_H
#define USDRASTERAUTHORING_METADATAAUTHORING_H

#include "pxr/pxr.h"

#include <usdraster/RasterMetadata.h>

#include <usdgeo/Diagnostic.h>

#include <cstdint>
#include <string>

PXR_NAMESPACE_OPEN_SCOPE
class SdfLayer;
PXR_NAMESPACE_CLOSE_SCOPE

namespace usdrasterauthoring {

/// Authors the metadata-only representation as a /Raster Scope.
///
/// No pixel data is accessed by this entry point. Source coordinates remain
/// in double precision in authored metadata; the metadata representation has
/// no stage-local point buffer to narrow.
bool AuthorMetadata(pxr::SdfLayer* layer,
                    const usdraster::RasterMetadata& metadata,
                    std::uint32_t selectedBand,
                    const std::string& pixelAnchorSource,
                    const std::string& sourceIdentifier,
                    usdgeo::DiagnosticSink* diagnostics);

}  // namespace usdrasterauthoring

#endif  // USDRASTERAUTHORING_METADATAAUTHORING_H