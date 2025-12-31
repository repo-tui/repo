#pragma once

#include <repo/domain/branch.hpp>
#include <repo/error.hpp>
#include <repo/tui/tea/cmd.hpp>
#include <repo/tui/tea/msg.hpp>

#include <optional>
#include <string>
#include <vector>

namespace repo::tui::models {

// Branch view mode
enum class BranchMode {
    Normal,        // Normal browsing
    CreateInput,   // Creating new branch (entering name)
    RenameInput,   // Renaming branch (entering new name)
    DeleteConfirm, // Confirming branch deletion
    MergeConfirm,  // Confirming branch merge
    Help           // Showing help
};

// Branch filter options
enum class BranchFilter {
    All,   // Show all branches (local + remote)
    Local, // Show only local branches
    Remote // Show only remote branches
};

// Branch view model
struct BranchModel {
    // Branch data
    std::vector<domain::Branch> branches;
    std::vector<domain::Branch> filtered_branches; // After applying filter
    std::optional<std::string> current_branch;     // Name of current branch

    // Selection and navigation
    size_t selected_index = 0;

    // View state
    BranchMode mode = BranchMode::Normal;
    BranchFilter filter = BranchFilter::All;

    // Input state (for create/rename)
    std::string input_text;
    size_t input_cursor_pos = 0;

    // Delete/merge confirmation state
    std::optional<domain::Branch> pending_delete_branch;
    std::optional<domain::Branch> pending_merge_branch;

    // Loading state
    bool is_loading = false;
    std::optional<Error> error;

    // UI state
    bool show_help = false;
    std::optional<std::string> notification; // Temporary notification message

    // Repository path
    std::string repo_path;
};

// Initialize the branch model
auto init_branch(std::string repo_path) -> std::pair<BranchModel, tea::CmdBatch>;

// Update function - processes messages and returns new model + commands
auto update_branch(BranchModel model, tea::Msg msg) -> std::pair<BranchModel, tea::CmdBatch>;

// Helper: Apply current filter to branches
auto apply_branch_filter(BranchModel& model) -> void;

// Helper: Get currently selected branch
auto selected_branch(const BranchModel& model) -> std::optional<domain::Branch>;

// Commands for async operations

// Load all branches
auto cmd_load_branches(std::string repo_path) -> tea::CmdBatch;

// Switch to a branch
auto cmd_switch_to_branch(std::string repo_path, std::string branch_name) -> tea::CmdBatch;

// Create a new branch
auto cmd_create_branch(std::string repo_path, std::string branch_name, bool switch_to = false)
    -> tea::CmdBatch;

// Delete a branch
auto cmd_delete_branch(std::string repo_path, std::string branch_name, bool force = false)
    -> tea::CmdBatch;

// Rename a branch
auto cmd_rename_branch(std::string repo_path, std::string old_name, std::string new_name)
    -> tea::CmdBatch;

// Merge a branch into current
auto cmd_merge_branch(std::string repo_path, std::string branch_name) -> tea::CmdBatch;

} // namespace repo::tui::models
