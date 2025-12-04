#pragma once

#include <string>
#include <vector>

#include "level_editor/SchemaTypes.h"

namespace level_editor {

class SchemaScanner {
public:
    static void PopulateFromCode(const std::vector<std::string>& searchRoots, SchemaCatalog& catalog);

private:
    static void ScanFile(const std::string& componentType, const std::string& path, SchemaCatalog& catalog);
};

}  // namespace level_editor

