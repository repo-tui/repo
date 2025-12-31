#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace repo::domain {

/// A single line in a diff hunk
struct DiffLine {
    enum class Origin {
        Context,  // Unchanged line
        Addition, // Added line (+)
        Deletion  // Deleted line (-)
    };

    Origin origin;
    std::string content;
    std::optional<uint32_t> old_lineno; // Line number in old file
    std::optional<uint32_t> new_lineno; // Line number in new file

    auto operator==(const DiffLine&) const -> bool = default;
};

/// A hunk in a file diff
struct DiffHunk {
    uint32_t old_start; // Starting line in old file
    uint32_t old_lines; // Number of lines in old file
    uint32_t new_start; // Starting line in new file
    uint32_t new_lines; // Number of lines in new file
    std::string header; // Hunk header (@@  ...)

    std::vector<DiffLine> lines;

    auto operator==(const DiffHunk&) const -> bool = default;
};

/// Diff for a single file
struct FileDiff {
    using Path = std::filesystem::path;

    Path path;
    std::optional<Path> old_path; // For renames

    enum class Status {
        Added,
        Deleted,
        Modified,
        Renamed,
        Copied,
        TypeChanged // File type changed (file->symlink, etc)
    };
    Status status;

    bool is_binary;
    std::vector<DiffHunk> hunks;

    // Statistics
    size_t additions;
    size_t deletions;

    auto operator==(const FileDiff&) const -> bool = default;
};

} // namespace repo::domain
