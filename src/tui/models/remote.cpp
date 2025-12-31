#include <repo/error.hpp>
#include <repo/ops/remote.hpp>
#include <repo/repository.hpp>
#include <repo/tui/models/remote.hpp>

#include <algorithm>

namespace repo::tui::models {

// ============================================================================
// Helper Functions
// ============================================================================

auto selected_remote(const RemoteModel& model) -> std::optional<domain::Remote> {
    if (model.remotes.empty() || model.selected_index >= model.remotes.size()) {
        return std::nullopt;
    }
    return model.remotes[model.selected_index];
}

auto current_input_text(const RemoteModel& model) -> std::string {
    switch (model.add_input_step) {
        case AddInputStep::Name:
            return model.input_name;
        case AddInputStep::URL:
            return model.input_url;
    }
    return "";
}

// ============================================================================
// Keyboard Input Handlers
// ============================================================================

// Handle normal mode keyboard input
static auto handle_normal_input(RemoteModel model, const tea::KeyMsg& key)
    -> std::pair<RemoteModel, tea::CmdBatch> {

    // Navigation
    if (key.type == tea::KeyMsg::Type::ArrowDown ||
        (key.type == tea::KeyMsg::Type::Character && key.character == 'j')) {
        if (!model.remotes.empty()) {
            model.selected_index = std::min(model.selected_index + 1, model.remotes.size() - 1);
        }
        return {std::move(model), tea::none()};
    }

    if (key.type == tea::KeyMsg::Type::ArrowUp ||
        (key.type == tea::KeyMsg::Type::Character && key.character == 'k')) {
        if (model.selected_index > 0) {
            model.selected_index--;
        }
        return {std::move(model), tea::none()};
    }

    if (key.type == tea::KeyMsg::Type::Home ||
        (key.type == tea::KeyMsg::Type::Character && key.character == 'g')) {
        model.selected_index = 0;
        return {std::move(model), tea::none()};
    }

    if (key.type == tea::KeyMsg::Type::End ||
        (key.type == tea::KeyMsg::Type::Character && key.character == 'G')) {
        if (!model.remotes.empty()) {
            model.selected_index = model.remotes.size() - 1;
        }
        return {std::move(model), tea::none()};
    }

    // Add remote
    if (key.type == tea::KeyMsg::Type::Character && key.character == 'a') {
        model.mode = RemoteMode::AddInput;
        model.add_input_step = AddInputStep::Name;
        model.input_name.clear();
        model.input_url.clear();
        model.input_cursor_pos = 0;
        return {std::move(model), tea::none()};
    }

    // Remove remote
    if (key.type == tea::KeyMsg::Type::Character && key.character == 'r') {
        auto remote = selected_remote(model);
        if (remote) {
            model.mode = RemoteMode::RemoveConfirm;
            model.pending_remove_remote = remote;
        }
        return {std::move(model), tea::none()};
    }

    // Fetch
    if (key.type == tea::KeyMsg::Type::Character && key.character == 'f') {
        auto remote = selected_remote(model);
        if (remote) {
            model.mode = RemoteMode::FetchProgress;
            model.operation_remote_name = remote->name;
            model.progress_received = 0;
            model.progress_total = 0;
            model.progress_phase = "Starting fetch...";
            return {std::move(model), cmd_fetch(model.repo_path, remote->name)};
        }
        return {std::move(model), tea::none()};
    }

    // Push
    if (key.type == tea::KeyMsg::Type::Character && key.character == 'p') {
        auto remote = selected_remote(model);
        if (remote) {
            model.mode = RemoteMode::PushProgress;
            model.operation_remote_name = remote->name;
            model.progress_received = 0;
            model.progress_total = 0;
            model.progress_phase = "Starting push...";
            return {std::move(model), cmd_push(model.repo_path, remote->name)};
        }
        return {std::move(model), tea::none()};
    }

    // Force push
    if (key.type == tea::KeyMsg::Type::Character && key.character == 'P') {
        auto remote = selected_remote(model);
        if (remote) {
            model.mode = RemoteMode::PushProgress;
            model.operation_remote_name = remote->name;
            model.progress_received = 0;
            model.progress_total = 0;
            model.progress_phase = "Starting force push...";
            return {std::move(model), cmd_push(model.repo_path, remote->name, true)};
        }
        return {std::move(model), tea::none()};
    }

    // Pull
    if (key.type == tea::KeyMsg::Type::Character && key.character == 'l') {
        auto remote = selected_remote(model);
        if (remote) {
            model.mode = RemoteMode::PullProgress;
            model.operation_remote_name = remote->name;
            model.progress_received = 0;
            model.progress_total = 0;
            model.progress_phase = "Starting pull...";
            return {std::move(model), cmd_pull(model.repo_path, remote->name)};
        }
        return {std::move(model), tea::none()};
    }

    // Refresh
    if (key.type == tea::KeyMsg::Type::Character && key.character == 'R') {
        model.is_loading = true;
        return {std::move(model), cmd_load_remotes(model.repo_path)};
    }

    // Help
    if (key.type == tea::KeyMsg::Type::Character && key.character == '?') {
        model.mode = (model.mode == RemoteMode::Help) ? RemoteMode::Normal : RemoteMode::Help;
        return {std::move(model), tea::none()};
    }

    return {std::move(model), tea::none()};
}

// Handle add remote input
static auto handle_add_input(RemoteModel model, const tea::KeyMsg& key)
    -> std::pair<RemoteModel, tea::CmdBatch> {

    // Cancel
    if (key.type == tea::KeyMsg::Type::Escape) {
        model.mode = RemoteMode::Normal;
        model.input_name.clear();
        model.input_url.clear();
        model.input_cursor_pos = 0;
        return {std::move(model), tea::none()};
    }

    // Tab to switch between name and URL
    if (key.type == tea::KeyMsg::Type::Tab) {
        if (model.add_input_step == AddInputStep::Name) {
            model.add_input_step = AddInputStep::URL;
            model.input_cursor_pos = model.input_url.size();
        } else {
            model.add_input_step = AddInputStep::Name;
            model.input_cursor_pos = model.input_name.size();
        }
        return {std::move(model), tea::none()};
    }

    // Get current text reference
    std::string* current_text =
        (model.add_input_step == AddInputStep::Name) ? &model.input_name : &model.input_url;

    // Submit
    if (key.type == tea::KeyMsg::Type::Enter) {
        // If on Name step and name is not empty, move to URL
        if (model.add_input_step == AddInputStep::Name && !model.input_name.empty()) {
            model.add_input_step = AddInputStep::URL;
            model.input_cursor_pos = model.input_url.size();
            return {std::move(model), tea::none()};
        }

        // If on URL step and both are filled, submit
        if (model.add_input_step == AddInputStep::URL && !model.input_name.empty() &&
            !model.input_url.empty()) {
            std::string name = model.input_name;
            std::string url = model.input_url;

            model.mode = RemoteMode::Normal;
            model.input_name.clear();
            model.input_url.clear();
            model.input_cursor_pos = 0;
            model.is_loading = true;

            return {std::move(model), cmd_add_remote(model.repo_path, name, url)};
        }

        return {std::move(model), tea::none()};
    }

    // Backspace
    if (key.type == tea::KeyMsg::Type::Backspace) {
        if (model.input_cursor_pos > 0 && !current_text->empty()) {
            current_text->erase(model.input_cursor_pos - 1, 1);
            model.input_cursor_pos--;
        }
        return {std::move(model), tea::none()};
    }

    // Delete
    if (key.type == tea::KeyMsg::Type::Delete) {
        if (model.input_cursor_pos < current_text->size()) {
            current_text->erase(model.input_cursor_pos, 1);
        }
        return {std::move(model), tea::none()};
    }

    // Arrow keys
    if (key.type == tea::KeyMsg::Type::ArrowLeft) {
        if (model.input_cursor_pos > 0) {
            model.input_cursor_pos--;
        }
        return {std::move(model), tea::none()};
    }

    if (key.type == tea::KeyMsg::Type::ArrowRight) {
        if (model.input_cursor_pos < current_text->size()) {
            model.input_cursor_pos++;
        }
        return {std::move(model), tea::none()};
    }

    if (key.type == tea::KeyMsg::Type::Home) {
        model.input_cursor_pos = 0;
        return {std::move(model), tea::none()};
    }

    if (key.type == tea::KeyMsg::Type::End) {
        model.input_cursor_pos = current_text->size();
        return {std::move(model), tea::none()};
    }

    // Regular character input
    if (key.type == tea::KeyMsg::Type::Character) {
        current_text->insert(model.input_cursor_pos, 1, key.character);
        model.input_cursor_pos++;
        return {std::move(model), tea::none()};
    }

    return {std::move(model), tea::none()};
}

// Handle remove confirmation
static auto handle_remove_confirm(RemoteModel model, const tea::KeyMsg& key)
    -> std::pair<RemoteModel, tea::CmdBatch> {

    // Confirm deletion
    if (key.type == tea::KeyMsg::Type::Character &&
        (key.character == 'y' || key.character == 'Y')) {
        if (model.pending_remove_remote) {
            std::string name = model.pending_remove_remote->name;

            model.mode = RemoteMode::Normal;
            model.pending_remove_remote = std::nullopt;
            model.is_loading = true;

            return {std::move(model), cmd_remove_remote(model.repo_path, name)};
        }
    }

    // Cancel
    if ((key.type == tea::KeyMsg::Type::Character &&
         (key.character == 'n' || key.character == 'N')) ||
        key.type == tea::KeyMsg::Type::Escape) {
        model.mode = RemoteMode::Normal;
        model.pending_remove_remote = std::nullopt;
        return {std::move(model), tea::none()};
    }

    return {std::move(model), tea::none()};
}

// Handle progress mode (fetch/push/pull)
static auto handle_progress_mode(RemoteModel model, const tea::KeyMsg& /* key */)
    -> std::pair<RemoteModel, tea::CmdBatch> {

    // Any key dismisses progress view (after operation completes)
    // Operation completion is handled by messages, not keyboard
    return {std::move(model), tea::none()};
}

// Handle help screen
static auto handle_help(RemoteModel model, const tea::KeyMsg& /* key */)
    -> std::pair<RemoteModel, tea::CmdBatch> {

    // Any key closes help
    model.mode = RemoteMode::Normal;
    return {std::move(model), tea::none()};
}

// ============================================================================
// Main Update Function
// ============================================================================

auto update_remote(RemoteModel model, tea::Msg msg) -> std::pair<RemoteModel, tea::CmdBatch> {
    return std::visit(
        [&model](auto&& m) -> std::pair<RemoteModel, tea::CmdBatch> {
            using T = std::decay_t<decltype(m)>;

            if constexpr (std::is_same_v<T, tea::KeyMsg>) {
                // Route to appropriate handler based on mode
                switch (model.mode) {
                    case RemoteMode::Normal:
                        return handle_normal_input(std::move(model), m);
                    case RemoteMode::AddInput:
                        return handle_add_input(std::move(model), m);
                    case RemoteMode::RemoveConfirm:
                        return handle_remove_confirm(std::move(model), m);
                    case RemoteMode::FetchProgress:
                    case RemoteMode::PushProgress:
                    case RemoteMode::PullProgress:
                        return handle_progress_mode(std::move(model), m);
                    case RemoteMode::Help:
                        return handle_help(std::move(model), m);
                }
                return {std::move(model), tea::none()};
            }

            else if constexpr (std::is_same_v<T, tea::RemotesLoadedMsg>) {
                model.remotes = std::move(m.remotes);
                model.is_loading = false;
                model.error = std::nullopt;

                // Reset selection if out of bounds
                if (model.selected_index >= model.remotes.size() && !model.remotes.empty()) {
                    model.selected_index = model.remotes.size() - 1;
                }

                return {std::move(model), tea::none()};
            }

            else if constexpr (std::is_same_v<T, tea::RemoteAddedMsg>) {
                model.is_loading = false;
                model.mode = RemoteMode::Normal;
                model.notification = "Remote '" + m.name + "' added successfully";

                // Reload remotes to refresh list
                return {std::move(model), cmd_load_remotes(model.repo_path)};
            }

            else if constexpr (std::is_same_v<T, tea::RemoteRemovedMsg>) {
                model.is_loading = false;
                model.mode = RemoteMode::Normal;
                model.notification = "Remote '" + m.name + "' removed";

                // Reload remotes to refresh list
                return {std::move(model), cmd_load_remotes(model.repo_path)};
            }

            else if constexpr (std::is_same_v<T, tea::FetchCompletedMsg>) {
                model.mode = RemoteMode::Normal;
                model.notification = "Fetch completed from '" + m.remote + "'";
                if (!m.updated_refs.empty()) {
                    model.notification = *model.notification + " (" +
                                         std::to_string(m.updated_refs.size()) + " refs updated)";
                }
                model.operation_remote_name = std::nullopt;
                return {std::move(model), tea::none()};
            }

            else if constexpr (std::is_same_v<T, tea::PushCompletedMsg>) {
                model.mode = RemoteMode::Normal;
                model.notification = "Push completed to '" + m.remote + "'";
                if (!m.updated_refs.empty()) {
                    model.notification = *model.notification + " (" +
                                         std::to_string(m.updated_refs.size()) + " refs updated)";
                }
                model.operation_remote_name = std::nullopt;
                return {std::move(model), tea::none()};
            }

            else if constexpr (std::is_same_v<T, tea::PullCompletedMsg>) {
                model.mode = RemoteMode::Normal;
                model.notification = "Pull completed (" + m.merge_type + ")";
                model.operation_remote_name = std::nullopt;
                return {std::move(model), tea::none()};
            }

            else if constexpr (std::is_same_v<T, tea::RemoteErrorMsg>) {
                model.is_loading = false;
                model.error = std::move(m.error);

                // Return to normal mode on error
                if (model.mode != RemoteMode::Normal && model.mode != RemoteMode::Help) {
                    model.mode = RemoteMode::Normal;
                }

                return {std::move(model), tea::none()};
            }

            // Default: no change
            return {std::move(model), tea::none()};
        },
        msg);
}

// ============================================================================
// Initialization
// ============================================================================

auto init_remote(std::string repo_path) -> std::pair<RemoteModel, tea::CmdBatch> {
    RemoteModel model;
    model.repo_path = repo_path;
    model.is_loading = true;

    return {std::move(model), cmd_load_remotes(repo_path)};
}

// ============================================================================
// Async Commands
// ============================================================================

auto cmd_load_remotes(std::string repo_path) -> tea::CmdBatch {
    return tea::async([repo_path = std::move(repo_path)]() -> std::optional<tea::Msg> {
        try {
            auto repo = Repository::open(repo_path);
            if (!repo) {
                return tea::RemoteErrorMsg{std::move(repo.error())};
            }

            auto result = ops::list_remotes(*repo);
            if (!result) {
                return tea::RemoteErrorMsg{std::move(result.error())};
            }

            return tea::RemotesLoadedMsg{std::move(result->remotes)};

        } catch (const std::exception& e) {
            return tea::RemoteErrorMsg{
                make_error(Error::Code::Unknown, "Failed to load remotes", e.what())};
        }
    });
}

auto cmd_add_remote(std::string repo_path, std::string name, std::string url) -> tea::CmdBatch {
    return tea::async([repo_path = std::move(repo_path), name = std::move(name),
                       url = std::move(url)]() -> std::optional<tea::Msg> {
        try {
            auto repo = Repository::open(repo_path);
            if (!repo) {
                return tea::RemoteErrorMsg{std::move(repo.error())};
            }

            auto result = ops::add_remote(*repo, ops::AddRemoteParams{.name = name, .url = url});

            if (!result) {
                return tea::RemoteErrorMsg{std::move(result.error())};
            }

            return tea::RemoteAddedMsg{name};

        } catch (const std::exception& e) {
            return tea::RemoteErrorMsg{
                make_error(Error::Code::Unknown, "Failed to add remote", e.what())};
        }
    });
}

auto cmd_remove_remote(std::string repo_path, std::string name) -> tea::CmdBatch {
    return tea::async(
        [repo_path = std::move(repo_path), name = std::move(name)]() -> std::optional<tea::Msg> {
            try {
                auto repo = Repository::open(repo_path);
                if (!repo) {
                    return tea::RemoteErrorMsg{std::move(repo.error())};
                }

                auto result = ops::remove_remote(*repo, ops::RemoveRemoteParams{.name = name});

                if (!result) {
                    return tea::RemoteErrorMsg{std::move(result.error())};
                }

                return tea::RemoteRemovedMsg{name};

            } catch (const std::exception& e) {
                return tea::RemoteErrorMsg{
                    make_error(Error::Code::Unknown, "Failed to remove remote", e.what())};
            }
        });
}

auto cmd_fetch(std::string repo_path, std::string remote_name, bool prune, bool tags)
    -> tea::CmdBatch {
    return tea::async([repo_path = std::move(repo_path), remote_name = std::move(remote_name),
                       prune, tags]() -> std::optional<tea::Msg> {
        try {
            auto repo = Repository::open(repo_path);
            if (!repo) {
                return tea::RemoteErrorMsg{std::move(repo.error())};
            }

            auto result =
                ops::fetch(*repo, ops::FetchParams{.remote = remote_name,
                                                   .refspec = "", // Empty = fetch all refs
                                                   .prune = prune,
                                                   .tags = tags});

            if (!result) {
                return tea::RemoteErrorMsg{std::move(result.error())};
            }

            return tea::FetchCompletedMsg{.remote = remote_name,
                                          .updated_refs = result->updated_refs};

        } catch (const std::exception& e) {
            return tea::RemoteErrorMsg{
                make_error(Error::Code::Unknown, "Failed to fetch from remote", e.what())};
        }
    });
}

auto cmd_push(std::string repo_path, std::string remote_name, bool force, bool set_upstream)
    -> tea::CmdBatch {
    return tea::async([repo_path = std::move(repo_path), remote_name = std::move(remote_name),
                       force, set_upstream]() -> std::optional<tea::Msg> {
        try {
            auto repo = Repository::open(repo_path);
            if (!repo) {
                return tea::RemoteErrorMsg{std::move(repo.error())};
            }

            auto result =
                ops::push(*repo, ops::PushParams{.remote = remote_name,
                                                 .refspec = "", // Empty = push current branch
                                                 .force = force,
                                                 .set_upstream = set_upstream});

            if (!result) {
                return tea::RemoteErrorMsg{std::move(result.error())};
            }

            return tea::PushCompletedMsg{.remote = remote_name,
                                         .updated_refs = result->updated_refs};

        } catch (const std::exception& e) {
            return tea::RemoteErrorMsg{
                make_error(Error::Code::Unknown, "Failed to push to remote", e.what())};
        }
    });
}

auto cmd_pull(std::string repo_path, std::string remote_name, bool rebase, bool prune)
    -> tea::CmdBatch {
    return tea::async([repo_path = std::move(repo_path), remote_name = std::move(remote_name),
                       rebase, prune]() -> std::optional<tea::Msg> {
        try {
            auto repo = Repository::open(repo_path);
            if (!repo) {
                return tea::RemoteErrorMsg{std::move(repo.error())};
            }

            auto result = ops::pull(
                *repo, ops::PullParams{
                           .remote = remote_name, .rebase = rebase, .prune = prune, .tags = true});

            if (!result) {
                return tea::RemoteErrorMsg{std::move(result.error())};
            }

            return tea::PullCompletedMsg{.remote_name = remote_name,
                                         .received_objects = result->fetch_result.received_objects,
                                         .received_bytes = result->fetch_result.received_bytes,
                                         .merge_type = result->merge_type};

        } catch (const std::exception& e) {
            return tea::RemoteErrorMsg{
                make_error(Error::Code::Unknown, "Failed to pull from remote", e.what())};
        }
    });
}

} // namespace repo::tui::models
