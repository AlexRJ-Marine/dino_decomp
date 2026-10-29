#include "atomic_file.h"

#include <stdexcept>

namespace dino {

AtomicFile::AtomicFile(const std::string& path, FileMode mode)
    : target_(path), temp_(target_) {
    temp_ += ".tmp";
    if (!target_.parent_path().empty()) {
        std::filesystem::create_directories(target_.parent_path());
    }
    std::ios::openmode flags = std::ios::out | std::ios::trunc;
    if (mode == FileMode::Binary) {
        flags |= std::ios::binary;
    }
    out_.open(temp_, flags);
    if (!out_) {
        throw std::runtime_error("cannot write " + temp_.string());
    }
}

AtomicFile::~AtomicFile() {
    if (committed_) {
        return;
    }
    out_.close();
    std::error_code ignored;
    std::filesystem::remove(temp_, ignored);
}

void AtomicFile::commit() {
    out_.flush();
    if (!out_) {
        throw std::runtime_error("cannot write " + temp_.string());
    }
    out_.close();
    if (!out_) {
        throw std::runtime_error("cannot write " + temp_.string());
    }
    std::error_code error;
    std::filesystem::rename(temp_, target_, error);
    if (error) {
        // Windows' rename can refuse to replace an existing target. Removing
        // it first is not atomic, but it still avoids a truncated save.
        std::error_code ignored;
        std::filesystem::remove(target_, ignored);
        error.clear();
        std::filesystem::rename(temp_, target_, error);
    }
    if (error) {
        std::error_code ignored;
        std::filesystem::remove(temp_, ignored);
        throw std::runtime_error("cannot replace " + target_.string() + ": " +
                                 error.message());
    }
    committed_ = true;
}

}  // namespace dino
