#include <repo/ops/commit.hpp>
#include <repo/ops/stage.hpp>
#include <repo/ops/status.hpp>
#include <repo/repository.hpp>
#include <repo/tui/models/status.hpp>

#include <algorithm>

namespace repo::tui::models {

using namespace tea;

// Forward declarations
static auto handle_normal_input(StatusModel model, const KeyMsg& key)
    -> std::pair<StatusModel, CmdBatch>;
static auto handle_commit_input(StatusModel model, const KeyMsg& key)
    -> std::pair<StatusModel, CmdBatch>;
static auto handle_help_input(StatusModel model, const KeyMsg& key)
    -> std::pair<StatusModel, CmdBatch>;

// Initialize the status model
auto init_status(std::string repo_path) -> std::pair<StatusModel, CmdBatch> {
    StatusModel model;
    model.repo_path = std::move(repo_path);
    model.is_loading = true;

    // Load status immediately
    auto cmds = cmd_load_status(model.repo_path);

    return {std::move(model), std::move(cmds)};
}

// Update function - processes messages and returns new model + commands
auto update_status(StatusModel model, Msg msg) -> std::pair<StatusModel, CmdBatch> {
    // Handle different message types
    return std::visit(
        [&model](auto&& m) -> std::pair<StatusModel, CmdBatch> {
            using T = std::decay_t<decltype(m)>;

            // Keyboard input
            if constexpr (std::is_same_v<T, KeyMsg>) {
                if (model.mode == StatusMode::CommitInput) {
                    return handle_commit_input(std::move(model), m);
                } else if (model.mode == StatusMode::Help) {
                    return handle_help_input(std::move(model), m);
                } else {
                    return handle_normal_input(std::move(model), m);
                }
            }

            // Status loaded
            else if constexpr (std::is_same_v<T, StatusLoadedMsg>) {
                model.files = std::move(m.files);
                model.is_loading = false;
                model.error = std::nullopt;
                apply_filter(model);
                return {std::move(model), none()};
            }

            // Status load error
            else if constexpr (std::is_same_v<T, StatusErrorMsg>) {
                model.is_loading = false;
                model.error = std::move(m.error);
                return {std::move(model), none()};
            }

            // File staged
            else if constexpr (std::is_same_v<T, FileStagedMsg>) {
                model.notification = "Staged: " + m.path.string();
                // Reload status to reflect changes
                return {std::move(model), cmd_load_status(model.repo_path)};
            }

            // File unstaged
            else if constexpr (std::is_same_v<T, FileUnstagedMsg>) {
                model.notification = "Unstaged: " + m.path.string();
                // Reload status to reflect changes
                return {std::move(model), cmd_load_status(model.repo_path)};
            }

            // Commit created
            else if constexpr (std::is_same_v<T, CommitCreatedMsg>) {
                model.notification = "Created commit: " + m.short_id;
                model.mode = StatusMode::Normal;
                model.commit_message.clear();
                // Reload status
                return {std::move(model), cmd_load_status(model.repo_path)};
            }

            // File operation error
            else if constexpr (std::is_same_v<T, FileOperationErrorMsg>) {
                model.error = std::move(m.error);
                model.notification = "Error: " + m.error.message;
                return {std::move(model), none()};
            }

            // Generic error
            else if constexpr (std::is_same_v<T, ErrorMsg>) {
                model.error = std::move(m.error);
                model.notification = "Error: " + m.error.message;
                return {std::move(model), none()};
            }

            // Unhandled message type
            else {
                return {std::move(model), none()};
            }
        },
        msg);
}

// Helper: Handle normal mode keyboard input
static auto handle_normal_input(StatusModel model, const KeyMsg& key)
    -> std::pair<StatusModel, CmdBatch> {
    // Navigation
    if (key.type == KeyMsg::Type::ArrowDown ||
        (key.type == KeyMsg::Type::Character && key.character == 'j')) {
        if (!model.filtered_files.empty() &&
            model.selected_index < model.filtered_files.size() - 1) {
            model.selected_index++;
        }
        return {std::move(model), none()};
    }

    if (key.type == KeyMsg::Type::ArrowUp ||
        (key.type == KeyMsg::Type::Character && key.character == 'k')) {
        if (model.selected_index > 0) {
            model.selected_index--;
        }
        return {std::move(model), none()};
    }

    // Home/End
    if (key.type == KeyMsg::Type::Home ||
        (key.type == KeyMsg::Type::Character && key.character == 'g')) {
        model.selected_index = 0;
        return {std::move(model), none()};
    }

    if (key.type == KeyMsg::Type::End ||
        (key.type == KeyMsg::Type::Character && key.character == 'G')) {
        if (!model.filtered_files.empty()) {
            model.selected_index = model.filtered_files.size() - 1;
        }
        return {std::move(model), none()};
    }

    // Page Up/Down
    if (key.type == KeyMsg::Type::PageUp) {
        if (model.selected_index >= 10) {
            model.selected_index -= 10;
        } else {
            model.selected_index = 0;
        }
        return {std::move(model), none()};
    }

    if (key.type == KeyMsg::Type::PageDown) {
        if (!model.filtered_files.empty()) {
            model.selected_index =
                std::min(model.selected_index + 10, model.filtered_files.size() - 1);
        }
        return {std::move(model), none()};
    }

    // Stage/Unstage (Space)
    if (key.type == KeyMsg::Type::Character && key.character == ' ') {
        auto file = selected_file(model);
        if (file) {
            // If file is staged, unstage it; otherwise stage it
            if (file->is_staged()) {
                return {std::move(model), cmd_unstage_file(model.repo_path, file->path)};
            } else {
                return {std::move(model), cmd_stage_file(model.repo_path, file->path)};
            }
        }
        return {std::move(model), none()};
    }

    // Stage file (s)
    if (key.type == KeyMsg::Type::Character && key.character == 's') {
        auto file = selected_file(model);
        if (file) {
            return {std::move(model), cmd_stage_file(model.repo_path, file->path)};
        }
        return {std::move(model), none()};
    }

    // Unstage file (u)
    if (key.type == KeyMsg::Type::Character && key.character == 'u') {
        auto file = selected_file(model);
        if (file) {
            return {std::move(model), cmd_unstage_file(model.repo_path, file->path)};
        }
        return {std::move(model), none()};
    }

    // Stage all (a)
    if (key.type == KeyMsg::Type::Character && key.character == 'a') {
        return {std::move(model), cmd_stage_all(model.repo_path, model.files)};
    }

    // Unstage all (A - shift+a)
    if (key.type == KeyMsg::Type::Character && key.character == 'A') {
        return {std::move(model), cmd_unstage_all(model.repo_path, model.files)};
    }

    // Commit (c)
    if (key.type == KeyMsg::Type::Character && key.character == 'c') {
        // Check if there are staged files
        bool has_staged = std::any_of(model.files.begin(), model.files.end(),
                                      [](const auto& f) { return f.is_staged(); });

        if (has_staged) {
            model.mode = StatusMode::CommitInput;
            model.commit_message.clear();
            model.commit_cursor_pos = 0;
        } else {
            model.notification = "No staged files to commit";
        }
        return {std::move(model), none()};
    }

    // Refresh (r)
    if (key.type == KeyMsg::Type::Character && key.character == 'r') {
        model.is_loading = true;
        return {std::move(model), cmd_load_status(model.repo_path)};
    }

    // Toggle filter (f)
    if (key.type == KeyMsg::Type::Character && key.character == 'f') {
        // Cycle through filters
        switch (model.filter) {
            case FileFilter::All:
                model.filter = FileFilter::Staged;
                break;
            case FileFilter::Staged:
                model.filter = FileFilter::Unstaged;
                break;
            case FileFilter::Unstaged:
                model.filter = FileFilter::Untracked;
                break;
            case FileFilter::Untracked:
                model.filter = FileFilter::All;
                break;
        }
        apply_filter(model);
        model.selected_index = 0;
        return {std::move(model), none()};
    }

    // Help (?)
    if (key.type == KeyMsg::Type::Character && key.character == '?') {
        model.mode = StatusMode::Help;
        return {std::move(model), none()};
    }

    return {std::move(model), none()};
}

// Helper: Handle commit input mode
static auto handle_commit_input(StatusModel model, const KeyMsg& key)
    -> std::pair<StatusModel, CmdBatch> {
    // Escape - cancel commit
    if (key.type == KeyMsg::Type::Escape) {
        model.mode = StatusMode::Normal;
        model.commit_message.clear();
        return {std::move(model), none()};
    }

    // Enter - submit commit
    if (key.type == KeyMsg::Type::Enter && key.ctrl) {
        if (!model.commit_message.empty()) {
            auto msg = model.commit_message;
            return {std::move(model), cmd_commit(model.repo_path, std::move(msg))};
        }
        return {std::move(model), none()};
    }

    // Character input
    if (key.type == KeyMsg::Type::Character) {
        model.commit_message.insert(model.commit_cursor_pos, 1, key.character);
        model.commit_cursor_pos++;
        return {std::move(model), none()};
    }

    // Backspace
    if (key.type == KeyMsg::Type::Backspace && model.commit_cursor_pos > 0) {
        model.commit_message.erase(model.commit_cursor_pos - 1, 1);
        model.commit_cursor_pos--;
        return {std::move(model), none()};
    }

    // Delete
    if (key.type == KeyMsg::Type::Delete && model.commit_cursor_pos < model.commit_message.size()) {
        model.commit_message.erase(model.commit_cursor_pos, 1);
        return {std::move(model), none()};
    }

    // Arrow left/right for cursor movement
    if (key.type == KeyMsg::Type::ArrowLeft && model.commit_cursor_pos > 0) {
        model.commit_cursor_pos--;
        return {std::move(model), none()};
    }

    if (key.type == KeyMsg::Type::ArrowRight &&
        model.commit_cursor_pos < model.commit_message.size()) {
        model.commit_cursor_pos++;
        return {std::move(model), none()};
    }

    // Home/End
    if (key.type == KeyMsg::Type::Home) {
        model.commit_cursor_pos = 0;
        return {std::move(model), none()};
    }

    if (key.type == KeyMsg::Type::End) {
        model.commit_cursor_pos = model.commit_message.size();
        return {std::move(model), none()};
    }

    return {std::move(model), none()};
}

// Helper: Handle help mode
static auto handle_help_input(StatusModel model, const KeyMsg& /* key */)
    -> std::pair<StatusModel, CmdBatch> {
    // Any key exits help
    model.mode = StatusMode::Normal;
    return {std::move(model), none()};
}

// Helper: Apply current filter to files
auto apply_filter(StatusModel& model) -> void {
    model.filtered_files.clear();

    for (const auto& file : model.files) {
        bool include = false;

        switch (model.filter) {
            case FileFilter::All:
                include = true;
                break;

            case FileFilter::Staged:
                include = file.is_staged();
                break;

            case FileFilter::Unstaged:
                include = file.is_unstaged();
                break;

            case FileFilter::Untracked:
                include = file.is_untracked();
                break;
        }

        if (include) {
            model.filtered_files.push_back(file);
        }
    }

    // Adjust selection if out of bounds
    if (!model.filtered_files.empty() && model.selected_index >= model.filtered_files.size()) {
        model.selected_index = model.filtered_files.size() - 1;
    }
}

// Helper: Get currently selected file
auto selected_file(const StatusModel& model) -> std::optional<domain::FileStatus> {
    if (model.selected_index < model.filtered_files.size()) {
        return model.filtered_files[model.selected_index];
    }
    return std::nullopt;
}

// Helper: Get all selected files
auto selected_files(const StatusModel& model) -> std::vector<domain::FileStatus> {
    // For now, just return the single selected file
    // Later we can implement multi-selection
    auto file = selected_file(model);
    if (file) {
        return {*file};
    }
    return {};
}

// Commands for async operations

auto cmd_load_status(std::string repo_path) -> CmdBatch {
    return async([repo_path = std::move(repo_path)]() -> std::optional<Msg> {
        try {
            auto repo = Repository::open(repo_path);
            if (!repo) {
                return StatusErrorMsg{std::move(repo.error())};
            }

            auto result = ops::status(*repo);
            if (!result) {
                return StatusErrorMsg{std::move(result.error())};
            }

            return StatusLoadedMsg{std::move(result->files)};

        } catch (const std::exception& e) {
            return StatusErrorMsg{
                make_error(Error::Code::Unknown, "Failed to load status", e.what())};
        }
    });
}

auto cmd_stage_file(std::string repo_path, std::filesystem::path file_path) -> CmdBatch {
    return async([repo_path = std::move(repo_path),
                  file_path = std::move(file_path)]() -> std::optional<Msg> {
        try {
            auto repo = Repository::open(repo_path);
            if (!repo) {
                return FileOperationErrorMsg{std::move(repo.error()), file_path};
            }

            auto result = ops::stage(*repo, ops::StageParams{.paths = {file_path}});
            if (!result) {
                return FileOperationErrorMsg{std::move(result.error()), file_path};
            }

            return FileStagedMsg{file_path};

        } catch (const std::exception& e) {
            return FileOperationErrorMsg{
                make_error(Error::Code::Unknown, "Failed to stage file", e.what()), file_path};
        }
    });
}

auto cmd_unstage_file(std::string repo_path, std::filesystem::path file_path) -> CmdBatch {
    return async([repo_path = std::move(repo_path),
                  file_path = std::move(file_path)]() -> std::optional<Msg> {
        try {
            auto repo = Repository::open(repo_path);
            if (!repo) {
                return FileOperationErrorMsg{std::move(repo.error()), file_path};
            }

            auto result = ops::unstage(*repo, ops::StageParams{.paths = {file_path}});
            if (!result) {
                return FileOperationErrorMsg{std::move(result.error()), file_path};
            }

            return FileUnstagedMsg{file_path};

        } catch (const std::exception& e) {
            return FileOperationErrorMsg{
                make_error(Error::Code::Unknown, "Failed to unstage file", e.what()), file_path};
        }
    });
}

auto cmd_commit(std::string repo_path, std::string message) -> CmdBatch {
    return async([repo_path = std::move(repo_path),
                  message = std::move(message)]() -> std::optional<Msg> {
        try {
            auto repo = Repository::open(repo_path);
            if (!repo) {
                return ErrorMsg{std::move(repo.error()), "commit"};
            }

            auto result = ops::commit(*repo, ops::CommitParams{.message = message});
            if (!result) {
                return ErrorMsg{std::move(result.error()), "commit"};
            }

            return CommitCreatedMsg{result->commit.id, result->commit.id.to_string().substr(0, 7)};

        } catch (const std::exception& e) {
            return ErrorMsg{make_error(Error::Code::Unknown, "Failed to create commit", e.what()),
                            "commit"};
        }
    });
}

auto cmd_stage_all(std::string repo_path, std::vector<domain::FileStatus> files) -> CmdBatch {
    return async(
        [repo_path = std::move(repo_path), files = std::move(files)]() -> std::optional<Msg> {
            try {
                auto repo = Repository::open(repo_path);
                if (!repo) {
                    return StatusErrorMsg{std::move(repo.error())};
                }

                // Stage all unstaged files
                std::vector<std::filesystem::path> paths;
                for (const auto& file : files) {
                    if (file.is_unstaged() || file.is_untracked()) {
                        paths.push_back(file.path);
                    }
                }

                if (!paths.empty()) {
                    auto result = ops::stage(*repo, ops::StageParams{.paths = paths});
                    if (!result) {
                        return StatusErrorMsg{std::move(result.error())};
                    }
                }

                // Reload status
                auto status_result = ops::status(*repo);
                if (!status_result) {
                    return StatusErrorMsg{std::move(status_result.error())};
                }

                return StatusLoadedMsg{std::move(status_result->files)};

            } catch (const std::exception& e) {
                return StatusErrorMsg{
                    make_error(Error::Code::Unknown, "Failed to stage all files", e.what())};
            }
        });
}

auto cmd_unstage_all(std::string repo_path, std::vector<domain::FileStatus> files) -> CmdBatch {
    return async(
        [repo_path = std::move(repo_path), files = std::move(files)]() -> std::optional<Msg> {
            try {
                auto repo = Repository::open(repo_path);
                if (!repo) {
                    return StatusErrorMsg{std::move(repo.error())};
                }

                // Unstage all staged files
                std::vector<std::filesystem::path> paths;
                for (const auto& file : files) {
                    if (file.is_staged()) {
                        paths.push_back(file.path);
                    }
                }

                if (!paths.empty()) {
                    auto result = ops::unstage(*repo, ops::StageParams{.paths = paths});
                    if (!result) {
                        return StatusErrorMsg{std::move(result.error())};
                    }
                }

                // Reload status
                auto status_result = ops::status(*repo);
                if (!status_result) {
                    return StatusErrorMsg{std::move(status_result.error())};
                }

                return StatusLoadedMsg{std::move(status_result->files)};

            } catch (const std::exception& e) {
                return StatusErrorMsg{
                    make_error(Error::Code::Unknown, "Failed to unstage all files", e.what())};
            }
        });
}

} // namespace repo::tui::models
