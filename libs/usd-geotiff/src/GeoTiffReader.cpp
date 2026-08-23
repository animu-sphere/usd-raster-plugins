#include "usdgeotiff/GeoTiffReader.h"

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <limits>
#include <map>
#include <new>
#include <utility>
#include <vector>

namespace usdgeotiff {
namespace {

enum : std::uint16_t { Byte = 1, Ascii = 2, Short = 3, Long = 4,
                        Rational = 5, Float = 11, Double = 12, Long8 = 16 };
enum : std::uint16_t { ImageWidth = 256, ImageLength = 257,
    BitsPerSample = 258, Compression = 259, StripOffsets = 273,
    SamplesPerPixel = 277, RowsPerStrip = 278, StripByteCounts = 279,
    PlanarConfig = 284, TileWidth = 322, TileLength = 323,
    TileOffsets = 324, TileByteCounts = 325, SampleFormat = 339,
    ModelPixelScale = 33550, ModelTiepoint = 33922,
    ModelTransformation = 34264, GeoKeyDirectory = 34735,
    GdalNoData = 42113 };
enum : std::uint16_t { ModelType = 1024, RasterType = 1025,
    GeographicType = 2048, ProjectedCsType = 3072, LinearUnits = 3076 };

struct Value { std::uint16_t type = 0; std::uint64_t count = 0;
               std::vector<std::uint8_t> bytes; };

constexpr std::size_t kMaxMetadataBytes = 64u * 1024u * 1024u;

bool IsKnownTag(std::uint16_t tag) {
    switch (tag) {
        case ImageWidth: case ImageLength: case BitsPerSample:
        case Compression: case StripOffsets: case SamplesPerPixel:
        case RowsPerStrip: case StripByteCounts: case PlanarConfig:
        case TileWidth: case TileLength: case TileOffsets: case TileByteCounts:
        case SampleFormat: case ModelPixelScale: case ModelTiepoint:
        case ModelTransformation: case GeoKeyDirectory: case GdalNoData:
            return true;
        default:
            return false;
    }
}

bool GetDataType(std::uint64_t format, std::uint64_t bits,
                 usdraster::RasterDataType& dataType) {
    if (format == 1 && bits == 8) dataType = usdraster::RasterDataType::UInt8;
    else if (format == 1 && bits == 16) dataType = usdraster::RasterDataType::UInt16;
    else if (format == 1 && bits == 32) dataType = usdraster::RasterDataType::UInt32;
    else if (format == 2 && bits == 8) dataType = usdraster::RasterDataType::Int8;
    else if (format == 2 && bits == 16) dataType = usdraster::RasterDataType::Int16;
    else if (format == 2 && bits == 32) dataType = usdraster::RasterDataType::Int32;
    else if (format == 3 && bits == 32) dataType = usdraster::RasterDataType::Float32;
    else if (format == 3 && bits == 64) dataType = usdraster::RasterDataType::Float64;
    else return false;
    return true;
}

std::size_t TypeSize(std::uint16_t type) {
    switch (type) { case Byte: case Ascii: return 1; case Short: return 2;
        case Long: case Float: return 4; case Rational: case Double: return 8;
        case Long8: return 8; default: return 0; }
}

class Parser {
public:
    Parser(usdraster::RandomAccessSource& source, usdgeo::DiagnosticSink& sink)
        : source(source), sink(sink), size(source.GetSize()) {}

