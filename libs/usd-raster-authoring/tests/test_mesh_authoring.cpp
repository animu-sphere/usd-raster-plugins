#include "usdrasterauthoring/MeshAuthoring.h"
#include "usdrasterauthoring/MetadataAuthoring.h"

#include "pxr/base/gf/vec3d.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/tf/token.h"
#include "pxr/base/vt/array.h"
#include "pxr/base/vt/value.h"
#include "pxr/usd/sdf/attributeSpec.h"
#include "pxr/usd/sdf/layer.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/usd/sdf/primSpec.h"

#include <cstdio>
#include <cstdlib>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", message);
        std::abort();
    }
}

template <typename Value>
Value GetAttribute(const SdfLayerHandle& layer, const char* path) {
    const SdfAttributeSpecHandle attribute =
        layer->GetAttributeAtPath(SdfPath(path));
    Check(static_cast<bool>(attribute), path);
    const VtValue value = attribute->GetDefaultValue();
    Check(value.IsHolding<Value>(), path);
    return value.Get<Value>();
}

}  // namespace

int main() {
    SdfLayerRefPtr layer =
        SdfLayer::CreateAnonymous("mesh-authoring-test");
    usdraster::RasterMetadata metadata;
    metadata.size = {2, 2};
    metadata.geoTransform = {1000000.25, 2.0, 0.0, 2000000.25, 0.0, -3.0};
    metadata.pixelAnchor = usdraster::PixelAnchor::Area;
    metadata.hasGeoTransform = true;
    metadata.crs.epsgCode = 32610;
    metadata.crs.linearUnit = usdgeo::LinearUnit::Metre;
    Check(usdraster::TryGetWindowBounds(
              metadata.geoTransform,
              usdraster::RasterWindow::FromSize(metadata.size),
              metadata.pixelAnchor, metadata.bounds),
          "fixture bounds");
    metadata.bands.push_back(
        {1, usdraster::RasterDataType::Float32, usdraster::NoDataValue::None()});

    usdraster::RasterGrid grid({0, 0, 2, 2}, 1, 1,
                               usdraster::RasterDataType::Float32,
                               usdraster::NoDataValue::None());
    grid.SetSample(0, 0, 10.0);
    grid.SetSample(1, 0, 20.0);
    grid.SetSample(0, 1, 30.0);
    grid.SetSample(1, 1, 40.0);

    usdgeo::DiagnosticSink diagnostics;
    Check(usdrasterauthoring::AuthorMesh(
              layer.operator->(), metadata, grid,
              usdrasterauthoring::MeshAuthoringOptions{},
              "file", "fixture.tif", &diagnostics),
          "author 2x2 mesh");
    Check(!diagnostics.HasError(), "mesh authoring diagnostics");

    const SdfPrimSpecHandle mesh = layer->GetPrimAtPath(SdfPath("/Raster"));
    Check(mesh && mesh->GetTypeName() == TfToken("Mesh"), "Raster is a Mesh");

    const VtArray<GfVec3f> points =
        GetAttribute<VtArray<GfVec3f>>(layer, "/Raster.points");
    Check(points.size() == 4, "2x2 mesh has four points");
    Check(points[0] == GfVec3f(-0.75f, -15.0f, -1.75f) &&
              points[3] == GfVec3f(1.25f, 15.0f, 1.25f),
          "points use local origin and Y-up mapping");

    const VtArray<int> counts =
        GetAttribute<VtArray<int>>(layer, "/Raster.faceVertexCounts");
    const VtArray<int> indices =
        GetAttribute<VtArray<int>>(layer, "/Raster.faceVertexIndices");
    Check(counts.size() == 1 && counts[0] == 4, "2x2 mesh has one quad");
    Check(indices.size() == 4 && indices[0] == 0 && indices[1] == 2 &&
              indices[2] == 3 && indices[3] == 1,
          "negative determinant reverses winding");

    const GfVec3d origin =
        GetAttribute<GfVec3d>(layer, "/Raster.geo:localOrigin");
    Check(origin == GfVec3d(1000002.0, 1999997.0, 25.0),
          "mesh records its local origin");
    SdfLayerRefPtr metadataLayer =
        SdfLayer::CreateAnonymous("metadata-authoring-test");
    usdgeo::DiagnosticSink metadataDiagnostics;
    Check(usdrasterauthoring::AuthorMetadata(
              metadataLayer.operator->(), metadata, 1, "file", "fixture.tif",
              &metadataDiagnostics),
          "author metadata representation");
    const GfVec3d metadataOrigin =
        GetAttribute<GfVec3d>(metadataLayer, "/Raster.geo:localOrigin");
    Check(metadataOrigin[0] == origin[0] && metadataOrigin[1] == origin[1] &&
              metadataOrigin[2] == 0.0,
          "metadata shares the quantized horizontal local origin");
    Check(GetAttribute<TfToken>(layer, "/Raster.raster:representation") ==
              TfToken("mesh"),
          "mesh conversion record");
    return 0;
}