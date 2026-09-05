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
    usdraster::RasterBandInfo band{
        1, usdraster::RasterDataType::Float32,
        usdraster::NoDataValue::None()};
    band.description = "elevation";
    band.unit = "metre";
    band.scale = 0.5;
    band.offset = 100.0;
    metadata.bands.push_back(band);

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
        const VtArray<std::string> descriptions =
          GetAttribute<VtArray<std::string>>(metadataLayer,
                                 "/Raster.raster:bandDescriptions");
        Check(descriptions.size() == 1 && descriptions[0] == "elevation",
            "metadata authors band description");
        const VtArray<std::string> units =
          GetAttribute<VtArray<std::string>>(metadataLayer,
                                 "/Raster.raster:bandUnits");
        Check(units.size() == 1 && units[0] == "metre",
            "metadata authors band unit");
        const VtArray<double> scales =
          GetAttribute<VtArray<double>>(metadataLayer,
                              "/Raster.raster:bandScales");
        Check(scales.size() == 1 && scales[0] == 0.5,
            "metadata authors band scale");
        const VtArray<double> offsets =
          GetAttribute<VtArray<double>>(metadataLayer,
                              "/Raster.raster:bandOffsets");
        Check(offsets.size() == 1 && offsets[0] == 100.0,
            "metadata authors band offset");
    Check(GetAttribute<TfToken>(layer, "/Raster.raster:representation") ==
              TfToken("mesh"),
          "mesh conversion record");

    usdraster::RasterMetadata pointMetadata = metadata;
    pointMetadata.pixelAnchor = usdraster::PixelAnchor::Point;
    Check(usdraster::TryGetWindowBounds(
              pointMetadata.geoTransform,
              usdraster::RasterWindow::FromSize(pointMetadata.size),
              pointMetadata.pixelAnchor, pointMetadata.bounds),
          "point fixture bounds");
    SdfLayerRefPtr pointLayer =
        SdfLayer::CreateAnonymous("mesh-pixel-is-point-test");
    usdgeo::DiagnosticSink pointDiagnostics;
    Check(usdrasterauthoring::AuthorMesh(
              pointLayer.operator->(), pointMetadata, grid,
              usdrasterauthoring::MeshAuthoringOptions{}, "point",
              "point.tif", &pointDiagnostics),
          "author pixel-is-point mesh");
    const VtArray<GfVec3f> pointPositions =
        GetAttribute<VtArray<GfVec3f>>(pointLayer, "/Raster.points");
    Check(pointPositions.size() == 4 &&
              pointPositions[0] == GfVec3f(-0.75f, -15.0f, -1.25f) &&
              pointPositions[3] == GfVec3f(1.25f, 15.0f, 1.75f),
          "pixel-is-point uses sample coordinates without a half-pixel shift");
    Check(GetAttribute<GfVec3d>(pointLayer, "/Raster.geo:localOrigin") ==
              GfVec3d(1000001.0, 1999999.0, 25.0),
          "pixel-is-point records its quantized local origin");
    Check(GetAttribute<GfVec3d>(pointLayer, "/Raster.geo:boundsMin") ==
              GfVec3d(1000000.25, 1999997.25, 10.0) &&
              GetAttribute<GfVec3d>(pointLayer, "/Raster.geo:boundsMax") ==
                  GfVec3d(1000002.25, 2000000.25, 40.0),
          "pixel-is-point records sample bounds");

    usdraster::RasterMetadata noDataMetadata;
    noDataMetadata.size = {2, 2};
    noDataMetadata.geoTransform = {0.0, 1.0, 0.0, 0.0, 0.0, -1.0};
    noDataMetadata.pixelAnchor = usdraster::PixelAnchor::Point;
    noDataMetadata.hasGeoTransform = true;
    Check(usdraster::TryGetWindowBounds(
              noDataMetadata.geoTransform,
              usdraster::RasterWindow::FromSize(noDataMetadata.size),
              noDataMetadata.pixelAnchor, noDataMetadata.bounds),
          "NoData fixture bounds");
    usdraster::RasterBandInfo noDataBand{
        1, usdraster::RasterDataType::Float32,
        usdraster::NoDataValue(-9999.0)};
    noDataMetadata.bands.push_back(noDataBand);
    usdraster::RasterGrid noDataGrid(
        {0, 0, 2, 2}, 1, 1, usdraster::RasterDataType::Float32,
        usdraster::NoDataValue(-9999.0));
    noDataGrid.SetSample(0, 0, 1.0);
    noDataGrid.SetSample(1, 0, 2.0);
    noDataGrid.SetSample(0, 1, 3.0);
    noDataGrid.SetSample(1, 1, -9999.0);

    SdfLayerRefPtr fillLayer = SdfLayer::CreateAnonymous("mesh-fill-test");
    usdrasterauthoring::MeshAuthoringOptions fillOptions;
    fillOptions.heightScale = 2.0;
    fillOptions.noDataPolicy = usdraster::NoDataPolicy::Fill;
    fillOptions.fillValue = 7.0;
    usdgeo::DiagnosticSink fillDiagnostics;
    Check(usdrasterauthoring::AuthorMesh(
              fillLayer.operator->(), noDataMetadata, noDataGrid, fillOptions,
              "file", "nodata.tif", &fillDiagnostics),
          "author filled NoData mesh");
    Check(GetAttribute<VtArray<int>>(fillLayer, "/Raster.faceVertexCounts").size() == 1,
          "fill policy keeps the quad");
    Check(GetAttribute<TfToken>(fillLayer, "/Raster.raster:noDataPolicy") ==
              TfToken("fill"),
          "fill policy is authored");
    Check(GetAttribute<double>(fillLayer, "/Raster.raster:heightScale") == 2.0 &&
              GetAttribute<double>(fillLayer, "/Raster.raster:fillValue") == 7.0,
          "mesh height and fill arguments are authored");

    SdfLayerRefPtr skipLayer = SdfLayer::CreateAnonymous("mesh-skip-test");
    usdrasterauthoring::MeshAuthoringOptions skipOptions;
    usdgeo::DiagnosticSink skipDiagnostics;
    Check(usdrasterauthoring::AuthorMesh(
              skipLayer.operator->(), noDataMetadata, noDataGrid, skipOptions,
              "file", "nodata.tif", &skipDiagnostics),
          "author skipped NoData mesh");
    Check(GetAttribute<VtArray<int>>(skipLayer, "/Raster.faceVertexCounts").empty(),
          "skip policy removes NoData quad");

    SdfLayerRefPtr keepLayer = SdfLayer::CreateAnonymous("mesh-keep-test");
    usdrasterauthoring::MeshAuthoringOptions keepOptions;
    keepOptions.noDataPolicy = usdraster::NoDataPolicy::Keep;
    usdgeo::DiagnosticSink keepDiagnostics;
    Check(usdrasterauthoring::AuthorMesh(
              keepLayer.operator->(), noDataMetadata, noDataGrid, keepOptions,
              "file", "nodata.tif", &keepDiagnostics),
          "author kept NoData mesh");
    Check(GetAttribute<VtArray<int>>(keepLayer, "/Raster.faceVertexCounts").size() == 1 &&
              GetAttribute<TfToken>(keepLayer, "/Raster.raster:noDataPolicy") ==
                  TfToken("keep"),
          "keep policy retains and records the NoData quad");
    return 0;
}