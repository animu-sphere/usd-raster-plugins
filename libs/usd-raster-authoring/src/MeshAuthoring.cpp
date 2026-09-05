#include "usdrasterauthoring/MeshAuthoring.h"

#include "usdrasterauthoring/MetadataAuthoring.h"

#include "pxr/base/gf/vec3f.h"
#include "pxr/base/tf/token.h"
#include "pxr/base/vt/array.h"
#include "pxr/base/vt/value.h"
#include "pxr/usd/sdf/attributeSpec.h"
#include "pxr/usd/sdf/layer.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/usd/sdf/primSpec.h"
#include "pxr/usd/sdf/types.h"

#include <usdgeo/LocalOrigin.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usdrasterauthoring {
namespace {

template <typename Value>
bool AddAttribute(const SdfPrimSpecHandle& prim, const char* name,
                  const SdfValueTypeName& type, const Value& value,
                  usdgeo::DiagnosticSink& diagnostics) {
    SdfAttributeSpecHandle attribute = prim->GetLayer()->GetAttributeAtPath(
        prim->GetPath().AppendProperty(TfToken(name)));
    if (!attribute) {
        attribute = SdfAttributeSpec::New(prim, name, type,
                                          SdfVariabilityVarying, true);
    }
    if (!attribute || !attribute->SetDefaultValue(VtValue(value))) {
        diagnostics.AddError(usdgeo::DiagnosticCode::AuthoringFailed,
                             std::string("unable to author attribute ") + name);
        return false;
    }
    return true;
}

struct SourceVertex {
    usdgeo::Vec3d position;
    bool noData = false;
};

bool AddAuthoringError(usdgeo::DiagnosticSink& diagnostics,
                       const std::string& message) {
    diagnostics.AddError(usdgeo::DiagnosticCode::AuthoringFailed, message);
    return false;
}

}  // namespace

