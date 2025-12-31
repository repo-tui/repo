#pragma once

#include <repo/domain/diff.hpp>
#include <repo/domain/file_status.hpp>
#include <repo/error.hpp>
#include <repo/tui/tea/cmd.hpp>
#include <repo/tui/tea/msg.hpp>

#include <optional>
#include <string>
#include <vector>

namespace repo::tui::models {

// Diff view mode
enum class DiffMode {
    Normal, // Normal diff browsing
    Help    // Showing help
};

// Diff type to display
enum class DiffType {
    Unstaged, // Working tree vs index (changes not staged)
    Staged,   // Index vs HEAD (changes staged for commit)
    All       // All changes (staged + unstaged)
};

// Diff view model
struct DiffModel {
    // Diff data
    std::vector<domain::FileDiff> file_diffs;

    // Navigation state
    size_t selected_file_index = 0; // Which file is selected
    size_t selected_hunk_index = 0; // Which hunk in the current file
    size_t selected_line_index = 0; // Which line in the current hunk
    size_t scroll_offset = 0;       // For scrolling through large diffs

    // View state
    DiffMode mode = DiffMode::Normal;
    DiffType diff_type = DiffType::Unstaged;
    bool show_line_numbers = true;
    size_t context_lines = 3;

    // Loading state
    bool is_loading = false;
    std::optional<Error> error;

    // UI state
    bool show_help = false;
    std::optional<std::string> notification; // Temporary notification message

    // Repository path
    std::string repo_path;

    // Optional: filter by specific file path
    std::optional<std::filesystem::path> file_filter;
};

// Initialize the diff model
auto init_diff(std::string repo_path, DiffType type = DiffType::Unstaged)
    -> std::pair<DiffModel, tea::CmdBatch>;

// Update function - processes messages and returns new model + commands
auto update_diff(DiffModel model, tea::Msg msg) -> std::pair<DiffModel, tea::CmdBatch>;

// Helper: Get currently selected file diff
auto selected_file_diff(const DiffModel& model) -> std::optional<domain::FileDiff>;

// Helper: Get currently selected hunk
auto selected_hunk(const DiffModel& model) -> std::optional<domain::DiffHunk>;

// Helper: Get currently selected line
auto selected_line(const DiffModel& model) -> std::optional<domain::DiffLine>;

// Commands for async operations

// Load diff based on type
auto cmd_load_diff(std::string repo_path, DiffType type) -> tea::CmdBatch;

// Stage a specific hunk
auto cmd_stage_hunk(std::string repo_path, std::filesystem::path file_path, domain::DiffHunk hunk)
    -> tea::CmdBatch;

// Unstage a specific hunk
auto cmd_unstage_hunk(std::string repo_path, std::filesystem::path file_path, domain::DiffHunk hunk)
    -> tea::CmdBatch;

// Stage specific lines
auto cmd_stage_lines(std::string repo_path, std::filesystem::path file_path,
                     std::vector<domain::DiffLine> lines) -> tea::CmdBatch;

// Unstage specific lines
auto cmd_unstage_lines(std::string repo_path, std::filesystem::path file_path,
                       std::vector<domain::DiffLine> lines) -> tea::CmdBatch;

} // namespace repo::tui::models
