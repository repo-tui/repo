#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

#include "object_id.hpp"

namespace repo::domain {

/// Entry in the Git index
struct IndexEntry {
    using Path = std::filesystem::path;

    Path path;
    ObjectId oid;
    uint32_t mode;
    std::chrono::system_clock::time_point mtime;
    size_t file_size;

    auto operator==(const IndexEntry&) const -> bool = default;
};

/// Conflict entry (3-way merge)
struct ConflictEntry {
    using Path = std::filesystem::path;

    Path path;
    std::optional<IndexEntry> ancestor; // Common ancestor version
    std::optional<IndexEntry> ours;     // Our version
    std::optional<IndexEntry> theirs;   // Their version

    auto operator==(const ConflictEntry&) const -> bool = default;
};

} // namespace repo::domain
