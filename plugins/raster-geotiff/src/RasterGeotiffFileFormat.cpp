// SPDX-License-Identifier: Apache-2.0
#include "RasterGeotiffFileFormat.h"

#include "usdrasterauthoring/MeshAuthoring.h"
#include "usdrasterauthoring/MetadataAuthoring.h"

#include <usdgeotiff/GeoTiffReader.h>
#include <usdraster/RasterGeoTransform.h>

#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/registryManager.h"
#include "pxr/base/tf/type.h"
#include "pxr/usd/ar/asset.h"
#include "pxr/usd/ar/resolvedPath.h"
#include "pxr/usd/ar/resolver.h"
#include "pxr/usd/sdf/layer.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>

PXR_NAMESPACE_OPEN_SCOPE

namespace {

std::string SafeSourceLabel(const std::string& identifier) {
    const std::size_t query = identifier.find_first_of("?#");
    const std::string withoutQuery = identifier.substr(0, query);
    const std::size_t separator = withoutQuery.find_last_of("/\\");
    const std::string label = separator == std::string::npos
                                  ? withoutQuery
                                  : withoutQuery.substr(separator + 1);
    return label.empty() ? "raster asset" : label;
}

class ArAssetSource final : public usdraster::RandomAccessSource {
public:
    ArAssetSource(std::shared_ptr<ArAsset> asset, std::string identifier)
        : _asset(std::move(asset)), _identifier(std::move(identifier)) {}

    std::uint64_t GetSize() const override {
        return _asset ? static_cast<std::uint64_t>(_asset->GetSize()) : 0;
    }

    usdraster::ReadResult Read(std::uint64_t offset, std::size_t size,
                               void* destination) override {
        if (!_asset || offset > GetSize() || size > GetSize() - offset) {
            return usdraster::ReadResult::End();
        }
        if (size == 0) return usdraster::ReadResult::Ok(0);
        const std::size_t bytes = _asset->Read(destination, size,
                                                static_cast<size_t>(offset));
        if (bytes == size) return usdraster::ReadResult::Ok(bytes);
        return bytes == 0 ? usdraster::ReadResult::Failed()
                          : usdraster::ReadResult::Short(bytes);
    }

    std::string GetIdentifier() const override { return _identifier; }

private:
    std::shared_ptr<ArAsset> _asset;
    std::string _identifier;
};

struct RasterArguments {
    std::string representation = "metadata";
    usdraster::PixelAnchor pixelAnchor = usdraster::PixelAnchor::Unknown;
};

bool ParseArguments(const SdfFileFormat::FileFormatArguments& values,
                    RasterArguments& result, usdgeo::DiagnosticSink& diagnostics) {
    for (const auto& argument : values) {
        if (argument.first == "representation") {
            result.representation = argument.second;
            if (result.representation != "metadata" &&
                result.representation != "mesh") {
                diagnostics.AddError(usdgeo::DiagnosticCode::UnsupportedFormatArgument,
                                     "representation must be metadata or mesh");
                return false;
            }
        } else if (argument.first == "pixelAnchor") {
            if (argument.second == "area") {
                result.pixelAnchor = usdraster::PixelAnchor::Area;
            } else if (argument.second == "point") {
                result.pixelAnchor = usdraster::PixelAnchor::Point;
            } else {
                diagnostics.AddError(usdgeo::DiagnosticCode::InvalidFormatArgument,
                                     "pixelAnchor must be area or point");
                return false;
            }
        } else {
            diagnostics.AddError(usdgeo::DiagnosticCode::UnknownFormatArgument,
                                 "unknown file-format argument: " + argument.first);
            return false;
        }
    }
    return true;
}

std::string PluginCode(usdgeo::DiagnosticCode code) {
    switch (code) {
        case usdgeo::DiagnosticCode::InvalidSignature:
        case usdgeo::DiagnosticCode::TruncatedHeader: return "GTIF001";
        case usdgeo::DiagnosticCode::UnsupportedVersion: return "GTIF002";
        case usdgeo::DiagnosticCode::InvalidOffset:
        case usdgeo::DiagnosticCode::TruncatedData: return "GTIF003";
        case usdgeo::DiagnosticCode::UnsupportedCompression: return "GTIF004";
        case usdgeo::DiagnosticCode::UnsupportedSampleFormat:
        case usdgeo::DiagnosticCode::UnsupportedPlanarConfiguration: return "GTIF005";
        case usdgeo::DiagnosticCode::MissingGeoreference: return "GTIF006";
        case usdgeo::DiagnosticCode::ConflictingGeoTransform: return "GTIF007";
        case usdgeo::DiagnosticCode::InvalidGeoTransform: return "GTIF008";
        case usdgeo::DiagnosticCode::UnknownPixelAnchor: return "GTIF009";
        case usdgeo::DiagnosticCode::InvalidBandIndex: return "GTIF010";
        case usdgeo::DiagnosticCode::UnknownFormatArgument:
        case usdgeo::DiagnosticCode::UnsupportedFormatArgument:
        case usdgeo::DiagnosticCode::InvalidFormatArgument:
        case usdgeo::DiagnosticCode::ConflictingFormatArguments: return "GTIF013";
        case usdgeo::DiagnosticCode::ShortRead:
        case usdgeo::DiagnosticCode::SourceUnavailable: return "GTIF014";
        case usdgeo::DiagnosticCode::MemoryBudgetExceeded: return "GTIF016";
        case usdgeo::DiagnosticCode::InvalidNoDataValue: return "GTIF018";
        case usdgeo::DiagnosticCode::AuthoringFailed: return "GTIF019";
        default: return "GTIF003";
    }
}

bool ReportDiagnostics(const usdgeo::DiagnosticSink& diagnostics,
                       const std::string& source) {
    for (const usdgeo::Diagnostic& diagnostic : diagnostics.GetDiagnostics()) {
        const std::string message = "[" + PluginCode(diagnostic.code) + "] " +
                                    source + ": " + diagnostic.message;
        if (diagnostic.severity == usdgeo::Severity::Warning) {
            TF_WARN("%s", message.c_str());
        } else {
            TF_RUNTIME_ERROR("%s", message.c_str());
        }
    }
    return !diagnostics.HasError();
}

}  // namespace

