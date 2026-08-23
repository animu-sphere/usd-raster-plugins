#include "usdgeo/Diagnostic.h"

#include <utility>

namespace usdgeo {

const char* GetDiagnosticCodeName(DiagnosticCode code) {
    switch (code) {
        case DiagnosticCode::InvalidSignature: return "InvalidSignature";
        case DiagnosticCode::UnsupportedVersion: return "UnsupportedVersion";
        case DiagnosticCode::TruncatedHeader: return "TruncatedHeader";
        case DiagnosticCode::TruncatedData: return "TruncatedData";
        case DiagnosticCode::InvalidOffset: return "InvalidOffset";
        case DiagnosticCode::ShortRead: return "ShortRead";
        case DiagnosticCode::SourceUnavailable: return "SourceUnavailable";

        case DiagnosticCode::UnsupportedCompression:
            return "UnsupportedCompression";
        case DiagnosticCode::UnsupportedSampleFormat:
            return "UnsupportedSampleFormat";
        case DiagnosticCode::UnsupportedPlanarConfiguration:
            return "UnsupportedPlanarConfiguration";
        case DiagnosticCode::InvalidRasterSize: return "InvalidRasterSize";
        case DiagnosticCode::InvalidBandIndex: return "InvalidBandIndex";
        case DiagnosticCode::InconsistentTileLayout:
            return "InconsistentTileLayout";

        case DiagnosticCode::MissingGeoreference: return "MissingGeoreference";
        case DiagnosticCode::InvalidGeoTransform: return "InvalidGeoTransform";
        case DiagnosticCode::ConflictingGeoTransform:
            return "ConflictingGeoTransform";
        case DiagnosticCode::UnknownPixelAnchor: return "UnknownPixelAnchor";
        case DiagnosticCode::InvalidCrs: return "InvalidCrs";
        case DiagnosticCode::ConflictingCrs: return "ConflictingCrs";
        case DiagnosticCode::UnsupportedCrs: return "UnsupportedCrs";

        case DiagnosticCode::NonFiniteValue: return "NonFiniteValue";
        case DiagnosticCode::InvalidNoDataValue: return "InvalidNoDataValue";
        case DiagnosticCode::LossyConversion: return "LossyConversion";

        case DiagnosticCode::WindowOutOfBounds: return "WindowOutOfBounds";
        case DiagnosticCode::UnsupportedOverviewLevel:
            return "UnsupportedOverviewLevel";
        case DiagnosticCode::MemoryBudgetExceeded:
            return "MemoryBudgetExceeded";
        case DiagnosticCode::Cancelled: return "Cancelled";

        case DiagnosticCode::UnknownFormatArgument:
            return "UnknownFormatArgument";
        case DiagnosticCode::UnsupportedFormatArgument:
            return "UnsupportedFormatArgument";
        case DiagnosticCode::InvalidFormatArgument:
            return "InvalidFormatArgument";
        case DiagnosticCode::ConflictingFormatArguments:
            return "ConflictingFormatArguments";

        case DiagnosticCode::AuthoringFailed: return "AuthoringFailed";
        case DiagnosticCode::VertexBudgetExceeded:
            return "VertexBudgetExceeded";
    }
    // Unreachable for a value of the enumeration. Reaching it means a code was
    // added without a name, which is a build-time omission, not a runtime
    // condition, so it does not get a diagnostic of its own.
    return "";
}

void DiagnosticSink::Add(Diagnostic diagnostic) {
    if (diagnostic.severity == Severity::Error) {
        ++_errorCount;
    }
    _diagnostics.push_back(std::move(diagnostic));
}

void DiagnosticSink::AddError(DiagnosticCode code, std::string message) {
    Diagnostic diagnostic;
    diagnostic.code = code;
    diagnostic.severity = Severity::Error;
    diagnostic.message = std::move(message);
    Add(std::move(diagnostic));
}

void DiagnosticSink::AddWarning(DiagnosticCode code, std::string message) {
    Diagnostic diagnostic;
    diagnostic.code = code;
    diagnostic.severity = Severity::Warning;
    diagnostic.message = std::move(message);
    Add(std::move(diagnostic));
}

void DiagnosticSink::Clear() {
    _diagnostics.clear();
    _errorCount = 0;
}

}  // namespace usdgeo
