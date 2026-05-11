#pragma once

#include <filesystem>
#include <string>

#include "DxfTypes.h"

namespace ModaDxf {

DxfModel ParseDxfText(const std::string& text);
DxfModel ParseDxfFile(const std::filesystem::path& filePath);

}  // namespace ModaDxf
