#ifndef USDGEO_DIAGNOSTIC_H
#define USDGEO_DIAGNOSTIC_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace usdgeo {

enum class Severity { Warning, Error };

/// The shared classification vocabulary. Codes are stable once published: a
/// name is never reused for a different meaning, and there is deliberately no
/// generic `Unknown` -- every failure path names its condition. See
/// docs/architecture/DIAGNOSTICS.md.
enum class DiagnosticCode {
    // source and container
    InvalidSignature,
    UnsupportedVersion,
    TruncatedHeader,
    TruncatedData,
    InvalidOffset,
    ShortRead,
    SourceUnavailable,

    // raster structure
    UnsupportedCompression,
    UnsupportedSampleFormat,
    UnsupportedPlanarConfiguration,
    InvalidRasterSize,
    InvalidBandIndex,
    InconsistentTileLayout,

    // georeferencing
    MissingGeoreference,
    InvalidGeoTransform,
    ConflictingGeoTransform,
    UnknownPixelAnchor,
    InvalidCrs,
    ConflictingCrs,
    UnsupportedCrs,

    // values
    NonFiniteValue,
    InvalidNoDataValue,
    LossyConversion,

    // request
    WindowOutOfBounds,
    UnsupportedOverviewLevel,
    MemoryBudgetExceeded,
    Cancelled,

    // arguments
    UnknownFormatArgument,
    UnsupportedFormatArgument,
    InvalidFormatArgument,
    ConflictingFormatArguments,

    // authoring
    AuthoringFailed,
    VertexBudgetExceeded
};

/// A rectangle in source pixel coordinates, carried by a diagnostic to say
/// which region a failure is about.
///
/// This duplicates the shape of `usdraster::RasterWindow` deliberately.
/// `usdGeoCore` may not depend on `usdRasterCore` -- section 2 of
/// docs/architecture/WORKSPACE.md forbids it depending on anything -- so the
/// anchor is its own plain rectangle with no arithmetic, and `RasterWindow`
/// converts to it through `RasterWindow::ToAnchor`. The window type keeps the
/// window arithmetic; this type only identifies a region in a message.
struct PixelWindow {
    std::uint64_t x = 0;
    std::uint64_t y = 0;
    std::uint64_t width = 0;
    std::uint64_t height = 0;
};

inline bool operator==(const PixelWindow& left, const PixelWindow& right) {
    return left.x == right.x && left.y == right.y &&
           left.width == right.width && left.height == right.height;
}

/// A typed failure or warning.
///
/// The anchors are the raster-specific part, and they are what makes a
/// diagnostic actionable on a source 40000 pixels wide. Each is present only
/// when it is meaningful: a truncated header has a byte offset and no pixel, a
/// window read that exceeds a memory budget has a window and no byte offset.
///
/// A message never contains a credential, a token, an authorization header, or
/// a signed URL. That is invariant 18 of the workspace contract, and it
/// applies here because diagnostics are the surface most likely to be logged.
struct Diagnostic {
    DiagnosticCode code = DiagnosticCode::SourceUnavailable;
    Severity severity = Severity::Error;
    std::string message;

    std::optional<std::uint64_t> byteOffset;
    std::optional<std::uint64_t> pixelX;
    std::optional<std::uint64_t> pixelY;
    std::optional<PixelWindow> window;
    std::optional<std::uint32_t> band;

    bool IsError() const { return severity == Severity::Error; }
};

/// The stable spelling of a code, for messages and tests. Never localized:
/// this is an identifier, not display text.
const char* GetDiagnosticCodeName(DiagnosticCode code);

/// Collects diagnostics for a single operation.
///
/// A sink is not a logger. It accumulates so a caller can report every problem
/// with a source rather than only the first, which is what makes a malformed
/// file diagnosable in one pass instead of one fix at a time.
class DiagnosticSink {
public:
    void Add(Diagnostic diagnostic);

    void AddError(DiagnosticCode code, std::string message);
    void AddWarning(DiagnosticCode code, std::string message);

    const std::vector<Diagnostic>& GetDiagnostics() const { return _diagnostics; }

    /// True once any diagnostic with `Severity::Error` has been added. A
    /// warning must leave a result the caller can still use; if a condition
    /// prevents that, it is an error. See rule 3 of the diagnostics contract.
    bool HasError() const { return _errorCount > 0; }

    std::size_t GetErrorCount() const { return _errorCount; }
    std::size_t GetWarningCount() const {
        return _diagnostics.size() - _errorCount;
    }

    bool IsEmpty() const { return _diagnostics.empty(); }
    void Clear();

private:
    std::vector<Diagnostic> _diagnostics;
    std::size_t _errorCount = 0;
};

}  // namespace usdgeo

#endif  // USDGEO_DIAGNOSTIC_H
