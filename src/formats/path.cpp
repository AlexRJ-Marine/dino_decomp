#include "path.h"

#include <algorithm>
#include <filesystem>

namespace dino {

std::string asset_path(const std::string& root, const std::string& path) {
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    const std::filesystem::path candidate(normalized);
    if (candidate.is_absolute()) {
        return candidate.string();
    }
    return (std::filesystem::path(root) / candidate).string();
}

}  // namespace dino
