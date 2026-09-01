#include "usdrasterauthoring/MetadataAuthoring.h"

#include "usdgeo/LocalOrigin.h"

#include "pxr/base/tf/token.h"
#include "pxr/base/vt/array.h"
#include "pxr/base/vt/value.h"
#include "pxr/usd/sdf/attributeSpec.h"
#include "pxr/usd/sdf/layer.h"
#include "pxr/usd/sdf/primSpec.h"
#include "pxr/usd/sdf/types.h"

#include <string>
#include <utility>

PXR_NAMESPACE_USING_DIRECTIVE

namespace usdrasterauthoring {
namespace {

template <typename Value>
bool AddAttribute(const SdfPrimSpecHandle& prim, const char* name,
                  const SdfValueTypeName& type, const Value& value,
                  usdgeo::DiagnosticSink& diagnostics) {
    const SdfAttributeSpecHandle attribute =
        SdfAttributeSpec::New(prim, name, type, SdfVariabilityVarying, true);
    if (!attribute || !attribute->SetDefaultValue(VtValue(value))) {
        diagnostics.AddError(usdgeo::DiagnosticCode::AuthoringFailed,
                             std::string("unable to author attribute ") + name);
        return false;
    }
    return true;
}

VtArray<double> TransformValues(const usdraster::RasterGeoTransform& transform) {
    VtArray<double> values(6);
    values[0] = transform.a0;
    values[1] = transform.a1;
    values[2] = transform.a2;
    values[3] = transform.b0;
    values[4] = transform.b1;
    values[5] = transform.b2;
    return values;
}

}  // namespace

bool AuthorMetadata(SdfLayer* layer, const usdraster::RasterMetadata& metadata,
                    std::uint32_t selectedBand,
                    const std::string& pixelAnchorSource,
                    const std::string& sourceIdentifier,
                    usdgeo::DiagnosticSink* diagnostics) {
    if (!layer || !diagnostics || !metadata.hasGeoTransform ||
        !metadata.bounds.IsValid()) {
        if (diagnostics) {
            diagnostics->AddError(usdgeo::DiagnosticCode::AuthoringFailed,
                                  "metadata authoring requires a valid georeferenced raster");
        }
        return false;
    }
    const usdraster::RasterBandInfo* selected = metadata.FindBand(selectedBand);
    if (!selected) {
        diagnostics->AddError(usdgeo::DiagnosticCode::InvalidBandIndex,
                              "selected band is not present in the source");
        return false;
    }

    const SdfPrimSpecHandle prim =
        SdfPrimSpec::New(SdfLayerHandle(layer), "Raster", SdfSpecifierDef,
                         "Scope");
    if (!prim) {
        diagnostics->AddError(usdgeo::DiagnosticCode::AuthoringFailed,
                              "unable to create /Raster Scope");
        return false;
    }

    bool ok = true;
    ok = AddAttribute(prim, "geo:sourceIdentifier", SdfValueTypeNames->String,
                      sourceIdentifier, *diagnostics) && ok;
    if (metadata.crs.epsgCode.has_value()) {
        ok = AddAttribute(prim, "geo:crs", SdfValueTypeNames->String,
                          std::string("EPSG:") + std::to_string(*metadata.crs.epsgCode),
                          *diagnostics) && ok;
    } else if (!metadata.crs.wkt.empty()) {
        ok = AddAttribute(prim, "geo:crs", SdfValueTypeNames->String,
                          metadata.crs.wkt, *diagnostics) && ok;
    }
    if (!metadata.crs.wkt.empty()) {
        ok = AddAttribute(prim, "geo:crsWkt", SdfValueTypeNames->String,
                          metadata.crs.wkt, *diagnostics) && ok;
    }
    if (metadata.crs.linearUnit != usdgeo::LinearUnit::Unknown) {
        ok = AddAttribute(prim, "geo:linearUnit", SdfValueTypeNames->String,
                          std::string(usdgeo::GetLinearUnitName(metadata.crs.linearUnit)),
                          *diagnostics) && ok;
    }
    if (metadata.crs.verticalUnit != usdgeo::LinearUnit::Unknown) {
        ok = AddAttribute(prim, "geo:verticalUnit", SdfValueTypeNames->String,
                          std::string(usdgeo::GetLinearUnitName(metadata.crs.verticalUnit)),
                          *diagnostics) && ok;
    }

    const usdgeo::Vec3d origin =
        usdgeo::LocalOrigin::FromBounds(metadata.bounds).GetValue();
    ok = AddAttribute(prim, "geo:localOrigin", SdfValueTypeNames->Double3,
                      GfVec3d(origin.x, origin.y, origin.z), *diagnostics) && ok;
    ok = AddAttribute(prim, "geo:boundsMin", SdfValueTypeNames->Double3,
                      GfVec3d(metadata.bounds.min.x, metadata.bounds.min.y,
                              metadata.bounds.min.z), *diagnostics) && ok;
    ok = AddAttribute(prim, "geo:boundsMax", SdfValueTypeNames->Double3,
                      GfVec3d(metadata.bounds.max.x, metadata.bounds.max.y,
                              metadata.bounds.max.z), *diagnostics) && ok;

    ok = AddAttribute(prim, "raster:width", SdfValueTypeNames->UInt64,
                      metadata.size.width, *diagnostics) && ok;
    ok = AddAttribute(prim, "raster:height", SdfValueTypeNames->UInt64,
                      metadata.size.height, *diagnostics) && ok;
    ok = AddAttribute(prim, "raster:bandCount", SdfValueTypeNames->UInt,
                      metadata.GetBandCount(), *diagnostics) && ok;
    ok = AddAttribute(prim, "raster:geoTransform", SdfValueTypeNames->DoubleArray,
                      TransformValues(metadata.geoTransform), *diagnostics) && ok;
    ok = AddAttribute(prim, "raster:pixelAnchor", SdfValueTypeNames->Token,
                      TfToken(usdraster::GetPixelAnchorName(metadata.pixelAnchor)),
                      *diagnostics) && ok;
    ok = AddAttribute(prim, "raster:pixelAnchorSource", SdfValueTypeNames->Token,
                      TfToken(pixelAnchorSource), *diagnostics) && ok;
    ok = AddAttribute(prim, "raster:dataType", SdfValueTypeNames->Token,
                      TfToken(usdraster::GetDataTypeName(selected->dataType)),
                      *diagnostics) && ok;
    ok = AddAttribute(prim, "raster:selectedBand", SdfValueTypeNames->UInt,
                      selectedBand, *diagnostics) && ok;
    ok = AddAttribute(prim, "raster:overviewCount", SdfValueTypeNames->UInt,
                      static_cast<std::uint32_t>(metadata.overviewSizes.size()),
                      *diagnostics) && ok;

    VtArray<unsigned int> bandIndices;
    VtArray<TfToken> bandTypes;
    VtArray<std::string> bandDescriptions;
    VtArray<std::string> bandUnits;
    VtArray<double> bandNoDataValues;
    bool hasDescriptions = false;
    bool hasUnits = false;
    bool everyBandHasNoData = !metadata.bands.empty();
    for (const usdraster::RasterBandInfo& band : metadata.bands) {
        bandIndices.push_back(band.index);
        bandTypes.push_back(TfToken(usdraster::GetDataTypeName(band.dataType)));
        bandDescriptions.push_back(band.description);
        bandUnits.push_back(band.unit);
        hasDescriptions = hasDescriptions || !band.description.empty();
        hasUnits = hasUnits || !band.unit.empty();
        if (band.noData.IsSet()) {
            bandNoDataValues.push_back(*band.noData.Get());
        } else {
            everyBandHasNoData = false;
        }
    }
    ok = AddAttribute(prim, "raster:bandIndices", SdfValueTypeNames->UIntArray,
                      bandIndices, *diagnostics) && ok;
    ok = AddAttribute(prim, "raster:bandDataTypes", SdfValueTypeNames->TokenArray,
                      bandTypes, *diagnostics) && ok;
    if (hasDescriptions) {
        ok = AddAttribute(prim, "raster:bandDescriptions",
                          SdfValueTypeNames->StringArray, bandDescriptions,
                          *diagnostics) && ok;
    }
    if (hasUnits) {
        ok = AddAttribute(prim, "raster:bandUnits", SdfValueTypeNames->StringArray,
                          bandUnits, *diagnostics) && ok;
    }
    if (everyBandHasNoData) {
        ok = AddAttribute(prim, "raster:bandNoDataValues",
                          SdfValueTypeNames->DoubleArray, bandNoDataValues,
                          *diagnostics) && ok;
    }
    if (selected->noData.IsSet()) {
        ok = AddAttribute(prim, "raster:noDataValue", SdfValueTypeNames->Double,
                          *selected->noData.Get(), *diagnostics) && ok;
    }
    if (metadata.nativeTileSize.has_value()) {
        ok = AddAttribute(prim, "raster:nativeTileWidth", SdfValueTypeNames->UInt,
                          static_cast<std::uint32_t>(metadata.nativeTileSize->width),
                          *diagnostics) && ok;
        ok = AddAttribute(prim, "raster:nativeTileHeight", SdfValueTypeNames->UInt,
                          static_cast<std::uint32_t>(metadata.nativeTileSize->height),
                          *diagnostics) && ok;
    }
    return ok && !diagnostics->HasError();
}

}  // namespace usdrasterauthoring