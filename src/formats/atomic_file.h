#pragma once

#include <filesystem>
#include <fstream>
#include <string>

namespace dino {

enum class FileMode { Text, Binary };

// Writes through a sibling `<path>.tmp` and renames it on commit. A crash,
// full disk or kill leaves either the old file or the new one, never a
// truncated target (P.8/R.1). Destruction before commit removes the temp file.
class AtomicFile {
public:
    explicit AtomicFile(const std::string& path, FileMode mode = FileMode::Text);
    ~AtomicFile();

    AtomicFile(const AtomicFile&) = delete;
    AtomicFile& operator=(const AtomicFile&) = delete;
    AtomicFile(AtomicFile&&) = delete;
    AtomicFile& operator=(AtomicFile&&) = delete;

    std::ostream& stream() { return out_; }
    void commit();

private:
    std::filesystem::path target_;
    std::filesystem::path temp_;
    std::ofstream out_;
    bool committed_ = false;
};

}  // namespace dino
