#include <repo/tui/models/app.hpp>

namespace repo::tui::models {

// ============================================================================
// Initialization
// ============================================================================

auto init_app(std::string repo_path) -> std::pair<AppModel, tea::CmdBatch> {
    AppModel model;
    model.repo_path = repo_path;

    // Initialize all view models
    auto [status_model, status_cmds] = init_status(repo_path);
    auto [log_model, log_cmds] = init_log(repo_path);
    auto [diff_model, diff_cmds] = init_diff(repo_path);
    auto [branch_model, branch_cmds] = init_branch(repo_path);
    auto [remote_model, remote_cmds] = init_remote(repo_path);
    auto [stash_model, stash_cmds] = init_stash(repo_path);
    auto [command_model, command_cmds] = init_command(repo_path);

    model.status = std::move(status_model);
    model.log = std::move(log_model);
    model.diff = std::move(diff_model);
    model.branch = std::move(branch_model);
    model.remote = std::move(remote_model);
    model.stash = std::move(stash_model);
    model.command = std::move(command_model);

    // Start with status view - only execute status commands initially
    return {std::move(model), std::move(status_cmds)};
}

// ============================================================================
// View Switching
// ============================================================================

auto switch_view(AppModel model, ActiveView view) -> std::pair<AppModel, tea::CmdBatch> {
    model.active_view = view;

    // Refresh the view we're switching to
    switch (view) {
        case ActiveView::Status:
            return {std::move(model), cmd_load_status(model.repo_path)};
        case ActiveView::Log:
            return {std::move(model), cmd_load_log(model.repo_path, 50)};
        case ActiveView::Diff:
            return {std::move(model), cmd_load_diff(model.repo_path, DiffType::Unstaged)};
        case ActiveView::Branch:
            return {std::move(model), cmd_load_branches(model.repo_path)};
        case ActiveView::Remote:
            return {std::move(model), cmd_load_remotes(model.repo_path)};
        case ActiveView::Stash:
            return {std::move(model), cmd_load_stashes(model.repo_path)};
    }

    return {std::move(model), tea::none()};
}

// ============================================================================
// Global Helpers
// ============================================================================

auto activate_command(AppModel model) -> AppModel {
    model.command = activate_command_mode(std::move(model.command));
    return model;
}

auto quit_app(AppModel model) -> AppModel {
    model.should_quit = true;
    return model;
}

// ============================================================================
// Main Update Function
// ============================================================================

auto update_app(AppModel model, tea::Msg msg) -> std::pair<AppModel, tea::CmdBatch> {
    return std::visit(
        [&model](auto&& m) -> std::pair<AppModel, tea::CmdBatch> {
            using T = std::decay_t<decltype(m)>;

            // Handle keyboard input
            if constexpr (std::is_same_v<T, tea::KeyMsg>) {
                // Command mode takes priority
                if (model.command.mode != CommandMode::Inactive) {
                    auto [new_command, cmds] =
                        update_command(std::move(model.command), tea::Msg{m});
                    model.command = std::move(new_command);
                    return {std::move(model), std::move(cmds)};
                }

                // Global shortcuts (only when not in command mode)

                // Activate command mode with ':'
                if (m.type == tea::KeyMsg::Type::Character && m.character == ':') {
                    model = activate_command(std::move(model));
                    return {std::move(model), tea::none()};
                }

                // Quit with 'q' or 'Q'
                if (m.type == tea::KeyMsg::Type::Character &&
                    (m.character == 'q' || m.character == 'Q')) {
                    model = quit_app(std::move(model));
                    return {std::move(model), tea::none()};
                }

                // View switching with number keys
                if (m.type == tea::KeyMsg::Type::Character && m.character == '1') {
                    return switch_view(std::move(model), ActiveView::Status);
                }
                if (m.type == tea::KeyMsg::Type::Character && m.character == '2') {
                    return switch_view(std::move(model), ActiveView::Log);
                }
                if (m.type == tea::KeyMsg::Type::Character && m.character == '3') {
                    return switch_view(std::move(model), ActiveView::Diff);
                }
                if (m.type == tea::KeyMsg::Type::Character && m.character == '4') {
                    return switch_view(std::move(model), ActiveView::Branch);
                }
                if (m.type == tea::KeyMsg::Type::Character && m.character == '5') {
                    return switch_view(std::move(model), ActiveView::Remote);
                }
                if (m.type == tea::KeyMsg::Type::Character && m.character == '6') {
                    return switch_view(std::move(model), ActiveView::Stash);
                }

                // Route to active view
                switch (model.active_view) {
                    case ActiveView::Status: {
                        auto [new_status, cmds] =
                            update_status(std::move(model.status), tea::Msg{m});
                        model.status = std::move(new_status);
                        return {std::move(model), std::move(cmds)};
                    }
                    case ActiveView::Log: {
                        auto [new_log, cmds] = update_log(std::move(model.log), tea::Msg{m});
                        model.log = std::move(new_log);
                        return {std::move(model), std::move(cmds)};
                    }
                    case ActiveView::Diff: {
                        auto [new_diff, cmds] = update_diff(std::move(model.diff), tea::Msg{m});
                        model.diff = std::move(new_diff);
                        return {std::move(model), std::move(cmds)};
                    }
                    case ActiveView::Branch: {
                        auto [new_branch, cmds] =
                            update_branch(std::move(model.branch), tea::Msg{m});
                        model.branch = std::move(new_branch);
                        return {std::move(model), std::move(cmds)};
                    }
                    case ActiveView::Remote: {
                        auto [new_remote, cmds] =
                            update_remote(std::move(model.remote), tea::Msg{m});
                        model.remote = std::move(new_remote);
                        return {std::move(model), std::move(cmds)};
                    }
                    case ActiveView::Stash: {
                        auto [new_stash, cmds] = update_stash(std::move(model.stash), tea::Msg{m});
                        model.stash = std::move(new_stash);
                        return {std::move(model), std::move(cmds)};
                    }
                }
            }

            // Handle CommandExecutedMsg
            else if constexpr (std::is_same_v<T, tea::CommandExecutedMsg>) {
                // Update command model
                auto [new_command, cmds] = update_command(std::move(model.command), tea::Msg{m});
                model.command = std::move(new_command);

                // If command succeeded, refresh all views (commands may have modified repo state)
                if (m.exit_code == 0) {
                    model.global_notification = "Command executed: " + m.command;

                    // Refresh current view
                    return switch_view(std::move(model), model.active_view);
                }

                return {std::move(model), std::move(cmds)};
            }

            // Route view-specific messages to appropriate view
            else if constexpr (std::is_same_v<T, tea::StatusLoadedMsg> ||
                               std::is_same_v<T, tea::StatusErrorMsg>) {
                auto [new_status, cmds] = update_status(std::move(model.status), tea::Msg{m});
                model.status = std::move(new_status);
                return {std::move(model), std::move(cmds)};
            }

            else if constexpr (std::is_same_v<T, tea::LogLoadedMsg> ||
                               std::is_same_v<T, tea::LogErrorMsg>) {
                auto [new_log, cmds] = update_log(std::move(model.log), tea::Msg{m});
                model.log = std::move(new_log);
                return {std::move(model), std::move(cmds)};
            }

            else if constexpr (std::is_same_v<T, tea::DiffLoadedMsg> ||
                               std::is_same_v<T, tea::DiffErrorMsg>) {
                auto [new_diff, cmds] = update_diff(std::move(model.diff), tea::Msg{m});
                model.diff = std::move(new_diff);
                return {std::move(model), std::move(cmds)};
            }

            else if constexpr (std::is_same_v<T, tea::BranchesLoadedMsg> ||
                               std::is_same_v<T, tea::BranchCreatedMsg> ||
                               std::is_same_v<T, tea::BranchDeletedMsg> ||
                               std::is_same_v<T, tea::BranchSwitchedMsg> ||
                               std::is_same_v<T, tea::BranchErrorMsg>) {
                auto [new_branch, cmds] = update_branch(std::move(model.branch), tea::Msg{m});
                model.branch = std::move(new_branch);
                return {std::move(model), std::move(cmds)};
            }

            else if constexpr (std::is_same_v<T, tea::RemotesLoadedMsg> ||
                               std::is_same_v<T, tea::RemoteAddedMsg> ||
                               std::is_same_v<T, tea::RemoteRemovedMsg> ||
                               std::is_same_v<T, tea::FetchProgressMsg> ||
                               std::is_same_v<T, tea::FetchCompletedMsg> ||
                               std::is_same_v<T, tea::PushProgressMsg> ||
                               std::is_same_v<T, tea::PushCompletedMsg> ||
                               std::is_same_v<T, tea::PullCompletedMsg> ||
                               std::is_same_v<T, tea::RemoteErrorMsg>) {
                auto [new_remote, cmds] = update_remote(std::move(model.remote), tea::Msg{m});
                model.remote = std::move(new_remote);
                return {std::move(model), std::move(cmds)};
            }

            else if constexpr (std::is_same_v<T, tea::StashesLoadedMsg> ||
                               std::is_same_v<T, tea::StashCreatedMsg> ||
                               std::is_same_v<T, tea::StashAppliedMsg> ||
                               std::is_same_v<T, tea::StashPoppedMsg> ||
                               std::is_same_v<T, tea::StashDroppedMsg> ||
                               std::is_same_v<T, tea::StashErrorMsg>) {
                auto [new_stash, cmds] = update_stash(std::move(model.stash), tea::Msg{m});
                model.stash = std::move(new_stash);
                return {std::move(model), std::move(cmds)};
            }

            // Default: no change
            return {std::move(model), tea::none()};
        },
        msg);
}

} // namespace repo::tui::models
