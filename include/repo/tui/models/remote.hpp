#pragma once

#include <repo/domain/remote.hpp>
#include <repo/error.hpp>
#include <repo/tui/tea/cmd.hpp>
#include <repo/tui/tea/msg.hpp>

#include <optional>
#include <string>
#include <vector>

namespace repo::tui::models {

// Remote view mode
enum class RemoteMode {
    Normal,        // Normal browsing
    AddInput,      // Adding new remote (entering name and URL)
    RemoveConfirm, // Confirming remote removal
    FetchProgress, // Showing fetch progress
    PushProgress,  // Showing push progress
    PullProgress,  // Showing pull progress
    Help           // Showing help
};

// Remote add input step
enum class AddInputStep {
    Name, // Entering remote name
    URL   // Entering remote URL
};

// Remote view model
struct RemoteModel {
    // Remote data
    std::vector<domain::Remote> remotes;

    // Selection and navigation
    size_t selected_index = 0;

    // View state
    RemoteMode mode = RemoteMode::Normal;

    // Add remote input state
    AddInputStep add_input_step = AddInputStep::Name;
    std::string input_name; // Name being entered
    std::string input_url;  // URL being entered
    size_t input_cursor_pos = 0;

    // Remove confirmation state
    std::optional<domain::Remote> pending_remove_remote;

    // Operation progress state
    std::optional<std::string> operation_remote_name; // Remote being operated on
    size_t progress_received = 0;                     // Objects/bytes received
    size_t progress_total = 0;                        // Total objects/bytes
    std::string progress_phase; // Current phase (e.g., "Receiving objects", "Resolving deltas")

    // Loading state
    bool is_loading = false;
    std::optional<Error> error;

    // UI state
    std::optional<std::string> notification; // Temporary notification message

    // Repository path
    std::string repo_path;
};

// Initialize the remote model
auto init_remote(std::string repo_path) -> std::pair<RemoteModel, tea::CmdBatch>;

// Update function - processes messages and returns new model + commands
auto update_remote(RemoteModel model, tea::Msg msg) -> std::pair<RemoteModel, tea::CmdBatch>;

// Helper: Get currently selected remote
auto selected_remote(const RemoteModel& model) -> std::optional<domain::Remote>;

// Helper: Get current input text based on add step
auto current_input_text(const RemoteModel& model) -> std::string;

// Commands for async operations

// Load all remotes
auto cmd_load_remotes(std::string repo_path) -> tea::CmdBatch;

// Add a new remote
auto cmd_add_remote(std::string repo_path, std::string name, std::string url) -> tea::CmdBatch;

// Remove a remote
auto cmd_remove_remote(std::string repo_path, std::string name) -> tea::CmdBatch;

// Fetch from a remote
auto cmd_fetch(std::string repo_path, std::string remote_name, bool prune = false, bool tags = true)
    -> tea::CmdBatch;

// Push to a remote
auto cmd_push(std::string repo_path, std::string remote_name, bool force = false,
              bool set_upstream = false) -> tea::CmdBatch;

// Pull from a remote
auto cmd_pull(std::string repo_path, std::string remote_name, bool rebase = false,
              bool prune = false) -> tea::CmdBatch;

} // namespace repo::tui::models
