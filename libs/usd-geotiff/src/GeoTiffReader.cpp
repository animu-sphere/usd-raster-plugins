#include "usdgeotiff/GeoTiffReader.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

#if defined(USDRASTER_HAS_LIBTIFF)
#include <tiffio.h>
#endif

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
    GdalMetadata = 42112, GdalNoData = 42113 };
enum : std::uint16_t { ModelType = 1024, RasterType = 1025,
    GeographicType = 2048, ProjectedCsType = 3072, LinearUnits = 3076 };

struct Value { std::uint16_t type = 0; std::uint64_t count = 0;
               std::vector<std::uint8_t> bytes; };

struct Segment {
    std::uint64_t offset = 0;
    std::uint64_t byteCount = 0;
};

struct TiffLayout {
    bool little = true;
    bool tiled = false;
    std::uint64_t width = 0;
    std::uint64_t height = 0;
    std::uint32_t samples = 1;
    std::uint64_t rowsPerStrip = 0;
    std::uint64_t tileWidth = 0;
    std::uint64_t tileHeight = 0;
    std::uint16_t planar = 1;
    std::uint16_t compression = 1;
    std::vector<std::uint32_t> sampleBytes;
    std::vector<std::uint64_t> sampleOffsets;
    std::vector<usdraster::RasterDataType> dataTypes;
    std::uint64_t pixelStride = 0;
    std::vector<Segment> segments;
};

bool IsLittleEndianHost() {
    const std::uint16_t value = 1;
    return *reinterpret_cast<const std::uint8_t*>(&value) == 1;
}

#if defined(USDRASTER_HAS_LIBTIFF)
struct LibTiffSource {
    usdraster::RandomAccessSource* source = nullptr;
    std::uint64_t position = 0;
    bool failed = false;
};

tmsize_t LibTiffRead(thandle_t handle, void* buffer, tmsize_t size) {
    auto* client = static_cast<LibTiffSource*>(handle);
    if (size <= 0) return 0;
    if (static_cast<std::uint64_t>(size) >
        std::numeric_limits<std::size_t>::max()) {
        client->failed = true;
        return 0;
    }
    const auto result = client->source->Read(
        client->position, static_cast<std::size_t>(size), buffer);
    if (result.bytesRead > std::numeric_limits<std::uint64_t>::max() -
                               client->position) {
        client->failed = true;
        return 0;
    }
    client->position += result.bytesRead;
    if (!result.IsOk() || result.bytesRead != static_cast<std::size_t>(size))
        client->failed = true;
    return static_cast<tmsize_t>(result.bytesRead);
}

toff_t LibTiffSeek(thandle_t handle, toff_t offset, int whence) {
    auto* client = static_cast<LibTiffSource*>(handle);
    const auto signedOffset = static_cast<std::int64_t>(offset);
    std::uint64_t base = 0;
    if (whence == SEEK_SET) {
        client->position = offset;
        return offset;
    }
    if (whence == SEEK_CUR) {
        base = client->position;
    } else if (whence == SEEK_END) {
        base = client->source->GetSize();
    } else {
        client->failed = true;
        return static_cast<toff_t>(-1);
    }
    if (signedOffset < 0) {
        const auto magnitude = static_cast<std::uint64_t>(-(signedOffset + 1)) + 1;
        if (magnitude > base) {
            client->failed = true;
            return static_cast<toff_t>(-1);
        }
        client->position = base - magnitude;
    } else {
        const auto distance = static_cast<std::uint64_t>(signedOffset);
        if (distance > std::numeric_limits<std::uint64_t>::max() - base) {
            client->failed = true;
            return static_cast<toff_t>(-1);
        }
        client->position = base + distance;
    }
    return static_cast<toff_t>(client->position);
}

int LibTiffClose(thandle_t) { return 0; }

toff_t LibTiffSize(thandle_t handle) {
    return static_cast<toff_t>(
        static_cast<LibTiffSource*>(handle)->source->GetSize());
}

int LibTiffMap(thandle_t, void**, toff_t*) { return 0; }
void LibTiffUnmap(thandle_t, void*, toff_t) {}
#endif

    bool CheckedAdd(std::uint64_t left, std::uint64_t right,
                    std::uint64_t* result) {
        if (left > std::numeric_limits<std::uint64_t>::max() - right)
            return false;
        *result = left + right;
        return true;
    }

    bool CheckedMultiply(std::uint64_t left, std::uint64_t right,
                         std::uint64_t* result) {
        if (left != 0 && right > std::numeric_limits<std::uint64_t>::max() / left)
            return false;
        *result = left * right;
        return true;
    }

std::uint16_t ReadU16(const std::uint8_t* p, bool little) {
    return little ? std::uint16_t(p[0] | (p[1] << 8))
                  : std::uint16_t((p[0] << 8) | p[1]);
}