    bool Run(usdraster::RasterMetadata& metadata) {
        std::uint8_t header[16] = {};
        if (!Read(0, isBigTiff ? 16 : 8, header)) return false;
        if (header[0] == 'I' && header[1] == 'I') little = true;
        else if (header[0] == 'M' && header[1] == 'M') little = false;
        else return Error(usdgeo::DiagnosticCode::InvalidSignature, 0,
                          "not a TIFF byte order marker");
        const std::uint16_t version = U16(header + 2);
        if (version == 42) { isBigTiff = false; firstIfd = U32(header + 4); }
        else if (version == 43) {
            isBigTiff = true;
            if (!Read(0, 16, header) || U16(header + 4) != 8 || U16(header + 6) != 0)
                return Error(usdgeo::DiagnosticCode::UnsupportedVersion, 2,
                             "unsupported BigTIFF offset size");
            firstIfd = U64(header + 8);
        } else return Error(usdgeo::DiagnosticCode::UnsupportedVersion, 2,
                            "unsupported TIFF version");

        std::map<std::uint16_t, Value> tags;
        std::uint64_t ifd = firstIfd;
        for (std::uint32_t level = 0; ifd != 0 && level < 1024; ++level) {
            std::map<std::uint16_t, Value> current;
            std::uint64_t next = 0;
            if (!ReadIfd(ifd, current, next)) return false;
            if (level == 0) tags = current;
            else {
                std::uint64_t width = Number(current, ImageWidth, 0);
                std::uint64_t height = Number(current, ImageLength, 0);
                if (width && height) metadata.overviewSizes.push_back({width, height});
            }
            ifd = next;
        }
        if (ifd != 0) return Error(usdgeo::DiagnosticCode::InvalidOffset, ifd,
                                   "IFD chain is too deep");
        return Decode(tags, metadata);
    }

private:
    bool Read(std::uint64_t offset, std::size_t length, void* destination) {
        if (offset > size || length > size - offset)
            return Error(usdgeo::DiagnosticCode::TruncatedData, offset,
                         "TIFF read extends beyond the source");
        auto result = source.Read(offset, length, destination);
        if (!result.IsOk() || result.bytesRead != length)
            return Error(result.status == usdraster::ReadStatus::ShortRead
                              ? usdgeo::DiagnosticCode::ShortRead
                              : usdgeo::DiagnosticCode::SourceUnavailable,
                         offset, "unable to read TIFF metadata");
        return true;
    }

