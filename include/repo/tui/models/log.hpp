#pragma once

#include <repo/domain/commit.hpp>
#include <repo/error.hpp>
#include <repo/tui/tea/cmd.hpp>
#include <repo/tui/tea/msg.hpp>

#include <optional>
#include <string>
#include <vector>

namespace repo::tui::models {

// Log view mode
enum class LogMode {
    Normal,  // Normal browsing
    Details, // Showing commit details
    Help     // Showing help
};

// Log display format
enum class LogFormat {
    Compact, // One line per commit
    Medium,  // Commit hash, author, date, message
    Full     // Full commit details
};

// Log view model
struct LogModel {
    // Commit data
    std::vector<domain::Commit> commits;
    bool has_more = false; // Are there more commits to load?

    // Selection and navigation
    size_t selected_index = 0;
    size_t page_size = 50; // Number of commits to load at once

    // View state
    LogMode mode = LogMode::Normal;
    LogFormat format = LogFormat::Medium;

    // Commit details (when in Details mode)
    std::optional<domain::Commit> detail_commit;
    std::optional<std::string> detail_diff; // Full diff for selected commit

    // Loading state
    bool is_loading = false;
    bool is_loading_more = false; // Loading additional commits
    std::optional<Error> error;

    // UI state
    bool show_help = false;
    std::optional<std::string> notification; // Temporary notification message

    // Repository path
    std::string repo_path;

    // Filter options (optional)
    std::optional<std::string> branch_filter; // Only show commits from this branch
    std::optional<std::string> path_filter;   // Only show commits affecting this path
};

// Initialize the log model
auto init_log(std::string repo_path) -> std::pair<LogModel, tea::CmdBatch>;

// Update function - processes messages and returns new model + commands
auto update_log(LogModel model, tea::Msg msg) -> std::pair<LogModel, tea::CmdBatch>;

// Helper: Get currently selected commit
auto selected_commit(const LogModel& model) -> std::optional<domain::Commit>;

// Commands for async operations

// Load initial commits
auto cmd_load_log(std::string repo_path, size_t max_count,
                  std::optional<std::string> branch = std::nullopt) -> tea::CmdBatch;

// Load more commits (pagination)
auto cmd_load_more_commits(std::string repo_path, size_t skip, size_t max_count) -> tea::CmdBatch;

// Load commit details with diff
auto cmd_load_commit_details(std::string repo_path, domain::ObjectId commit_id) -> tea::CmdBatch;

// Cherry-pick commit (select_commit operation)
auto cmd_cherry_pick_commit(std::string repo_path, domain::ObjectId commit_id) -> tea::CmdBatch;

// Create branch from commit
auto cmd_create_branch_at_commit(std::string repo_path, std::string branch_name,
                                 domain::ObjectId commit_id) -> tea::CmdBatch;

// Checkout commit
auto cmd_checkout_commit(std::string repo_path, domain::ObjectId commit_id) -> tea::CmdBatch;

} // namespace repo::tui::models