std::uint32_t ReadU32(const std::uint8_t* p, bool little) {
    if (little) return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
        (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) |
        (std::uint32_t(p[2]) << 8) | p[3];
}

std::uint64_t ReadU64(const std::uint8_t* p, bool little) {
    std::uint64_t value = 0;
    if (little) for (int i = 7; i >= 0; --i) value = (value << 8) | p[i];
    else for (int i = 0; i < 8; ++i) value = (value << 8) | p[i];
    return value;
}

double DecodeSample(const std::uint8_t* p, usdraster::RasterDataType type,
                    bool little) {
    switch (type) {
        case usdraster::RasterDataType::UInt8: return p[0];
        case usdraster::RasterDataType::Int8: return static_cast<std::int8_t>(p[0]);
        case usdraster::RasterDataType::UInt16: return ReadU16(p, little);
        case usdraster::RasterDataType::Int16: return static_cast<std::int16_t>(ReadU16(p, little));
        case usdraster::RasterDataType::UInt32: return ReadU32(p, little);
        case usdraster::RasterDataType::Int32: return static_cast<std::int32_t>(ReadU32(p, little));
        case usdraster::RasterDataType::Float32: {
            const std::uint32_t bits = ReadU32(p, little);
            float value = 0.0f;
            std::memcpy(&value, &bits, sizeof(value));
            return value;
        }
        case usdraster::RasterDataType::Float64: {
            const std::uint64_t bits = ReadU64(p, little);
            double value = 0.0;
            std::memcpy(&value, &bits, sizeof(value));
            return value;
        }
    }
    return 0.0;
}

bool AddReadError(usdgeo::DiagnosticSink& sink, usdgeo::DiagnosticCode code,
                  const char* message, const usdraster::RasterWindow& window,
                  std::uint32_t band = 0) {
    usdgeo::Diagnostic diagnostic;
    diagnostic.code = code;
    diagnostic.severity = usdgeo::Severity::Error;
    diagnostic.message = message;
    diagnostic.window = window.ToAnchor();
    if (band != 0) diagnostic.band = band;
    sink.Add(std::move(diagnostic));
    return false;
}

    void AddReadWarning(usdgeo::DiagnosticSink& sink, usdgeo::DiagnosticCode code,
                        const char* message, const usdraster::RasterWindow& window,
                        std::uint32_t band) {
        usdgeo::Diagnostic diagnostic;
        diagnostic.code = code;
        diagnostic.severity = usdgeo::Severity::Warning;
        diagnostic.message = message;
        diagnostic.window = window.ToAnchor();
        diagnostic.band = band;
        sink.Add(std::move(diagnostic));
    }

    double ConvertSample(double value, usdraster::RasterDataType outputType) {
        switch (outputType) {
            case usdraster::RasterDataType::UInt8:
                if (!std::isfinite(value) || value <= 0.0) return 0.0;
                return value >= std::numeric_limits<std::uint8_t>::max()
                    ? std::numeric_limits<std::uint8_t>::max()
                    : static_cast<std::uint8_t>(value);
            case usdraster::RasterDataType::Int8:
                if (!std::isfinite(value)) return 0.0;
                if (value <= std::numeric_limits<std::int8_t>::min())
                    return std::numeric_limits<std::int8_t>::min();
                if (value >= std::numeric_limits<std::int8_t>::max())
                    return std::numeric_limits<std::int8_t>::max();
                return static_cast<std::int8_t>(value);
            case usdraster::RasterDataType::UInt16:
                if (!std::isfinite(value) || value <= 0.0) return 0.0;
                return value >= std::numeric_limits<std::uint16_t>::max()
                    ? std::numeric_limits<std::uint16_t>::max()
                    : static_cast<std::uint16_t>(value);
            case usdraster::RasterDataType::Int16:
                if (!std::isfinite(value)) return 0.0;
                if (value <= std::numeric_limits<std::int16_t>::min())
                    return std::numeric_limits<std::int16_t>::min();
                if (value >= std::numeric_limits<std::int16_t>::max())
                    return std::numeric_limits<std::int16_t>::max();
                return static_cast<std::int16_t>(value);
            case usdraster::RasterDataType::UInt32:
                if (!std::isfinite(value) || value <= 0.0) return 0.0;
                return value >= std::numeric_limits<std::uint32_t>::max()
                    ? std::numeric_limits<std::uint32_t>::max()
                    : static_cast<std::uint32_t>(value);
            case usdraster::RasterDataType::Int32:
                if (!std::isfinite(value)) return 0.0;
                if (value <= std::numeric_limits<std::int32_t>::min())
                    return std::numeric_limits<std::int32_t>::min();
                if (value >= std::numeric_limits<std::int32_t>::max())
                    return std::numeric_limits<std::int32_t>::max();
                return static_cast<std::int32_t>(value);
            case usdraster::RasterDataType::Float32:
                return static_cast<double>(static_cast<float>(value));
            case usdraster::RasterDataType::Float64:
                return value;
        }
        return value;
    }

