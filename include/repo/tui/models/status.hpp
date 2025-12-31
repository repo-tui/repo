#pragma once

#include <repo/domain/file_status.hpp>
#include <repo/error.hpp>
#include <repo/tui/tea/cmd.hpp>
#include <repo/tui/tea/msg.hpp>

#include <optional>
#include <string>
#include <vector>

namespace repo::tui::models {

// Status view mode
enum class StatusMode {
    Normal,      // Normal browsing
    CommitInput, // Entering commit message
    Help         // Showing help
};

// File filter options
enum class FileFilter {
    All,      // Show all files
    Staged,   // Show only staged files
    Unstaged, // Show only unstaged files
    Untracked // Show only untracked files
};

// Status view model
struct StatusModel {
    // File data
    std::vector<domain::FileStatus> files;
    std::vector<domain::FileStatus> filtered_files; // After applying filter

    // Selection and navigation
    size_t selected_index = 0;
    std::vector<size_t> multi_selection; // For multi-file operations

    // View state
    StatusMode mode = StatusMode::Normal;
    FileFilter filter = FileFilter::All;
    bool show_unstaged = true;
    bool show_staged = true;

    // Commit input state
    std::string commit_message;
    size_t commit_cursor_pos = 0;

    // Loading state
    bool is_loading = false;
    std::optional<Error> error;

    // UI state
    bool show_help = false;
    std::optional<std::string> notification; // Temporary notification message

    // Repository path (needed for operations)
    std::string repo_path;
};

// Initialize the status model
auto init_status(std::string repo_path) -> std::pair<StatusModel, tea::CmdBatch>;

// Update function - processes messages and returns new model + commands
auto update_status(StatusModel model, tea::Msg msg) -> std::pair<StatusModel, tea::CmdBatch>;

// Helper: Apply current filter to files
auto apply_filter(StatusModel& model) -> void;

// Helper: Get currently selected file
auto selected_file(const StatusModel& model) -> std::optional<domain::FileStatus>;

// Helper: Get all selected files (single or multi)
auto selected_files(const StatusModel& model) -> std::vector<domain::FileStatus>;

// Commands for async operations

// Load status from repository
auto cmd_load_status(std::string repo_path) -> tea::CmdBatch;

// Stage a file
auto cmd_stage_file(std::string repo_path, std::filesystem::path file_path) -> tea::CmdBatch;

// Unstage a file
auto cmd_unstage_file(std::string repo_path, std::filesystem::path file_path) -> tea::CmdBatch;

// Commit staged changes
auto cmd_commit(std::string repo_path, std::string message) -> tea::CmdBatch;

// Stage all files
auto cmd_stage_all(std::string repo_path, std::vector<domain::FileStatus> files) -> tea::CmdBatch;

// Unstage all files
auto cmd_unstage_all(std::string repo_path, std::vector<domain::FileStatus> files) -> tea::CmdBatch;

} // namespace repo::tui::models
