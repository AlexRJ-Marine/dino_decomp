#pragma once

#include <string>

namespace dino {

// Joins a retail path (backslashes) onto an asset root. Absolute paths pass
// through unchanged.
std::string asset_path(const std::string& root, const std::string& path);

}  // namespace dino