    bool Error(usdgeo::DiagnosticCode code, std::uint64_t offset,
               const char* message) {
        usdgeo::Diagnostic diagnostic;
        diagnostic.code = code; diagnostic.severity = usdgeo::Severity::Error;
        diagnostic.message = message; diagnostic.byteOffset = offset;
        sink.Add(std::move(diagnostic));
        return false;
    }
    void Warning(usdgeo::DiagnosticCode code, const char* message) {
        sink.AddWarning(code, message);
    }
    std::uint16_t U16(const std::uint8_t* p) const {
        return little ? std::uint16_t(p[0] | (p[1] << 8))
                       : std::uint16_t((p[0] << 8) | p[1]);
    }
    std::uint32_t U32(const std::uint8_t* p) const {
        if (little) return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
            (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
        return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) |
            (std::uint32_t(p[2]) << 8) | p[3];
    }
    std::uint64_t U64(const std::uint8_t* p) const {
        std::uint64_t value = 0;
        if (little) for (int i = 7; i >= 0; --i) value = (value << 8) | p[i];
        else for (int i = 0; i < 8; ++i) value = (value << 8) | p[i];
        return value;
    }
    double NumberValue(const std::uint8_t* p, std::uint16_t type) const {
        if (type == Short) return U16(p);
        if (type == Long) return U32(p);
        if (type == Long8) return static_cast<double>(U64(p));
        std::uint64_t bits = type == Float ? U32(p) : U64(p);
        double result = 0.0;
        if (type == Float) { float f; std::uint32_t b = static_cast<std::uint32_t>(bits);
            std::memcpy(&f, &b, sizeof(f)); result = f; }
        else std::memcpy(&result, &bits, sizeof(result));
        return result;
    }
    bool ReadIfd(std::uint64_t offset, std::map<std::uint16_t, Value>& tags,
                 std::uint64_t& next) {
        const std::size_t countSize = isBigTiff ? 8 : 2;
        std::uint8_t countBytes[8] = {};
        if (!Read(offset, countSize, countBytes)) return false;
        const std::uint64_t count = isBigTiff ? U64(countBytes) : U16(countBytes);
        const std::uint64_t entrySize = isBigTiff ? 20 : 12;
        const std::uint64_t tailSize = isBigTiff ? 8 : 4;
        const std::uint64_t remaining = size - offset - countSize;
        if (remaining < tailSize || count > (remaining - tailSize) / entrySize)
            return Error(usdgeo::DiagnosticCode::InvalidOffset, offset,
                         "IFD entry table exceeds the source");
        const std::uint64_t tableSize = count * entrySize + tailSize;
        if (tableSize > kMaxMetadataBytes || tableSize > std::numeric_limits<std::size_t>::max())
            return Error(usdgeo::DiagnosticCode::InvalidOffset, offset,
                         "IFD entry table is too large");
        std::vector<std::uint8_t> table(static_cast<std::size_t>(tableSize));
        if (!Read(offset + countSize, table.size(), table.data())) return false;
        for (std::uint64_t i = 0; i < count; ++i) {
            const std::uint8_t* p = table.data() + i * entrySize;
            const auto tag = U16(p); const auto type = U16(p + 2);
            const auto number = isBigTiff ? U64(p + 4) : U32(p + 4);
            const std::size_t typeSize = TypeSize(type);
            if (!typeSize || number > std::numeric_limits<std::size_t>::max() / typeSize)
                return Error(usdgeo::DiagnosticCode::InvalidOffset, offset,
                             "invalid TIFF field type or count");
            const std::size_t valueBytes = typeSize * static_cast<std::size_t>(number);
            if (!IsKnownTag(tag)) continue;
            if (valueBytes > kMaxMetadataBytes || metadataBytes > kMaxMetadataBytes - valueBytes)
                return Error(usdgeo::DiagnosticCode::InvalidOffset, offset,
                             "TIFF metadata values exceed the reader limit");
            Value value{type, number, {}}; value.bytes.resize(valueBytes);
            metadataBytes += valueBytes;
            const std::size_t capacity = isBigTiff ? 8 : 4;
            if (valueBytes <= capacity) std::memcpy(value.bytes.data(), p + (isBigTiff ? 12 : 8), valueBytes);
            else {
                const std::uint64_t valueOffset = isBigTiff ? U64(p + 12) : U32(p + 8);
                if (!Read(valueOffset, valueBytes, value.bytes.data())) return false;
            }
            tags[tag] = std::move(value);
        }
        const std::uint8_t* tail = table.data() + count * entrySize;
        next = isBigTiff ? U64(tail) : U32(tail);
        return true;
    }
    std::uint64_t Number(const std::map<std::uint16_t, Value>& tags,
                         std::uint16_t tag, std::size_t index) const {
        auto it = tags.find(tag); if (it == tags.end() || index >= it->second.count) return 0;
        return static_cast<std::uint64_t>(NumberValue(it->second.bytes.data() + index * TypeSize(it->second.type), it->second.type));
    }
    double Real(const std::map<std::uint16_t, Value>& tags, std::uint16_t tag,
                std::size_t index) const {
        auto it = tags.find(tag); if (it == tags.end() || index >= it->second.count) return 0.0;
        return NumberValue(it->second.bytes.data() + index * TypeSize(it->second.type), it->second.type);
    }
    bool Decode(const std::map<std::uint16_t, Value>& tags, usdraster::RasterMetadata& m) {
        m.bounds = usdgeo::GeoBounds::Empty();
        const auto width = Number(tags, ImageWidth, 0), height = Number(tags, ImageLength, 0);
        if (!width || !height) return Error(usdgeo::DiagnosticCode::InvalidRasterSize, firstIfd, "TIFF dimensions are missing or zero");
        m.size = {width, height};
        const auto samples = tags.count(SamplesPerPixel) ? Number(tags, SamplesPerPixel, 0) : 1;
        if (!samples || samples > std::numeric_limits<std::uint32_t>::max())
            return Error(usdgeo::DiagnosticCode::InvalidRasterSize, firstIfd, "TIFF samples per pixel are missing or invalid");
        const auto bitsTag = tags.find(BitsPerSample);
        const auto formatTag = tags.find(SampleFormat);
        if (bitsTag == tags.end() || bitsTag->second.count == 0 ||
            (bitsTag->second.count != 1 && bitsTag->second.count != samples) ||
            (formatTag != tags.end() && formatTag->second.count != 1 && formatTag->second.count != samples))
            return Error(usdgeo::DiagnosticCode::UnsupportedSampleFormat, firstIfd, "inconsistent TIFF sample metadata");
        const auto defaultFormat = formatTag == tags.end() ? 1 : 0;
        for (std::uint32_t i = 1; i <= samples; ++i) {
            const std::size_t index = i - 1;
            const auto bits = Number(tags, BitsPerSample, bitsTag->second.count == 1 ? 0 : index);
            const auto format = formatTag == tags.end() ? defaultFormat : Number(tags, SampleFormat, formatTag->second.count == 1 ? 0 : index);
            usdraster::RasterDataType dataType;
            if (!GetDataType(format, bits, dataType))
                return Error(usdgeo::DiagnosticCode::UnsupportedSampleFormat, firstIfd, "unsupported TIFF sample format or bit depth");
            usdraster::RasterBandInfo band; band.index = i; band.dataType = dataType; m.bands.push_back(std::move(band));
        }
        const auto compression = tags.count(Compression) ? Number(tags, Compression, 0) : 1;
        if (compression != 1) return Error(usdgeo::DiagnosticCode::UnsupportedCompression, firstIfd, "unsupported TIFF compression");
        const auto planar = tags.count(PlanarConfig) ? Number(tags, PlanarConfig, 0) : 1;
        if (planar != 0 && planar != 1) return Error(usdgeo::DiagnosticCode::UnsupportedPlanarConfiguration, firstIfd, "unsupported TIFF planar configuration");
        if (tags.count(TileWidth) || tags.count(TileLength)) {
            const auto tileWidth = Number(tags, TileWidth, 0), tileHeight = Number(tags, TileLength, 0);
            if (!tileWidth || !tileHeight) return Error(usdgeo::DiagnosticCode::InconsistentTileLayout, firstIfd, "TIFF tile dimensions are invalid");
            m.nativeTileSize = usdraster::RasterSize{tileWidth, tileHeight};
        }
        const auto offsetTag = m.nativeTileSize ? TileOffsets : StripOffsets;
        const auto countTag = m.nativeTileSize ? TileByteCounts : StripByteCounts;
        auto offsets = tags.find(offsetTag); auto byteCounts = tags.find(countTag);
        if (offsets == tags.end() || byteCounts == tags.end() || offsets->second.count != byteCounts->second.count)
            return Error(usdgeo::DiagnosticCode::InconsistentTileLayout, firstIfd, "TIFF segment layout is missing or inconsistent");
        for (std::size_t i = 0; i < offsets->second.count; ++i) {
            const auto offset = Number(tags, offsetTag, i), byteCount = Number(tags, countTag, i);
            if (!byteCount || offset > source.GetSize() || byteCount > source.GetSize() - offset)
                return Error(usdgeo::DiagnosticCode::InvalidOffset, offset, "TIFF segment extends beyond the source");
        }
        auto keyValue = [&](std::uint16_t wanted) {
            auto key = tags.find(GeoKeyDirectory);
            if (key == tags.end()) return std::uint64_t{0};
            for (std::size_t i = 4; i + 3 < key->second.count; i += 4)
                if (Number(tags, GeoKeyDirectory, i) == wanted)
                    return Number(tags, GeoKeyDirectory, i + 3);
            return std::uint64_t{0};
        };
        auto geoKeyDirectory = tags.find(GeoKeyDirectory);
        if (geoKeyDirectory != tags.end() &&
            (geoKeyDirectory->second.type != Short ||
             geoKeyDirectory->second.count < 4 ||
             (geoKeyDirectory->second.count - 4) % 4 != 0))
            return Error(usdgeo::DiagnosticCode::InvalidCrs, firstIfd,
                         "invalid GeoTIFF key directory");
        const auto modelType = keyValue(ModelType);
        m.crs.kind = modelType == 1 ? usdgeo::CrsKind::Projected : modelType == 2 ? usdgeo::CrsKind::Geographic : usdgeo::CrsKind::Unknown;
        const auto epsg = keyValue(m.crs.kind == usdgeo::CrsKind::Projected ? ProjectedCsType : GeographicType); if (epsg) m.crs.epsgCode = static_cast<int>(epsg);
        if (m.crs.kind == usdgeo::CrsKind::Projected && keyValue(LinearUnits) == 9001) m.crs.linearUnit = usdgeo::LinearUnit::Metre;
        const auto rasterType = keyValue(RasterType); if (rasterType == 1) m.pixelAnchor = usdraster::PixelAnchor::Area; else if (rasterType == 2) m.pixelAnchor = usdraster::PixelAnchor::Point;
        const auto matrixTag = tags.find(ModelTransformation);
        const auto scaleTag = tags.find(ModelPixelScale);
        const auto tiepointTag = tags.find(ModelTiepoint);
        const bool hasMatrix = matrixTag != tags.end();
        const bool hasScaleTag = scaleTag != tags.end();
        const bool hasTiepointTag = tiepointTag != tags.end();
        if (hasMatrix && (matrixTag->second.type != Double || matrixTag->second.count != 16))
            return Error(usdgeo::DiagnosticCode::InvalidGeoTransform, firstIfd,
                         "ModelTransformationTag must contain sixteen doubles");
        if (hasScaleTag != hasTiepointTag)
            return Error(usdgeo::DiagnosticCode::InvalidGeoTransform, firstIfd,
                         "ModelPixelScaleTag and ModelTiepointTag must appear together");
        const bool hasScale = hasScaleTag && hasTiepointTag;
        if (hasScale && (scaleTag->second.type != Double || scaleTag->second.count != 3 ||
                         tiepointTag->second.type != Double || tiepointTag->second.count < 6))
            return Error(usdgeo::DiagnosticCode::InvalidGeoTransform, firstIfd,
                         "invalid ModelPixelScaleTag or ModelTiepointTag");
        usdraster::RasterGeoTransform matrixTransform;
        usdraster::RasterGeoTransform scaleTransform;
        if (hasMatrix) {
            double matrix[16];
            for (int i = 0; i < 16; ++i) matrix[i] = Real(tags, ModelTransformation, i);
            matrixTransform = usdraster::RasterGeoTransform::FromMatrix(matrix);
        }
        if (hasScale)
            scaleTransform = usdraster::RasterGeoTransform::FromPixelScaleAndTiepoint(
                Real(tags, ModelPixelScale, 0), Real(tags, ModelPixelScale, 1),
                Real(tags, ModelTiepoint, 0), Real(tags, ModelTiepoint, 1),
                Real(tags, ModelTiepoint, 3), Real(tags, ModelTiepoint, 4));
        if (hasMatrix) {
            m.geoTransform = matrixTransform;
            m.hasGeoTransform = true;
            if (hasScale && !(matrixTransform == scaleTransform))
                Warning(usdgeo::DiagnosticCode::ConflictingGeoTransform,
                        "georeferencing tags disagree; using ModelTransformation");
        } else if (hasScale) {
            m.geoTransform = scaleTransform;
            m.hasGeoTransform = true;
        }
        if (m.hasGeoTransform && !m.geoTransform.IsInvertible()) return Error(usdgeo::DiagnosticCode::InvalidGeoTransform, firstIfd, "GeoTIFF geotransform is not invertible");
        if (m.hasGeoTransform && m.pixelAnchor == usdraster::PixelAnchor::Unknown) Warning(usdgeo::DiagnosticCode::UnknownPixelAnchor, "GeoTIFF pixel anchoring is absent; an explicit pixelAnchor is required");
        if (m.hasGeoTransform && m.pixelAnchor != usdraster::PixelAnchor::Unknown) usdraster::TryGetWindowBounds(m.geoTransform, {0, 0, width, height}, m.pixelAnchor, m.bounds);
        auto nodata = tags.find(GdalNoData);
        if (nodata != tags.end()) {
            if (nodata->second.type != Ascii)
                return Error(usdgeo::DiagnosticCode::InvalidNoDataValue, firstIfd,
                             "GDAL_NODATA must be an ASCII value");
            std::string text(reinterpret_cast<const char*>(nodata->second.bytes.data()), nodata->second.bytes.size());
            const auto terminator = text.find('\0');
            if (terminator != std::string::npos) text.resize(terminator);
            char* end = nullptr;
            errno = 0;
            const double value = std::strtod(text.c_str(), &end);
            while (end != text.c_str() && *end != '\0' &&
                   std::isspace(static_cast<unsigned char>(*end))) ++end;
            if (end == text.c_str() || *end != '\0' || errno == ERANGE ||
                (!std::isfinite(value) && !std::isnan(value)))
                return Error(usdgeo::DiagnosticCode::InvalidNoDataValue, firstIfd,
                             "GDAL_NODATA is not a valid numeric value");
            for (auto& band : m.bands) band.noData = usdraster::NoDataValue(value);
        }
        return true;
    }
    usdraster::RandomAccessSource& source; usdgeo::DiagnosticSink& sink; std::uint64_t size = 0, firstIfd = 0; std::size_t metadataBytes = 0; bool little = true, isBigTiff = false;
};

}  // namespace

GeoTiffReader::GeoTiffReader(usdraster::RandomAccessSource& source) : _source(source) {}

bool GeoTiffReader::ReadMetadata(usdraster::RasterMetadata* metadata,
                                 usdgeo::DiagnosticSink* diagnostics) const {
    if (!metadata || !diagnostics) return false;
    *metadata = usdraster::RasterMetadata{};
    try {
        return Parser(_source, *diagnostics).Run(*metadata);
    } catch (const std::bad_alloc&) {
        diagnostics->AddError(usdgeo::DiagnosticCode::MemoryBudgetExceeded,
                              "memory allocation failed while reading TIFF metadata");
        return false;
    }
}

}  // namespace usdgeotiff