bool AuthorMesh(SdfLayer* layer, const usdraster::RasterMetadata& metadata,
                const usdraster::RasterGrid& grid,
                const MeshAuthoringOptions& options,
                const std::string& pixelAnchorSource,
                const std::string& sourceIdentifier,
                usdgeo::DiagnosticSink* diagnostics) {
    if (!layer || !diagnostics || !metadata.hasGeoTransform ||
        !metadata.bounds.IsValid() ||
        metadata.pixelAnchor == usdraster::PixelAnchor::Unknown) {
        if (diagnostics) {
            diagnostics->AddError(usdgeo::DiagnosticCode::AuthoringFailed,
                                  "mesh authoring requires a valid georeferenced raster");
        }
        return false;
    }
    if (!metadata.FindBand(grid.GetBand())) {
        return AddAuthoringError(*diagnostics,
                                 "mesh grid band is not present in the source");
    }
    const usdraster::RasterSize& size = grid.GetSize();
    if (grid.IsEmpty() || size.width == 0 || size.height == 0) {
        return AddAuthoringError(*diagnostics,
                                 "mesh authoring requires a non-empty raster grid");
    }

    std::vector<SourceVertex> sourceVertices;
    sourceVertices.reserve(grid.GetSampleCount());
    double minimumElevation = std::numeric_limits<double>::infinity();
    double maximumElevation = -std::numeric_limits<double>::infinity();
    bool hasElevation = false;
    for (std::uint64_t row = 0; row < size.height; ++row) {
        for (std::uint64_t column = 0; column < size.width; ++column) {
            std::uint64_t sourceColumn = 0;
            std::uint64_t sourceRow = 0;
            grid.GetSourcePixel(column, row, sourceColumn, sourceRow);
            double sourceX = 0.0;
            double sourceY = 0.0;
            if (sourceColumn >= metadata.size.width || sourceRow >= metadata.size.height ||
                !metadata.geoTransform.TryPixelToSource(
                    sourceColumn, sourceRow, metadata.pixelAnchor, sourceX, sourceY)) {
                return AddAuthoringError(*diagnostics,
                                         "mesh grid contains a pixel outside its source raster");
            }

            const bool noData = grid.IsNoData(column, row);
            double elevation = grid.GetSample(column, row);
            if (noData && options.noDataPolicy == usdraster::NoDataPolicy::Fill) {
                elevation = options.fillValue;
            } else if (noData && options.noDataPolicy == usdraster::NoDataPolicy::Skip) {
                elevation = 0.0;
            }
            elevation = options.heightScale * elevation + options.heightOffset;
            if (!noData || options.noDataPolicy != usdraster::NoDataPolicy::Skip) {
                minimumElevation = (std::min)(minimumElevation, elevation);
                maximumElevation = (std::max)(maximumElevation, elevation);
                hasElevation = true;
            }
            sourceVertices.push_back({{sourceX, sourceY, elevation}, noData});
        }
    }

    usdgeo::GeoBounds sourceBounds = metadata.bounds;
    if (hasElevation) {
        sourceBounds.min.z = minimumElevation;
        sourceBounds.max.z = maximumElevation;
    }
    const usdgeo::LocalOrigin origin = usdgeo::LocalOrigin::FromBounds(sourceBounds);

    VtArray<GfVec3f> points;
    points.reserve(sourceVertices.size());
    GfVec3f extentMin(std::numeric_limits<float>::infinity());
    GfVec3f extentMax(-std::numeric_limits<float>::infinity());
    for (const SourceVertex& vertex : sourceVertices) {
        const usdgeo::Vec3d local = origin.ToStageLocal(vertex.position);
        const GfVec3f point(static_cast<float>(local.x), static_cast<float>(local.z),
                            static_cast<float>(-local.y));
        points.push_back(point);
        for (int axis = 0; axis != 3; ++axis) {
            extentMin[axis] = (std::min)(extentMin[axis], point[axis]);
            extentMax[axis] = (std::max)(extentMax[axis], point[axis]);
        }
    }

    VtArray<int> faceVertexCounts;
    VtArray<int> faceVertexIndices;
    const bool reverseWinding = metadata.geoTransform.GetDeterminant() < 0.0;
    for (std::uint64_t row = 0; row + 1 < size.height; ++row) {
        for (std::uint64_t column = 0; column + 1 < size.width; ++column) {
            const std::uint64_t topLeft = row * size.width + column;
            const std::uint64_t topRight = topLeft + 1;
            const std::uint64_t bottomLeft = topLeft + size.width;
            const std::uint64_t bottomRight = bottomLeft + 1;
            if (options.noDataPolicy == usdraster::NoDataPolicy::Skip &&
                (sourceVertices[topLeft].noData || sourceVertices[topRight].noData ||
                 sourceVertices[bottomLeft].noData || sourceVertices[bottomRight].noData)) {
                continue;
            }
            if (bottomRight > static_cast<std::uint64_t>((std::numeric_limits<int>::max)())) {
                return AddAuthoringError(*diagnostics,
                                         "mesh vertex index exceeds OpenUSD integer range");
            }
            faceVertexCounts.push_back(4);
            if (reverseWinding) {
                faceVertexIndices.push_back(static_cast<int>(topLeft));
                faceVertexIndices.push_back(static_cast<int>(bottomLeft));
                faceVertexIndices.push_back(static_cast<int>(bottomRight));
                faceVertexIndices.push_back(static_cast<int>(topRight));
            } else {
                faceVertexIndices.push_back(static_cast<int>(topLeft));
                faceVertexIndices.push_back(static_cast<int>(topRight));
                faceVertexIndices.push_back(static_cast<int>(bottomRight));
                faceVertexIndices.push_back(static_cast<int>(bottomLeft));
            }
        }
    }

    if (!AuthorMetadata(layer, metadata, grid.GetBand(), pixelAnchorSource,
                        sourceIdentifier, diagnostics)) {
        return false;
    }
    const SdfPrimSpecHandle prim = layer->GetPrimAtPath(SdfPath("/Raster"));
    if (!prim) {
        return AddAuthoringError(*diagnostics, "unable to retrieve /Raster Mesh");
    }
    prim->SetTypeName(TfToken("Mesh"));

    bool ok = true;
    ok = AddAttribute(prim, "geo:localOrigin", SdfValueTypeNames->Double3,
                      GfVec3d(origin.GetValue().x, origin.GetValue().y,
                              origin.GetValue().z), *diagnostics) && ok;
    ok = AddAttribute(prim, "geo:boundsMin", SdfValueTypeNames->Double3,
                      GfVec3d(sourceBounds.min.x, sourceBounds.min.y,
                              sourceBounds.min.z), *diagnostics) && ok;
    ok = AddAttribute(prim, "geo:boundsMax", SdfValueTypeNames->Double3,
                      GfVec3d(sourceBounds.max.x, sourceBounds.max.y,
                              sourceBounds.max.z), *diagnostics) && ok;
    ok = AddAttribute(prim, "points", SdfValueTypeNames->Point3fArray,
                      points, *diagnostics) && ok;
    ok = AddAttribute(prim, "faceVertexCounts", SdfValueTypeNames->IntArray,
                      faceVertexCounts, *diagnostics) && ok;
    ok = AddAttribute(prim, "faceVertexIndices", SdfValueTypeNames->IntArray,
                      faceVertexIndices, *diagnostics) && ok;
    ok = AddAttribute(prim, "orientation", SdfValueTypeNames->Token,
                      TfToken("rightHanded"), *diagnostics) && ok;
    ok = AddAttribute(prim, "subdivisionScheme", SdfValueTypeNames->Token,
                      TfToken("none"), *diagnostics) && ok;
    VtArray<GfVec3f> extent;
    extent.push_back(extentMin);
    extent.push_back(extentMax);
    ok = AddAttribute(prim, "extent", SdfValueTypeNames->Float3Array,
                      extent, *diagnostics) && ok;
    ok = AddAttribute(prim, "raster:representation", SdfValueTypeNames->Token,
                      TfToken("mesh"), *diagnostics) && ok;
    ok = AddAttribute(prim, "raster:samplingStep", SdfValueTypeNames->UInt,
                      static_cast<std::uint32_t>(grid.GetSamplingStep()),
                      *diagnostics) && ok;
    ok = AddAttribute(prim, "raster:heightScale", SdfValueTypeNames->Double,
                      options.heightScale, *diagnostics) && ok;
    ok = AddAttribute(prim, "raster:heightOffset", SdfValueTypeNames->Double,
                      options.heightOffset, *diagnostics) && ok;
    ok = AddAttribute(prim, "raster:noDataPolicy", SdfValueTypeNames->Token,
                      TfToken(usdraster::GetNoDataPolicyName(options.noDataPolicy)),
                      *diagnostics) && ok;
    if (options.noDataPolicy == usdraster::NoDataPolicy::Fill) {
        ok = AddAttribute(prim, "raster:fillValue", SdfValueTypeNames->Double,
                          options.fillValue, *diagnostics) && ok;
    }
    return ok && !diagnostics->HasError();
}

}  // namespace usdrasterauthoring