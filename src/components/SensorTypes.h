#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class SenseType : uint8_t {
    None = 0,
    Collision = 1 << 0,
    Distance = 1 << 1,
    InteractInput = 1 << 2,
    InputActivity = 1 << 3,
    BoxZone = 1 << 4,
    GlobalValue = 1 << 5,
};

using SenseTypeMask = uint8_t;

SenseTypeMask makeSenseMask(SenseType type);
SenseTypeMask addSenseToMask(SenseTypeMask mask, SenseType type);
bool senseMaskHas(SenseTypeMask mask, SenseType type);
SenseTypeMask senseMaskFromStrings(const std::vector<std::string>& senseNames);
std::vector<std::string> senseMaskToStrings(SenseTypeMask mask);
std::string senseTypeToString(SenseType type);
SenseType parseSenseType(const std::string& value);