constexpr std::size_t kMaxMetadataBytes = 64u * 1024u * 1024u;

bool IsKnownTag(std::uint16_t tag) {
    switch (tag) {
        case ImageWidth: case ImageLength: case BitsPerSample:
        case Compression: case StripOffsets: case SamplesPerPixel:
        case RowsPerStrip: case StripByteCounts: case PlanarConfig:
        case TileWidth: case TileLength: case TileOffsets: case TileByteCounts:
        case SampleFormat: case ModelPixelScale: case ModelTiepoint:
        case ModelTransformation: case GeoKeyDirectory: case GdalMetadata:
        case GdalNoData:
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

    bool Run(usdraster::RasterMetadata& metadata, TiffLayout* layout = nullptr) {
        std::uint8_t header[16] = {};
        if (size < 8) return Error(usdgeo::DiagnosticCode::TruncatedHeader, 0,
                                   "TIFF header is truncated");
        if (!Read(0, 8, header)) return false;
        if (header[0] == 'I' && header[1] == 'I') little = true;
        else if (header[0] == 'M' && header[1] == 'M') little = false;
        else return Error(usdgeo::DiagnosticCode::InvalidSignature, 0,
                          "not a TIFF byte order marker");
        const std::uint16_t version = U16(header + 2);
        if (version == 42) { isBigTiff = false; firstIfd = U32(header + 4); }
        else if (version == 43) {
            isBigTiff = true;
            if (size < 16) return Error(usdgeo::DiagnosticCode::TruncatedHeader, 0,
                                        "BigTIFF header is truncated");
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
        return Decode(tags, metadata, layout);
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
    std::uint64_t IntegerValue(const std::uint8_t* p, std::uint16_t type) const {
        if (type == Short) return U16(p);
        if (type == Long) return U32(p);
        if (type == Long8) return U64(p);
        return static_cast<std::uint64_t>(NumberValue(p, type));
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
                return IntegerValue(it->second.bytes.data() + index * TypeSize(it->second.type), it->second.type);
    }
    double Real(const std::map<std::uint16_t, Value>& tags, std::uint16_t tag,
                std::size_t index) const {
        auto it = tags.find(tag); if (it == tags.end() || index >= it->second.count) return 0.0;
        return NumberValue(it->second.bytes.data() + index * TypeSize(it->second.type), it->second.type);
    }
    static std::string Attribute(const std::string& element,
                                 const char* name) {
        const std::string key = std::string(name) + "=\"";
        const std::size_t start = element.find(key);
        if (start == std::string::npos) return {};
        const std::size_t valueStart = start + key.size();
        const std::size_t valueEnd = element.find('"', valueStart);
        return valueEnd == std::string::npos
            ? std::string() : element.substr(valueStart, valueEnd - valueStart);
    }
    static std::string UnescapeXml(std::string value) {
        const std::pair<const char*, const char*> entities[] = {
            {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"},
            {"&quot;", "\""}, {"&apos;", "'"}};
        for (const auto& entity : entities) {
            std::size_t position = 0;
            while ((position = value.find(entity.first, position)) !=
                   std::string::npos) {
                value.replace(position, std::strlen(entity.first), entity.second);
                position += std::strlen(entity.second);
            }
        }
        return value;
    }
    void DecodeGdalMetadata(const Value& value,
                            std::vector<usdraster::RasterBandInfo>& bands) {
        if (value.type != Ascii) return;
        std::string xml(reinterpret_cast<const char*>(value.bytes.data()),
                        value.bytes.size());
        const std::size_t terminator = xml.find('\0');
        if (terminator != std::string::npos) xml.resize(terminator);
        std::size_t itemStart = 0;
        while ((itemStart = xml.find("<Item", itemStart)) !=
               std::string::npos) {
            const std::size_t openEnd = xml.find('>', itemStart);
            if (openEnd == std::string::npos) break;
            const std::size_t closeStart = xml.find("</Item>", openEnd + 1);
            if (closeStart == std::string::npos) break;
            const std::string element = xml.substr(itemStart,
                                                    openEnd - itemStart + 1);
            const std::string sampleText = Attribute(element, "sample");
            char* sampleEnd = nullptr;
            const unsigned long sample = sampleText.empty()
                ? 0 : std::strtoul(sampleText.c_str(), &sampleEnd, 10);
            if (!sampleText.empty() && sampleEnd && *sampleEnd == '\0' &&
                sample < bands.size()) {
                std::string name = Attribute(element, "name");
                std::transform(name.begin(), name.end(), name.begin(),
                               [](unsigned char character) {
                                   return static_cast<char>(std::tolower(character));
                               });
                std::string text = UnescapeXml(
                    xml.substr(openEnd + 1, closeStart - openEnd - 1));
                if (name == "description") {
                    bands[sample].description = std::move(text);
                } else if (name == "unit" || name == "unittype") {
                    bands[sample].unit = std::move(text);
                } else if (name == "scale" || name == "offset") {
                    const auto first = text.find_first_not_of(" \t\r\n");
                    const auto last = text.find_last_not_of(" \t\r\n");
                    if (first == std::string::npos) {
                        itemStart = closeStart + 7;
                        continue;
                    }
                    text = text.substr(first, last - first + 1);
                    char* end = nullptr;
                    errno = 0;
                    const double number = std::strtod(text.c_str(), &end);
                    if (end != text.c_str() && *end == '\0' &&
                        errno != ERANGE && std::isfinite(number)) {
                        if (name == "scale") bands[sample].scale = number;
                        else bands[sample].offset = number;
                    }
                }
            }
            itemStart = closeStart + 7;
        }
    }
    bool Decode(const std::map<std::uint16_t, Value>& tags,
                usdraster::RasterMetadata& m, TiffLayout* layout) {
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
        std::vector<std::uint32_t> sampleBytes;
        std::vector<std::uint64_t> sampleOffsets;
        std::vector<usdraster::RasterDataType> dataTypes;
        std::uint64_t pixelStride = 0;
        for (std::uint32_t i = 1; i <= samples; ++i) {
            const std::size_t index = i - 1;
            const auto bits = Number(tags, BitsPerSample, bitsTag->second.count == 1 ? 0 : index);
            const auto format = formatTag == tags.end() ? defaultFormat : Number(tags, SampleFormat, formatTag->second.count == 1 ? 0 : index);
            usdraster::RasterDataType dataType;
            if (!GetDataType(format, bits, dataType))
                return Error(usdgeo::DiagnosticCode::UnsupportedSampleFormat, firstIfd, "unsupported TIFF sample format or bit depth");
            const std::uint32_t bytes = usdraster::GetDataTypeSize(dataType);
            if (pixelStride > std::numeric_limits<std::uint64_t>::max() - bytes)
                return Error(usdgeo::DiagnosticCode::InvalidRasterSize, firstIfd, "TIFF pixel stride is too large");
            usdraster::RasterBandInfo band; band.index = i; band.dataType = dataType; m.bands.push_back(std::move(band));
            sampleBytes.push_back(bytes);
            sampleOffsets.push_back(pixelStride);
            dataTypes.push_back(dataType);
            pixelStride += bytes;
        }
        const auto compression = tags.count(Compression) ? Number(tags, Compression, 0) : 1;
        if (compression != 1
    #if defined(USDRASTER_HAS_LIBTIFF)
            && compression != 5 && compression != 8 && compression != 32773 &&
            compression != 32946
    #endif
        ) return Error(usdgeo::DiagnosticCode::UnsupportedCompression, firstIfd, "unsupported TIFF compression");
        const auto planar = tags.count(PlanarConfig) ? Number(tags, PlanarConfig, 0) : 1;
        if (planar != 1 && planar != 2) return Error(usdgeo::DiagnosticCode::UnsupportedPlanarConfiguration, firstIfd, "unsupported TIFF planar configuration");
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
        const bool tiled = m.nativeTileSize.has_value();
        std::uint64_t segmentsPerPlane = 0;
        if (tiled) {
            const auto tileWidth = m.nativeTileSize->width;
            const auto tileHeight = m.nativeTileSize->height;
            const auto across = (width - 1) / tileWidth + 1;
            const auto down = (height - 1) / tileHeight + 1;
            if (across > std::numeric_limits<std::uint64_t>::max() / down)
                return Error(usdgeo::DiagnosticCode::InconsistentTileLayout, firstIfd, "TIFF tile count overflows");
            segmentsPerPlane = across * down;
        } else {
            const auto rows = Number(tags, RowsPerStrip, 0);
            if (!rows) return Error(usdgeo::DiagnosticCode::InconsistentTileLayout, firstIfd, "TIFF rows per strip are missing or zero");
            segmentsPerPlane = (height - 1) / rows + 1;
        }
        if (segmentsPerPlane > std::numeric_limits<std::uint64_t>::max() / samples)
            return Error(usdgeo::DiagnosticCode::InconsistentTileLayout, firstIfd, "TIFF segment count overflows");
        const auto expectedSegments = planar == 2
            ? segmentsPerPlane * samples : segmentsPerPlane;
        if (segmentsPerPlane == 0 || expectedSegments > std::numeric_limits<std::size_t>::max() ||
            offsets->second.count != expectedSegments)
            return Error(usdgeo::DiagnosticCode::InconsistentTileLayout, firstIfd, "TIFF segment count does not match the raster layout");
        std::vector<Segment> segments;
        for (std::size_t i = 0; i < offsets->second.count; ++i) {
            const auto offset = Number(tags, offsetTag, i), byteCount = Number(tags, countTag, i);
            if (!byteCount || byteCount > std::numeric_limits<std::size_t>::max() ||
                offset > source.GetSize() || byteCount > source.GetSize() - offset)
                return Error(usdgeo::DiagnosticCode::InvalidOffset, offset, "TIFF segment extends beyond the source");
            segments.push_back(Segment{offset, byteCount});
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
        const auto gdalMetadata = tags.find(GdalMetadata);
        if (gdalMetadata != tags.end()) DecodeGdalMetadata(gdalMetadata->second,
                                                            m.bands);
        if (layout) {
            layout->little = little;
            layout->tiled = tiled;
            layout->width = width;
            layout->height = height;
            layout->samples = static_cast<std::uint32_t>(samples);
            layout->rowsPerStrip = Number(tags, RowsPerStrip, 0);
            layout->tileWidth = tiled ? m.nativeTileSize->width : 0;
            layout->tileHeight = tiled ? m.nativeTileSize->height : 0;
            layout->planar = static_cast<std::uint16_t>(planar);
            layout->compression = static_cast<std::uint16_t>(compression);
            layout->sampleBytes = std::move(sampleBytes);
            if (planar == 2) sampleOffsets.assign(samples, 0);
            layout->sampleOffsets = std::move(sampleOffsets);
            layout->dataTypes = std::move(dataTypes);
            layout->pixelStride = pixelStride;
            layout->segments = std::move(segments);
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

bool GeoTiffReader::ReadWindow(const usdraster::RasterWindow& window,
                               const usdraster::RasterReadOptions& options,
                               usdraster::RasterGrid* grid,
                               usdgeo::DiagnosticSink* diagnostics) const {
    if (!grid || !diagnostics) return false;
    *grid = usdraster::RasterGrid{};

    usdraster::RasterMetadata metadata;
    TiffLayout layout;
    try {
        if (!Parser(_source, *diagnostics).Run(metadata, &layout)) return false;
    } catch (const std::bad_alloc&) {
        diagnostics->AddError(usdgeo::DiagnosticCode::MemoryBudgetExceeded,
                              "memory allocation failed while reading TIFF metadata");
        return false;
    }

    const auto* band = metadata.FindBand(options.band);
    if (!band) {
        return AddReadError(*diagnostics, usdgeo::DiagnosticCode::InvalidBandIndex,
                            "requested TIFF band does not exist", window, options.band);
    }
    if (options.overviewLevel.has_value()) {
        return AddReadError(*diagnostics, usdgeo::DiagnosticCode::UnsupportedOverviewLevel,
                            "TIFF overview reads are not implemented", window, options.band);
    }
    const std::uint64_t step = options.samplingStep == 0 ? 1 : options.samplingStep;
    const bool endXValid = window.GetEndX() >= window.x;
    const bool endYValid = window.GetEndY() >= window.y;
    if (!endXValid || !endYValid || window.x > metadata.size.width ||
        window.y > metadata.size.height || window.GetEndX() > metadata.size.width ||
        window.GetEndY() > metadata.size.height) {
        return AddReadError(*diagnostics, usdgeo::DiagnosticCode::WindowOutOfBounds,
                            "requested window is outside the TIFF raster", window, options.band);
    }

    std::uint64_t segmentsAcross = 1;
    std::uint64_t segmentsDown = 1;
    if (layout.tiled) {
        segmentsAcross = (layout.width - 1) / layout.tileWidth + 1;
        segmentsDown = (layout.height - 1) / layout.tileHeight + 1;
    } else {
        segmentsDown = (layout.height - 1) / layout.rowsPerStrip + 1;
    }
    std::uint64_t segmentsPerPlane = 0;
    if (!CheckedMultiply(segmentsAcross, segmentsDown, &segmentsPerPlane) ||
        segmentsPerPlane == 0) {
        return AddReadError(*diagnostics, usdgeo::DiagnosticCode::InconsistentTileLayout,
                            "TIFF segment count overflows", window, options.band);
    }

    auto getSegmentWindow = [&](std::uint64_t segmentIndex,
                                usdraster::RasterWindow* segmentWindow) {
        std::uint64_t segmentX = 0;
        std::uint64_t segmentY = 0;
        std::uint64_t segmentWidth = layout.width;
        std::uint64_t segmentHeight = 0;
        if (layout.tiled) {
            if (!CheckedMultiply(segmentIndex % segmentsAcross, layout.tileWidth,
                                 &segmentX) ||
                !CheckedMultiply(segmentIndex / segmentsAcross, layout.tileHeight,
                                 &segmentY)) {
                return false;
            }
            if (segmentX >= layout.width || segmentY >= layout.height) return false;
            segmentWidth = std::min<std::uint64_t>(layout.tileWidth,
                                                   layout.width - segmentX);
            segmentHeight = std::min<std::uint64_t>(layout.tileHeight,
                                                    layout.height - segmentY);
        } else {
            if (!CheckedMultiply(segmentIndex, layout.rowsPerStrip, &segmentY) ||
                segmentY >= layout.height) {
                return false;
            }
            segmentHeight = std::min<std::uint64_t>(layout.rowsPerStrip,
                                                    layout.height - segmentY);
        }
        *segmentWindow = usdraster::RasterWindow{
            segmentX, segmentY, segmentWidth, segmentHeight};
        return true;
    };

    const std::uint32_t bandIndex = options.band - 1;
    const std::uint32_t sourceBytes = layout.sampleBytes[bandIndex];
    const std::uint64_t sourceOffset = layout.sampleOffsets[bandIndex];
    const std::uint64_t sampleStride = layout.planar == 2
        ? sourceBytes : layout.pixelStride;
    const std::uint64_t rowWidth = layout.tiled ? layout.tileWidth : layout.width;
    std::uint64_t rowStride = 0;
    if (!CheckedMultiply(rowWidth, sampleStride, &rowStride)) {
        return AddReadError(*diagnostics, usdgeo::DiagnosticCode::InconsistentTileLayout,
                            "TIFF decoded segment size overflows", window, options.band);
    }

    const usdraster::RasterSize sampled = usdraster::GetSampledSize(window, step);
    const std::uint64_t sampledCount = sampled.GetPixelCount();
    constexpr std::uint64_t kMaxSize =
        static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max());
    if (sampledCount == std::numeric_limits<std::uint64_t>::max() ||
        sampledCount > kMaxSize / sizeof(double)) {
        return AddReadError(*diagnostics, usdgeo::DiagnosticCode::MemoryBudgetExceeded,
                            "requested raster window is too large to allocate", window, options.band);
    }

    std::uint64_t maxSegmentBytes = 0;
    for (std::uint64_t segmentIndex = 0; segmentIndex < segmentsPerPlane;
         ++segmentIndex) {
        usdraster::RasterWindow segmentWindow;
        if (!getSegmentWindow(segmentIndex, &segmentWindow)) {
            return AddReadError(*diagnostics, usdgeo::DiagnosticCode::InconsistentTileLayout,
                                "TIFF segment geometry overflows", window, options.band);
        }
        if (!segmentWindow.Intersect(window).IsEmpty()) {
            const Segment& segment = layout.segments[static_cast<std::size_t>(
                segmentIndex + (layout.planar == 2
                    ? static_cast<std::uint64_t>(options.band - 1) * segmentsPerPlane
                    : 0))];
            std::uint64_t segmentBytes = segment.byteCount;
            if (layout.compression != 1) {
                std::uint64_t decodedBytes = 0;
                if (!CheckedMultiply(rowStride, segmentWindow.height,
                                     &decodedBytes) ||
                    !CheckedAdd(segmentBytes, decodedBytes, &segmentBytes)) {
                    return AddReadError(*diagnostics,
                                        usdgeo::DiagnosticCode::InconsistentTileLayout,
                                        "TIFF decoded segment size overflows",
                                        window, options.band);
                }
            }
            maxSegmentBytes = std::max(maxSegmentBytes, segmentBytes);
        }
    }
    if (options.memoryBudgetBytes != 0) {
        if (sampledCount > (std::numeric_limits<std::uint64_t>::max() - maxSegmentBytes) /
                               sizeof(double) ||
            sampledCount * sizeof(double) + maxSegmentBytes > options.memoryBudgetBytes) {
            return AddReadError(*diagnostics, usdgeo::DiagnosticCode::MemoryBudgetExceeded,
                                "memory budget cannot satisfy the TIFF window read", window, options.band);
        }
    }
    if (options.isCancelled && options.isCancelled()) {
        return AddReadError(*diagnostics, usdgeo::DiagnosticCode::Cancelled,
                            "TIFF window read was cancelled", window, options.band);
    }

#if defined(USDRASTER_HAS_LIBTIFF)
    LibTiffSource libTiffSource{&_source};
    const std::string identifier = _source.GetIdentifier();
    std::unique_ptr<TIFF, decltype(&TIFFClose)> libTiff(
        nullptr, TIFFClose);
    if (layout.compression != 1) {
        libTiff.reset(TIFFClientOpen(
            identifier.empty() ? "usdGeoTiff" : identifier.c_str(), "r",
            static_cast<thandle_t>(&libTiffSource), LibTiffRead, nullptr,
            LibTiffSeek, LibTiffClose, LibTiffSize, LibTiffMap,
            LibTiffUnmap));
        if (!libTiff) {
            return AddReadError(*diagnostics,
                                usdgeo::DiagnosticCode::SourceUnavailable,
                                "unable to open TIFF through libtiff",
                                window, options.band);
        }
    }
#endif

    try {
        *grid = usdraster::RasterGrid(window, step, options.band, band->dataType,
                                      band->noData);
    } catch (const std::bad_alloc&) {
        return AddReadError(*diagnostics, usdgeo::DiagnosticCode::MemoryBudgetExceeded,
                            "requested raster window could not be allocated", window, options.band);
    }
    if (window.IsEmpty()) return true;
    if (grid->IsEmpty()) {
        return AddReadError(*diagnostics, usdgeo::DiagnosticCode::MemoryBudgetExceeded,
                            "requested raster window could not be allocated", window, options.band);
    }

    if (options.outputType != band->dataType &&
        !usdraster::IsExactlyRepresentable(band->dataType, options.outputType)) {
        AddReadWarning(*diagnostics, usdgeo::DiagnosticCode::LossyConversion,
                       "TIFF window read applies a potentially lossy output conversion",
                       window, options.band);
    }

    std::uint64_t planeOffset = 0;
    if (layout.planar == 2 &&
        !CheckedMultiply(static_cast<std::uint64_t>(bandIndex), segmentsPerPlane,
                         &planeOffset)) {
        return AddReadError(*diagnostics, usdgeo::DiagnosticCode::InconsistentTileLayout,
                            "TIFF plane offset overflows", window, options.band);
    }

    for (std::uint64_t segmentIndex = 0; segmentIndex < segmentsPerPlane; ++segmentIndex) {
        if (options.isCancelled && options.isCancelled()) {
            *grid = usdraster::RasterGrid{};
            return AddReadError(*diagnostics, usdgeo::DiagnosticCode::Cancelled,
                                "TIFF window read was cancelled", window, options.band);
        }
        usdraster::RasterWindow segmentWindow;
        if (!getSegmentWindow(segmentIndex, &segmentWindow)) {
            *grid = usdraster::RasterGrid{};
            return AddReadError(*diagnostics, usdgeo::DiagnosticCode::InconsistentTileLayout,
                                "TIFF segment geometry overflows", window, options.band);
        }
        if (segmentWindow.Intersect(window).IsEmpty()) continue;

        std::uint64_t decodedSegmentBytes = 0;
        if (layout.compression != 1 &&
            !CheckedMultiply(rowStride, segmentWindow.height,
                             &decodedSegmentBytes)) {
            *grid = usdraster::RasterGrid{};
            return AddReadError(*diagnostics,
                                usdgeo::DiagnosticCode::InconsistentTileLayout,
                                "TIFF decoded segment size overflows",
                                window, options.band);
        }

        std::uint64_t sourceSegmentIndex = 0;
        if (!CheckedAdd(segmentIndex, planeOffset, &sourceSegmentIndex)) {
            *grid = usdraster::RasterGrid{};
            return AddReadError(*diagnostics, usdgeo::DiagnosticCode::InconsistentTileLayout,
                                "TIFF segment index overflows", window, options.band);
        }
        if (sourceSegmentIndex >= layout.segments.size()) {
            *grid = usdraster::RasterGrid{};
            return AddReadError(*diagnostics, usdgeo::DiagnosticCode::InconsistentTileLayout,
                                "TIFF segment index is outside the validated layout", window, options.band);
        }
        const Segment& segment = layout.segments[static_cast<std::size_t>(sourceSegmentIndex)];
        if (segment.byteCount > std::numeric_limits<std::size_t>::max()) {
            *grid = usdraster::RasterGrid{};
            return AddReadError(*diagnostics, usdgeo::DiagnosticCode::MemoryBudgetExceeded,
                                "TIFF segment is too large to decode", window, options.band);
        }
        std::vector<std::uint8_t> bytes;
        std::size_t availableBytes = 0;
        bool sampleLittle = layout.little;
#if defined(USDRASTER_HAS_LIBTIFF)
        if (layout.compression != 1) {
            if (decodedSegmentBytes > std::numeric_limits<std::size_t>::max() ||
                decodedSegmentBytes >
                    static_cast<std::uint64_t>(std::numeric_limits<tmsize_t>::max())) {
                *grid = usdraster::RasterGrid{};
                return AddReadError(*diagnostics,
                                    usdgeo::DiagnosticCode::MemoryBudgetExceeded,
                                    "TIFF decoded segment is too large to allocate",
                                    window, options.band);
            }
            try {
                bytes.resize(static_cast<std::size_t>(decodedSegmentBytes));
            } catch (const std::bad_alloc&) {
                *grid = usdraster::RasterGrid{};
                return AddReadError(*diagnostics,
                                    usdgeo::DiagnosticCode::MemoryBudgetExceeded,
                                    "TIFF decoded segment buffer could not be allocated",
                                    window, options.band);
            }
            const auto encodedIndex = static_cast<std::uint64_t>(sourceSegmentIndex);
            tmsize_t decoded = 0;
            if (layout.tiled) {
                if (encodedIndex > std::numeric_limits<ttile_t>::max()) {
                    *grid = usdraster::RasterGrid{};
                    return AddReadError(*diagnostics,
                                        usdgeo::DiagnosticCode::InconsistentTileLayout,
                                        "TIFF tile index is too large for libtiff",
                                        window, options.band);
                }
                decoded = TIFFReadEncodedTile(
                    libTiff.get(), static_cast<ttile_t>(encodedIndex),
                    bytes.data(), static_cast<tmsize_t>(bytes.size()));
            } else {
                if (encodedIndex > std::numeric_limits<tstrip_t>::max()) {
                    *grid = usdraster::RasterGrid{};
                    return AddReadError(*diagnostics,
                                        usdgeo::DiagnosticCode::InconsistentTileLayout,
                                        "TIFF strip index is too large for libtiff",
                                        window, options.band);
                }
                decoded = TIFFReadEncodedStrip(
                    libTiff.get(), static_cast<tstrip_t>(encodedIndex),
                    bytes.data(), static_cast<tmsize_t>(bytes.size()));
            }
            if (decoded < 0 || libTiffSource.failed) {
                *grid = usdraster::RasterGrid{};
                return AddReadError(*diagnostics,
                                    usdgeo::DiagnosticCode::SourceUnavailable,
                                    "unable to decode TIFF pixel segment",
                                    window, options.band);
            }
            availableBytes = static_cast<std::size_t>(decoded);
            sampleLittle = IsLittleEndianHost();
        } else
#endif
        {
            try {
                bytes.resize(static_cast<std::size_t>(segment.byteCount));
            } catch (const std::bad_alloc&) {
                *grid = usdraster::RasterGrid{};
                return AddReadError(*diagnostics, usdgeo::DiagnosticCode::MemoryBudgetExceeded,
                                    "TIFF segment buffer could not be allocated", window, options.band);
            }
            const auto result = _source.Read(segment.offset, bytes.size(), bytes.data());
            if (!result.IsOk() || result.bytesRead != bytes.size()) {
                *grid = usdraster::RasterGrid{};
                const auto code = result.status == usdraster::ReadStatus::ShortRead
                    ? usdgeo::DiagnosticCode::ShortRead
                    : usdgeo::DiagnosticCode::SourceUnavailable;
                return AddReadError(*diagnostics, code,
                                    "unable to read TIFF pixel segment", window, options.band);
            }
            availableBytes = bytes.size();
        }

        for (std::uint64_t outputRow = 0; outputRow < grid->GetSize().height;
             ++outputRow) {
            std::uint64_t sourceY = 0;
            if (!CheckedMultiply(outputRow, step, &sourceY) ||
                !CheckedAdd(window.y, sourceY, &sourceY)) {
                *grid = usdraster::RasterGrid{};
                return AddReadError(*diagnostics, usdgeo::DiagnosticCode::InconsistentTileLayout,
                                    "TIFF source row coordinate overflows", window, options.band);
            }
            if (sourceY < segmentWindow.y || sourceY >= segmentWindow.GetEndY()) continue;
            for (std::uint64_t outputColumn = 0;
                 outputColumn < grid->GetSize().width; ++outputColumn) {
                std::uint64_t sourceX = 0;
                if (!CheckedMultiply(outputColumn, step, &sourceX) ||
                    !CheckedAdd(window.x, sourceX, &sourceX)) {
                    *grid = usdraster::RasterGrid{};
                    return AddReadError(*diagnostics, usdgeo::DiagnosticCode::InconsistentTileLayout,
                                        "TIFF source column coordinate overflows", window, options.band);
                }
                if (sourceX < segmentWindow.x || sourceX >= segmentWindow.GetEndX()) continue;
                const std::uint64_t localColumn = sourceX - segmentWindow.x;
                const std::uint64_t localRow = sourceY - segmentWindow.y;
                std::uint64_t offset = 0;
                std::uint64_t componentOffset = 0;
                if (!CheckedMultiply(localRow, rowStride, &offset) ||
                    !CheckedMultiply(localColumn, sampleStride, &componentOffset) ||
                    !CheckedAdd(offset, componentOffset, &offset) ||
                    !CheckedAdd(offset, sourceOffset, &offset)) {
                    *grid = usdraster::RasterGrid{};
                    return AddReadError(*diagnostics, usdgeo::DiagnosticCode::InconsistentTileLayout,
                                        "TIFF pixel offset overflows", window, options.band);
                }
                if (offset > availableBytes || sourceBytes > availableBytes - offset) {
                    *grid = usdraster::RasterGrid{};
                    return AddReadError(*diagnostics, usdgeo::DiagnosticCode::InconsistentTileLayout,
                                        "TIFF pixel segment is shorter than its layout", window, options.band);
                }
                double value = DecodeSample(bytes.data() + offset, band->dataType, sampleLittle);
                value = band->ApplyScaleAndOffset(value);
                value = ConvertSample(value, options.outputType);
                grid->SetSample(outputColumn, outputRow, value);
            }
        }
    }
    return true;
}

}  // namespace usdgeotiff