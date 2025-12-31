#pragma once

#include <filesystem>
#include <vector>

#include "../result.hpp"

namespace repo {
class Repository;

namespace ops {

struct CleanParams {
    bool dry_run = false;             // Don't actually delete, just show what would be deleted
    bool include_directories = false; // Remove untracked directories too
    bool force = false;               // Required for actual deletion (safety check)
    bool include_ignored = false;     // Also remove ignored files
};

struct CleanResult {
    std::vector<std::filesystem::path> removed_files;
    std::vector<std::filesystem::path> removed_directories;

    [[nodiscard]] auto total_removed() const -> size_t {
        return removed_files.size() + removed_directories.size();
    }
};

auto clean(Repository& repo, CleanParams params) -> Result<CleanResult>;

} // namespace ops
} // namespace repo
