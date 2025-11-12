#include "SensorTypes.h"

#include <algorithm>
#include <cctype>

namespace {

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

} // namespace

SenseTypeMask makeSenseMask(SenseType type) {
    return static_cast<SenseTypeMask>(type);
}

SenseTypeMask addSenseToMask(SenseTypeMask mask, SenseType type) {
    return static_cast<SenseTypeMask>(mask | makeSenseMask(type));
}

bool senseMaskHas(SenseTypeMask mask, SenseType type) {
    return (mask & makeSenseMask(type)) == makeSenseMask(type);
}

SenseType parseSenseType(const std::string& value) {
    const std::string lowered = toLower(value);
    if (lowered == "collision" || lowered == "touch") {
        return SenseType::Collision;
    }
    if (lowered == "distance" || lowered == "proximity") {
        return SenseType::Distance;
    }
    if (lowered == "interact" || lowered == "input" || lowered == "button") {
        return SenseType::InteractInput;
    }
    return SenseType::None;
}

SenseTypeMask senseMaskFromStrings(const std::vector<std::string>& senseNames) {
    SenseTypeMask mask = makeSenseMask(SenseType::None);
    for (const auto& name : senseNames) {
        const SenseType sense = parseSenseType(name);
        if (sense != SenseType::None) {
            mask = addSenseToMask(mask, sense);
        }
    }
    return mask;
}

std::string senseTypeToString(SenseType type) {
    switch (type) {
        case SenseType::Collision:
            return "collision";
        case SenseType::Distance:
            return "distance";
        case SenseType::InteractInput:
            return "interact";
        case SenseType::None:
        default:
            return "none";
    }
}

std::vector<std::string> senseMaskToStrings(SenseTypeMask mask) {
    std::vector<std::string> values;
    if (senseMaskHas(mask, SenseType::Collision)) {
        values.push_back(senseTypeToString(SenseType::Collision));
    }
    if (senseMaskHas(mask, SenseType::Distance)) {
        values.push_back(senseTypeToString(SenseType::Distance));
    }
    if (senseMaskHas(mask, SenseType::InteractInput)) {
        values.push_back(senseTypeToString(SenseType::InteractInput));
    }
    if (values.empty()) {
        values.push_back(senseTypeToString(SenseType::None));
    }
    return values;
}


