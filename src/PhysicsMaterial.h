#pragma once

#include <string>

/**
 * PhysicsMaterialLibrary provides a lightweight runtime registry that maps
 * human-readable material names (e.g. "wood", "steel") to stable integer IDs.
 * These IDs can be stored in Box2D shape metadata and later converted back to
 * names when triggering effects such as sound playback.
 */
class PhysicsMaterialLibrary {
public:
    /**
     * Get (or lazily create) the integer ID associated with a material name.
     * Material names are treated case-insensitively. Unknown names are added
     * to the registry automatically.
     */
    static int getMaterialId(const std::string& name);

    /**
     * Retrieve the canonical name for a material ID. Returns "unknown" if the
     * ID is not currently registered.
     */
    static const std::string& getMaterialName(int materialId);

    /**
     * Check if a material ID is registered.
     */
    static bool isValid(int materialId);

private:
    static void ensureInitialized();
};