// Register the format with USD's type system so the plug system can find it.
TF_REGISTRY_FUNCTION(TfType)
{
    SDF_DEFINE_FILE_FORMAT(UsdRasterGeoTiffFileFormat, SdfFileFormat);
}

UsdRasterGeoTiffFileFormat::UsdRasterGeoTiffFileFormat()
    : SdfFileFormat(TfToken("tif"), TfToken("1.0"), TfToken("usd"),
                    std::vector<std::string>{"tif", "tiff"}) {}

UsdRasterGeoTiffFileFormat::~UsdRasterGeoTiffFileFormat() = default;

SdfFileFormat::FileFormatArguments
UsdRasterGeoTiffFileFormat::GetDefaultFileFormatArguments() const {
    return {{"representation", "metadata"}};
}

bool UsdRasterGeoTiffFileFormat::CanRead(const std::string& file) const {
    std::string extension = SdfFileFormat::GetFileExtension(file);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return extension == "tif" || extension == "tiff";
}

bool UsdRasterGeoTiffFileFormat::Read(SdfLayer* layer,
                                      const std::string& resolvedPath,
                                      bool metadataOnly) const {
    usdgeo::DiagnosticSink diagnostics;
    const std::string sourceLabel = SafeSourceLabel(resolvedPath);
    RasterArguments arguments;
    if (!ParseArguments(layer ? layer->GetFileFormatArguments()
                              : SdfFileFormat::FileFormatArguments{},
                        arguments, diagnostics)) {
        ReportDiagnostics(diagnostics, sourceLabel);
        return false;
    }
    if (metadataOnly) arguments.representation = "metadata";

    const std::shared_ptr<ArAsset> asset =
        ArGetResolver().OpenAsset(ArResolvedPath(resolvedPath));
    if (!asset) {
        diagnostics.AddError(usdgeo::DiagnosticCode::SourceUnavailable,
                             "resolver could not open the asset");
        ReportDiagnostics(diagnostics, sourceLabel);
        return false;
    }
    ArAssetSource source(asset, sourceLabel);
    usdraster::RasterMetadata metadata;
    usdgeotiff::GeoTiffReader reader(source);
    if (!reader.ReadMetadata(&metadata, &diagnostics)) {
        ReportDiagnostics(diagnostics, sourceLabel);
        return false;
    }
    if (!metadata.hasGeoTransform) {
        diagnostics.AddError(usdgeo::DiagnosticCode::MissingGeoreference,
                             "source has no usable georeferencing");
        ReportDiagnostics(diagnostics, sourceLabel);
        return false;
    }
    if (metadata.pixelAnchor != usdraster::PixelAnchor::Unknown &&
        arguments.pixelAnchor != usdraster::PixelAnchor::Unknown) {
        diagnostics.AddError(usdgeo::DiagnosticCode::ConflictingFormatArguments,
                             "pixelAnchor cannot override anchoring declared by the source");
        ReportDiagnostics(diagnostics, sourceLabel);
        return false;
    }
    std::string pixelAnchorSource = "file";
    if (metadata.pixelAnchor == usdraster::PixelAnchor::Unknown) {
        if (arguments.pixelAnchor == usdraster::PixelAnchor::Unknown) {
            diagnostics.AddError(usdgeo::DiagnosticCode::MissingGeoreference,
                                 "pixel anchoring is absent; supply pixelAnchor=area or point");
            ReportDiagnostics(diagnostics, sourceLabel);
            return false;
        }
        metadata.pixelAnchor = arguments.pixelAnchor;
        pixelAnchorSource = "argument";
        usdraster::TryGetWindowBounds(metadata.geoTransform,
                                      usdraster::RasterWindow::FromSize(metadata.size),
                                      metadata.pixelAnchor, metadata.bounds);
    }
    if (arguments.representation == "metadata") {
        if (!usdrasterauthoring::AuthorMetadata(
                layer, metadata, 1, pixelAnchorSource, sourceLabel, &diagnostics)) {
            ReportDiagnostics(diagnostics, sourceLabel);
            return false;
        }
    } else {
        usdraster::RasterGrid grid;
        usdraster::RasterReadOptions options;
        options.band = 1;
        if (!reader.ReadWindow(usdraster::RasterWindow::FromSize(metadata.size),
                               options, &grid, &diagnostics) ||
            !usdrasterauthoring::AuthorMesh(
                layer, metadata, grid, usdrasterauthoring::MeshAuthoringOptions{},
                pixelAnchorSource, sourceLabel, &diagnostics)) {
            ReportDiagnostics(diagnostics, sourceLabel);
            return false;
        }
    }
    if (diagnostics.HasError()) {
        ReportDiagnostics(diagnostics, sourceLabel);
        return false;
    }
    ReportDiagnostics(diagnostics, sourceLabel);
    return true;
}

bool UsdRasterGeoTiffFileFormat::WriteToString(
    const SdfLayer& layer, std::string* str, const std::string& comment) const {
    const SdfFileFormatConstPtr usda = SdfFileFormat::FindByExtension("usda");
    return usda ? usda->WriteToString(layer, str, comment)
                : layer.ExportToString(str);
}

PXR_NAMESPACE_CLOSE_SCOPE
