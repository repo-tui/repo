#pragma once

#include <repo/tui/models/branch.hpp>
#include <repo/tui/models/command.hpp>
#include <repo/tui/models/diff.hpp>
#include <repo/tui/models/log.hpp>
#include <repo/tui/models/remote.hpp>
#include <repo/tui/models/stash.hpp>
#include <repo/tui/models/status.hpp>
#include <repo/tui/tea/cmd.hpp>
#include <repo/tui/tea/msg.hpp>

#include <string>

namespace repo::tui::models {

// Active view in the application
enum class ActiveView { Status, Log, Diff, Branch, Remote, Stash };

// Main application model containing all views
struct AppModel {
    // Current active view
    ActiveView active_view = ActiveView::Status;

    // View models
    StatusModel status;
    LogModel log;
    DiffModel diff;
    BranchModel branch;
    RemoteModel remote;
    StashModel stash;

    // Command mode (overlays on all views)
    CommandModel command;

    // Global state
    std::string repo_path;
    bool should_quit = false;

    // Global notification (shown at top of screen)
    std::optional<std::string> global_notification;
};

// Initialize the application
auto init_app(std::string repo_path) -> std::pair<AppModel, tea::CmdBatch>;

// Main update function - routes messages to appropriate view
auto update_app(AppModel model, tea::Msg msg) -> std::pair<AppModel, tea::CmdBatch>;

// Helper: Switch to a different view
auto switch_view(AppModel model, ActiveView view) -> std::pair<AppModel, tea::CmdBatch>;

// Helper: Activate command mode
auto activate_command(AppModel model) -> AppModel;

// Helper: Handle quit
auto quit_app(AppModel model) -> AppModel;

} // namespace repo::tui::models
