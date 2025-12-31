#pragma once

#include <repo/domain/stash.hpp>
#include <repo/error.hpp>
#include <repo/tui/tea/cmd.hpp>
#include <repo/tui/tea/msg.hpp>

#include <optional>
#include <string>
#include <vector>

namespace repo::tui::models {

// Stash view mode
enum class StashMode {
    Normal,       // Normal browsing
    CreateInput,  // Creating new stash (entering message)
    ApplyConfirm, // Confirming stash apply
    PopConfirm,   // Confirming stash pop
    DropConfirm,  // Confirming stash drop
    Details,      // Showing stash details
    Help          // Showing help
};

// Stash view model
struct StashModel {
    // Stash data
    std::vector<domain::Stash> stashes;

    // Selection and navigation
    size_t selected_index = 0;

    // View state
    StashMode mode = StashMode::Normal;

    // Create stash input state
    std::string input_message;
    size_t input_cursor_pos = 0;
    bool include_untracked = false;
    bool keep_index = false;

    // Confirmation state
    std::optional<domain::Stash> pending_operation_stash;
    bool reinstate_index = false; // For apply/pop operations

    // Details view state
    std::optional<domain::Stash> detail_stash;
    std::optional<std::string> detail_diff; // Diff of stashed changes

    // Loading state
    bool is_loading = false;
    std::optional<Error> error;

    // UI state
    std::optional<std::string> notification; // Temporary notification message

    // Repository path
    std::string repo_path;
};

// Initialize the stash model
auto init_stash(std::string repo_path) -> std::pair<StashModel, tea::CmdBatch>;

// Update function - processes messages and returns new model + commands
auto update_stash(StashModel model, tea::Msg msg) -> std::pair<StashModel, tea::CmdBatch>;

// Helper: Get currently selected stash
auto selected_stash(const StashModel& model) -> std::optional<domain::Stash>;

// Commands for async operations

// Load all stashes
auto cmd_load_stashes(std::string repo_path) -> tea::CmdBatch;

// Create a new stash
auto cmd_create_stash(std::string repo_path, std::string message, std::string stasher_name,
                      std::string stasher_email, bool include_untracked = false,
                      bool keep_index = false) -> tea::CmdBatch;

// Apply a stash (without removing it)
auto cmd_apply_stash(std::string repo_path, size_t index, bool reinstate_index = false)
    -> tea::CmdBatch;

// Pop a stash (apply and remove)
auto cmd_pop_stash(std::string repo_path, size_t index, bool reinstate_index = false)
    -> tea::CmdBatch;

// Drop a stash (remove without applying)
auto cmd_drop_stash(std::string repo_path, size_t index) -> tea::CmdBatch;

// Load stash details (diff)
auto cmd_load_stash_details(std::string repo_path, domain::Stash stash) -> tea::CmdBatch;

} // namespace repo::tui::models
