#include "usdgeo/CrsDescription.h"

namespace usdgeo {

double GetMetresPerUnit(LinearUnit unit) {
    switch (unit) {
        case LinearUnit::Metre: return 1.0;
        case LinearUnit::Foot: return 0.3048;
        // 1200/3937 exactly. Written as the quotient rather than as a rounded
        // decimal so the value is the one the definition gives.
        case LinearUnit::USSurveyFoot: return 1200.0 / 3937.0;
        case LinearUnit::Unknown: return 0.0;
    }
    return 0.0;
}

const char* GetLinearUnitName(LinearUnit unit) {
    switch (unit) {
        case LinearUnit::Metre: return "metre";
        case LinearUnit::Foot: return "foot";
        case LinearUnit::USSurveyFoot: return "USSurveyFoot";
        case LinearUnit::Unknown: return "unknown";
    }
    return "unknown";
}

const char* GetAngularUnitName(AngularUnit unit) {
    switch (unit) {
        case AngularUnit::Degree: return "degree";
        case AngularUnit::Radian: return "radian";
        case AngularUnit::Unknown: return "unknown";
    }
    return "unknown";
}

const char* GetCrsKindName(CrsKind kind) {
    switch (kind) {
        case CrsKind::Projected: return "projected";
        case CrsKind::Geographic: return "geographic";
        case CrsKind::Unknown: return "unknown";
    }
    return "unknown";
}

bool CrsDescription::IsEmpty() const {
    return kind == CrsKind::Unknown && !epsgCode.has_value() && wkt.empty() &&
           citation.empty() && linearUnit == LinearUnit::Unknown &&
           linearUnitMetres == 0.0 && angularUnit == AngularUnit::Unknown &&
           !verticalEpsgCode.has_value() &&
           verticalUnit == LinearUnit::Unknown && verticalCitation.empty();
}

bool CrsDescription::IsMetric() const {
    if (kind != CrsKind::Projected) {
        return false;
    }
    return linearUnit != LinearUnit::Unknown || linearUnitMetres > 0.0;
}

bool CrsDescription::HasMixedUnits() const {
    if (verticalUnit == LinearUnit::Unknown ||
        linearUnit == LinearUnit::Unknown) {
        return false;
    }
    return verticalUnit != linearUnit;
}

bool operator==(const CrsDescription& left, const CrsDescription& right) {
    return left.kind == right.kind && left.epsgCode == right.epsgCode &&
           left.wkt == right.wkt && left.citation == right.citation &&
           left.linearUnit == right.linearUnit &&
           left.linearUnitMetres == right.linearUnitMetres &&
           left.angularUnit == right.angularUnit &&
           left.verticalEpsgCode == right.verticalEpsgCode &&
           left.verticalUnit == right.verticalUnit &&
           left.verticalCitation == right.verticalCitation;
}

}  // namespace usdgeo
