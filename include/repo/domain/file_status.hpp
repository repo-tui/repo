#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

namespace repo::domain {

/// File status in working tree and index
struct FileStatus {
    using Path = std::filesystem::path;

    Path path;

    /// File state
    enum class State {
        Untracked,   // Not in Git
        Ignored,     // Matched by .gitignore
        Unmodified,  // Same as HEAD
        Modified,    // Content changed
        Added,       // Newly added to index
        Deleted,     // Deleted
        Renamed,     // Renamed
        Copied,      // Copied
        TypeChanged, // File type changed (file->symlink, etc)
        Conflicted,  // Has merge conflicts
    };

    State index_status;    // Status in index vs HEAD
    State worktree_status; // Status in worktree vs index

    // For renames/copies
    std::optional<Path> old_path;
    std::optional<uint8_t> similarity; // 0-100%

    /// Check if file is staged (in index)
    [[nodiscard]] auto is_staged() const -> bool {
        return index_status != State::Unmodified && index_status != State::Untracked &&
               index_status != State::Ignored;
    }

    /// Check if file has unstaged changes
    [[nodiscard]] auto is_unstaged() const -> bool {
        return worktree_status != State::Unmodified && worktree_status != State::Untracked &&
               worktree_status != State::Ignored;
    }

    /// Check if file is untracked
    [[nodiscard]] auto is_untracked() const -> bool {
        return index_status == State::Untracked && worktree_status == State::Untracked;
    }

    /// Check if file is ignored
    [[nodiscard]] auto is_ignored() const -> bool { return worktree_status == State::Ignored; }

    /// Check if file has conflicts
    [[nodiscard]] auto is_conflicted() const -> bool {
        return index_status == State::Conflicted || worktree_status == State::Conflicted;
    }

    auto operator==(const FileStatus&) const -> bool = default;
};

} // namespace repo::domain
