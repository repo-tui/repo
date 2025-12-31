#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "../result.hpp"

namespace repo {
class Repository;

namespace ops {

struct MergeParams {
    enum class Strategy {
        FastForward,    // Only allow fast-forward (no merge commit)
        NoFastForward,  // Always create merge commit
        FastForwardOnly // Fail if fast-forward not possible
    };

    std::string source; // Branch name, commit ID, or ref to merge
    Strategy strategy = Strategy::FastForward;
    bool commit = true;  // Create merge commit (false = stage only)
    std::string message; // Custom merge commit message (empty = auto)
};

struct MergeResult {
    enum class Status {
        FastForward, // Fast-forwarded (no merge commit created)
        MergeCommit, // Merge commit created
        Conflicts,   // Merge has conflicts (not committed)
        UpToDate,    // Already up to date
        Staged       // Merge staged but not committed (commit=false)
    };

    Status status;
    std::string commit_id;                        // ID of merge commit (if created)
    std::vector<std::filesystem::path> conflicts; // Conflicted files
    size_t files_changed = 0;
};

auto merge(Repository& repo, MergeParams params) -> Result<MergeResult>;

} // namespace ops
} // namespace repo
