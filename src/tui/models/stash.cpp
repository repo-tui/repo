#include <repo/error.hpp>
#include <repo/ops/stash.hpp>
#include <repo/repository.hpp>
#include <repo/tui/models/stash.hpp>

#include <algorithm>

namespace repo::tui::models {

// ============================================================================
// Helper Functions
// ============================================================================

auto selected_stash(const StashModel& model) -> std::optional<domain::Stash> {
    if (model.stashes.empty() || model.selected_index >= model.stashes.size()) {
        return std::nullopt;
    }
    return model.stashes[model.selected_index];
}

// ============================================================================
// Keyboard Input Handlers
// ============================================================================

// Handle normal mode keyboard input
static auto handle_normal_input(StashModel model, const tea::KeyMsg& key)
    -> std::pair<StashModel, tea::CmdBatch> {

    // Navigation
    if (key.type == tea::KeyMsg::Type::ArrowDown ||
        (key.type == tea::KeyMsg::Type::Character && key.character == 'j')) {
        if (!model.stashes.empty()) {
            model.selected_index = std::min(model.selected_index + 1, model.stashes.size() - 1);
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
        if (!model.stashes.empty()) {
            model.selected_index = model.stashes.size() - 1;
        }
        return {std::move(model), tea::none()};
    }

    // Create stash
    if (key.type == tea::KeyMsg::Type::Character && key.character == 'c') {
        model.mode = StashMode::CreateInput;
        model.input_message.clear();
        model.input_cursor_pos = 0;
        model.include_untracked = false;
        model.keep_index = false;
        return {std::move(model), tea::none()};
    }

    // Apply stash
    if (key.type == tea::KeyMsg::Type::Character && key.character == 'a') {
        auto stash = selected_stash(model);
        if (stash) {
            model.mode = StashMode::ApplyConfirm;
            model.pending_operation_stash = stash;
            model.reinstate_index = false;
        }
        return {std::move(model), tea::none()};
    }

    // Pop stash
    if (key.type == tea::KeyMsg::Type::Character && key.character == 'p') {
        auto stash = selected_stash(model);
        if (stash) {
            model.mode = StashMode::PopConfirm;
            model.pending_operation_stash = stash;
            model.reinstate_index = false;
        }
        return {std::move(model), tea::none()};
    }

    // Drop stash
    if (key.type == tea::KeyMsg::Type::Character && key.character == 'd') {
        auto stash = selected_stash(model);
        if (stash) {
            model.mode = StashMode::DropConfirm;
            model.pending_operation_stash = stash;
        }
        return {std::move(model), tea::none()};
    }

    // View details
    if (key.type == tea::KeyMsg::Type::Enter ||
        (key.type == tea::KeyMsg::Type::Character && key.character == 'v')) {
        auto stash = selected_stash(model);
        if (stash) {
            model.mode = StashMode::Details;
            model.detail_stash = stash;
            model.detail_diff = std::nullopt;
            // Load stash details
            return {std::move(model), cmd_load_stash_details(model.repo_path, *stash)};
        }
        return {std::move(model), tea::none()};
    }

    // Refresh
    if (key.type == tea::KeyMsg::Type::Character && key.character == 'R') {
        model.is_loading = true;
        return {std::move(model), cmd_load_stashes(model.repo_path)};
    }

    // Help
    if (key.type == tea::KeyMsg::Type::Character && key.character == '?') {
        model.mode = (model.mode == StashMode::Help) ? StashMode::Normal : StashMode::Help;
        return {std::move(model), tea::none()};
    }

    return {std::move(model), tea::none()};
}

// Handle create stash input
static auto handle_create_input(StashModel model, const tea::KeyMsg& key)
    -> std::pair<StashModel, tea::CmdBatch> {

    // Cancel
    if (key.type == tea::KeyMsg::Type::Escape) {
        model.mode = StashMode::Normal;
        model.input_message.clear();
        model.input_cursor_pos = 0;
        return {std::move(model), tea::none()};
    }

    // Submit
    if (key.type == tea::KeyMsg::Type::Enter) {
        if (!model.input_message.empty()) {
            std::string message = model.input_message;
            bool include_untracked = model.include_untracked;
            bool keep_index = model.keep_index;

            model.mode = StashMode::Normal;
            model.input_message.clear();
            model.input_cursor_pos = 0;
            model.is_loading = true;

            // Use default git config for stasher info
            return {std::move(model),
                    cmd_create_stash(model.repo_path, message, "User", "user@example.com",
                                     include_untracked, keep_index)};
        }
        return {std::move(model), tea::none()};
    }

    // Toggle options
    if (key.type == tea::KeyMsg::Type::Character && key.character == 'u') {
        model.include_untracked = !model.include_untracked;
        return {std::move(model), tea::none()};
    }

    if (key.type == tea::KeyMsg::Type::Character && key.character == 'i') {
        model.keep_index = !model.keep_index;
        return {std::move(model), tea::none()};
    }

    // Backspace
    if (key.type == tea::KeyMsg::Type::Backspace) {
        if (model.input_cursor_pos > 0 && !model.input_message.empty()) {
            model.input_message.erase(model.input_cursor_pos - 1, 1);
            model.input_cursor_pos--;
        }
        return {std::move(model), tea::none()};
    }

    // Delete
    if (key.type == tea::KeyMsg::Type::Delete) {
        if (model.input_cursor_pos < model.input_message.size()) {
            model.input_message.erase(model.input_cursor_pos, 1);
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
        if (model.input_cursor_pos < model.input_message.size()) {
            model.input_cursor_pos++;
        }
        return {std::move(model), tea::none()};
    }

    if (key.type == tea::KeyMsg::Type::Home) {
        model.input_cursor_pos = 0;
        return {std::move(model), tea::none()};
    }

    if (key.type == tea::KeyMsg::Type::End) {
        model.input_cursor_pos = model.input_message.size();
        return {std::move(model), tea::none()};
    }

    // Regular character input
    if (key.type == tea::KeyMsg::Type::Character) {
        model.input_message.insert(model.input_cursor_pos, 1, key.character);
        model.input_cursor_pos++;
        return {std::move(model), tea::none()};
    }

    return {std::move(model), tea::none()};
}

// Handle apply confirmation
static auto handle_apply_confirm(StashModel model, const tea::KeyMsg& key)
    -> std::pair<StashModel, tea::CmdBatch> {

    // Confirm
    if (key.type == tea::KeyMsg::Type::Character &&
        (key.character == 'y' || key.character == 'Y')) {
        if (model.pending_operation_stash) {
            size_t index = model.pending_operation_stash->index;
            bool reinstate = model.reinstate_index;

            model.mode = StashMode::Normal;
            model.pending_operation_stash = std::nullopt;
            model.is_loading = true;

            return {std::move(model), cmd_apply_stash(model.repo_path, index, reinstate)};
        }
    }

    // Toggle reinstate index option
    if (key.type == tea::KeyMsg::Type::Character && key.character == 'i') {
        model.reinstate_index = !model.reinstate_index;
        return {std::move(model), tea::none()};
    }

    // Cancel
    if ((key.type == tea::KeyMsg::Type::Character &&
         (key.character == 'n' || key.character == 'N')) ||
        key.type == tea::KeyMsg::Type::Escape) {
        model.mode = StashMode::Normal;
        model.pending_operation_stash = std::nullopt;
        model.reinstate_index = false;
        return {std::move(model), tea::none()};
    }

    return {std::move(model), tea::none()};
}

// Handle pop confirmation
static auto handle_pop_confirm(StashModel model, const tea::KeyMsg& key)
    -> std::pair<StashModel, tea::CmdBatch> {

    // Confirm
    if (key.type == tea::KeyMsg::Type::Character &&
        (key.character == 'y' || key.character == 'Y')) {
        if (model.pending_operation_stash) {
            size_t index = model.pending_operation_stash->index;
            bool reinstate = model.reinstate_index;

            model.mode = StashMode::Normal;
            model.pending_operation_stash = std::nullopt;
            model.is_loading = true;

            return {std::move(model), cmd_pop_stash(model.repo_path, index, reinstate)};
        }
    }

    // Toggle reinstate index option
    if (key.type == tea::KeyMsg::Type::Character && key.character == 'i') {
        model.reinstate_index = !model.reinstate_index;
        return {std::move(model), tea::none()};
    }

    // Cancel
    if ((key.type == tea::KeyMsg::Type::Character &&
         (key.character == 'n' || key.character == 'N')) ||
        key.type == tea::KeyMsg::Type::Escape) {
        model.mode = StashMode::Normal;
        model.pending_operation_stash = std::nullopt;
        model.reinstate_index = false;
        return {std::move(model), tea::none()};
    }

    return {std::move(model), tea::none()};
}

// Handle drop confirmation
static auto handle_drop_confirm(StashModel model, const tea::KeyMsg& key)
    -> std::pair<StashModel, tea::CmdBatch> {

    // Confirm
    if (key.type == tea::KeyMsg::Type::Character &&
        (key.character == 'y' || key.character == 'Y')) {
        if (model.pending_operation_stash) {
            size_t index = model.pending_operation_stash->index;

            model.mode = StashMode::Normal;
            model.pending_operation_stash = std::nullopt;
            model.is_loading = true;

            return {std::move(model), cmd_drop_stash(model.repo_path, index)};
        }
    }

    // Cancel
    if ((key.type == tea::KeyMsg::Type::Character &&
         (key.character == 'n' || key.character == 'N')) ||
        key.type == tea::KeyMsg::Type::Escape) {
        model.mode = StashMode::Normal;
        model.pending_operation_stash = std::nullopt;
        return {std::move(model), tea::none()};
    }

    return {std::move(model), tea::none()};
}

// Handle details view
static auto handle_details(StashModel model, const tea::KeyMsg& /* key */)
    -> std::pair<StashModel, tea::CmdBatch> {

    // Any key closes details
    model.mode = StashMode::Normal;
    model.detail_stash = std::nullopt;
    model.detail_diff = std::nullopt;
    return {std::move(model), tea::none()};
}

// Handle help screen
static auto handle_help(StashModel model, const tea::KeyMsg& /* key */)
    -> std::pair<StashModel, tea::CmdBatch> {

    // Any key closes help
    model.mode = StashMode::Normal;
    return {std::move(model), tea::none()};
}

// ============================================================================
// Main Update Function
// ============================================================================

auto update_stash(StashModel model, tea::Msg msg) -> std::pair<StashModel, tea::CmdBatch> {
    return std::visit(
        [&model](auto&& m) -> std::pair<StashModel, tea::CmdBatch> {
            using T = std::decay_t<decltype(m)>;

            if constexpr (std::is_same_v<T, tea::KeyMsg>) {
                // Route to appropriate handler based on mode
                switch (model.mode) {
                    case StashMode::Normal:
                        return handle_normal_input(std::move(model), m);
                    case StashMode::CreateInput:
                        return handle_create_input(std::move(model), m);
                    case StashMode::ApplyConfirm:
                        return handle_apply_confirm(std::move(model), m);
                    case StashMode::PopConfirm:
                        return handle_pop_confirm(std::move(model), m);
                    case StashMode::DropConfirm:
                        return handle_drop_confirm(std::move(model), m);
                    case StashMode::Details:
                        return handle_details(std::move(model), m);
                    case StashMode::Help:
                        return handle_help(std::move(model), m);
                }
                return {std::move(model), tea::none()};
            }

            else if constexpr (std::is_same_v<T, tea::StashesLoadedMsg>) {
                model.stashes = std::move(m.stashes);
                model.is_loading = false;
                model.error = std::nullopt;

                // Reset selection if out of bounds
                if (model.selected_index >= model.stashes.size() && !model.stashes.empty()) {
                    model.selected_index = model.stashes.size() - 1;
                }

                return {std::move(model), tea::none()};
            }

            else if constexpr (std::is_same_v<T, tea::StashCreatedMsg>) {
                model.is_loading = false;
                model.mode = StashMode::Normal;
                model.notification = "Stash created: " + m.stash_id.to_string().substr(0, 7);

                // Reload stashes to refresh list
                return {std::move(model), cmd_load_stashes(model.repo_path)};
            }

            else if constexpr (std::is_same_v<T, tea::StashAppliedMsg>) {
                model.is_loading = false;
                model.mode = StashMode::Normal;
                model.notification = "Stash applied: stash@{" + std::to_string(m.index) + "}";
                return {std::move(model), tea::none()};
            }

            else if constexpr (std::is_same_v<T, tea::StashPoppedMsg>) {
                model.is_loading = false;
                model.mode = StashMode::Normal;
                model.notification = "Stash popped: stash@{" + std::to_string(m.index) + "}";

                // Reload stashes to refresh list
                return {std::move(model), cmd_load_stashes(model.repo_path)};
            }

            else if constexpr (std::is_same_v<T, tea::StashDroppedMsg>) {
                model.is_loading = false;
                model.mode = StashMode::Normal;
                model.notification = "Stash dropped: stash@{" + std::to_string(m.index) + "}";

                // Reload stashes to refresh list
                return {std::move(model), cmd_load_stashes(model.repo_path)};
            }

            else if constexpr (std::is_same_v<T, tea::DiffLoadedMsg>) {
                // This is for stash details diff
                if (model.mode == StashMode::Details) {
                    // Build diff text from file diffs
                    std::string diff_text;
                    for (const auto& file_diff : m.diffs) {
                        auto old_path = file_diff.old_path ? file_diff.old_path->string()
                                                           : file_diff.path.string();
                        diff_text +=
                            "diff --git a/" + old_path + " b/" + file_diff.path.string() + "\n";

                        for (const auto& hunk : file_diff.hunks) {
                            diff_text += hunk.header + "\n";
                            for (const auto& line : hunk.lines) {
                                diff_text += line.content + "\n";
                            }
                        }
                    }
                    model.detail_diff = diff_text;
                }
                return {std::move(model), tea::none()};
            }

            else if constexpr (std::is_same_v<T, tea::StashErrorMsg>) {
                model.is_loading = false;
                model.error = std::move(m.error);

                // Return to normal mode on error
                if (model.mode != StashMode::Normal && model.mode != StashMode::Help) {
                    model.mode = StashMode::Normal;
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

auto init_stash(std::string repo_path) -> std::pair<StashModel, tea::CmdBatch> {
    StashModel model;
    model.repo_path = repo_path;
    model.is_loading = true;

    return {std::move(model), cmd_load_stashes(repo_path)};
}

// ============================================================================
// Async Commands
// ============================================================================

auto cmd_load_stashes(std::string repo_path) -> tea::CmdBatch {
    return tea::async([repo_path = std::move(repo_path)]() -> std::optional<tea::Msg> {
        try {
            auto repo = Repository::open(repo_path);
            if (!repo) {
                return tea::StashErrorMsg{std::move(repo.error())};
            }

            auto result = ops::list_stashes(*repo);
            if (!result) {
                return tea::StashErrorMsg{std::move(result.error())};
            }

            return tea::StashesLoadedMsg{std::move(result->stashes)};

        } catch (const std::exception& e) {
            return tea::StashErrorMsg{
                make_error(Error::Code::Unknown, "Failed to load stashes", e.what())};
        }
    });
}

auto cmd_create_stash(std::string repo_path, std::string message, std::string stasher_name,
                      std::string stasher_email, bool include_untracked, bool keep_index)
    -> tea::CmdBatch {
    return tea::async([repo_path = std::move(repo_path), message = std::move(message),
                       stasher_name = std::move(stasher_name),
                       stasher_email = std::move(stasher_email), include_untracked,
                       keep_index]() -> std::optional<tea::Msg> {
        try {
            auto repo = Repository::open(repo_path);
            if (!repo) {
                return tea::StashErrorMsg{std::move(repo.error())};
            }

            auto result = ops::create_stash(
                *repo, ops::CreateStashParams{
                           .message = message,
                           .stasher = domain::Signature{.name = stasher_name,
                                                        .email = stasher_email,
                                                        .when = std::chrono::system_clock::now()},
                           .include_untracked = include_untracked,
                           .keep_index = keep_index});

            if (!result) {
                return tea::StashErrorMsg{std::move(result.error())};
            }

            return tea::StashCreatedMsg{result->stash_id};

        } catch (const std::exception& e) {
            return tea::StashErrorMsg{
                make_error(Error::Code::Unknown, "Failed to create stash", e.what())};
        }
    });
}

auto cmd_apply_stash(std::string repo_path, size_t index, bool reinstate_index) -> tea::CmdBatch {
    return tea::async([repo_path = std::move(repo_path), index,
                       reinstate_index]() -> std::optional<tea::Msg> {
        try {
            auto repo = Repository::open(repo_path);
            if (!repo) {
                return tea::StashErrorMsg{std::move(repo.error())};
            }

            auto result = ops::apply_stash(
                *repo, ops::ApplyStashParams{.index = index, .reinstate_index = reinstate_index});

            if (!result) {
                return tea::StashErrorMsg{std::move(result.error())};
            }

            return tea::StashAppliedMsg{index};

        } catch (const std::exception& e) {
            return tea::StashErrorMsg{
                make_error(Error::Code::Unknown, "Failed to apply stash", e.what())};
        }
    });
}

auto cmd_pop_stash(std::string repo_path, size_t index, bool reinstate_index) -> tea::CmdBatch {
    return tea::async(
        [repo_path = std::move(repo_path), index, reinstate_index]() -> std::optional<tea::Msg> {
            try {
                auto repo = Repository::open(repo_path);
                if (!repo) {
                    return tea::StashErrorMsg{std::move(repo.error())};
                }

                auto result = ops::pop_stash(
                    *repo, ops::PopStashParams{.index = index, .reinstate_index = reinstate_index});

                if (!result) {
                    return tea::StashErrorMsg{std::move(result.error())};
                }

                return tea::StashPoppedMsg{index};

            } catch (const std::exception& e) {
                return tea::StashErrorMsg{
                    make_error(Error::Code::Unknown, "Failed to pop stash", e.what())};
            }
        });
}

auto cmd_drop_stash(std::string repo_path, size_t index) -> tea::CmdBatch {
    return tea::async([repo_path = std::move(repo_path), index]() -> std::optional<tea::Msg> {
        try {
            auto repo = Repository::open(repo_path);
            if (!repo) {
                return tea::StashErrorMsg{std::move(repo.error())};
            }

            auto result = ops::drop_stash(*repo, ops::DropStashParams{.index = index});

            if (!result) {
                return tea::StashErrorMsg{std::move(result.error())};
            }

            return tea::StashDroppedMsg{index};

        } catch (const std::exception& e) {
            return tea::StashErrorMsg{
                make_error(Error::Code::Unknown, "Failed to drop stash", e.what())};
        }
    });
}

auto cmd_load_stash_details(std::string /* repo_path */, domain::Stash /* stash */)
    -> tea::CmdBatch {
    // TODO: Implement stash diff loading when backend supports it
    // For now, return empty diff
    return tea::just(tea::DiffLoadedMsg{std::vector<domain::FileDiff>{}});
}

} // namespace repo::tui::models
