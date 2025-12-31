#include <repo/ops/diff.hpp>
#include <repo/ops/stage.hpp>
#include <repo/repository.hpp>
#include <repo/tui/models/diff.hpp>

#include <algorithm>

namespace repo::tui::models {

using namespace tea;

// Forward declarations
static auto handle_normal_input(DiffModel model, const KeyMsg& key)
    -> std::pair<DiffModel, CmdBatch>;
static auto handle_help_input(DiffModel model, const KeyMsg& key) -> std::pair<DiffModel, CmdBatch>;

// Initialize the diff model
auto init_diff(std::string repo_path, DiffType type) -> std::pair<DiffModel, CmdBatch> {
    DiffModel model;
    model.repo_path = std::move(repo_path);
    model.diff_type = type;
    model.is_loading = true;

    // Load diff immediately
    auto cmds = cmd_load_diff(model.repo_path, model.diff_type);

    return {std::move(model), std::move(cmds)};
}

// Update function - processes messages and returns new model + commands
auto update_diff(DiffModel model, Msg msg) -> std::pair<DiffModel, CmdBatch> {
    // Handle different message types
    return std::visit(
        [&model](auto&& m) -> std::pair<DiffModel, CmdBatch> {
            using T = std::decay_t<decltype(m)>;

            // Keyboard input
            if constexpr (std::is_same_v<T, KeyMsg>) {
                if (model.mode == DiffMode::Help) {
                    return handle_help_input(std::move(model), m);
                } else {
                    return handle_normal_input(std::move(model), m);
                }
            }

            // Diff loaded
            else if constexpr (std::is_same_v<T, DiffLoadedMsg>) {
                model.file_diffs = std::move(m.diffs);
                model.is_loading = false;
                model.error = std::nullopt;

                // Reset navigation
                model.selected_file_index = 0;
                model.selected_hunk_index = 0;
                model.selected_line_index = 0;

                return {std::move(model), none()};
            }

            // Diff load error
            else if constexpr (std::is_same_v<T, DiffErrorMsg>) {
                model.is_loading = false;
                model.error = std::move(m.error);
                return {std::move(model), none()};
            }

            // File staged (after hunk staging)
            else if constexpr (std::is_same_v<T, FileStagedMsg>) {
                model.notification = "Staged hunk in: " + m.path.string();
                // Reload diff to reflect changes
                return {std::move(model), cmd_load_diff(model.repo_path, model.diff_type)};
            }

            // File unstaged
            else if constexpr (std::is_same_v<T, FileUnstagedMsg>) {
                model.notification = "Unstaged hunk in: " + m.path.string();
                // Reload diff to reflect changes
                return {std::move(model), cmd_load_diff(model.repo_path, model.diff_type)};
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
static auto handle_normal_input(DiffModel model, const KeyMsg& key)
    -> std::pair<DiffModel, CmdBatch> {
    // File navigation (J/K - capital letters)
    if (key.type == KeyMsg::Type::Character && key.character == 'J') {
        if (!model.file_diffs.empty() && model.selected_file_index < model.file_diffs.size() - 1) {
            model.selected_file_index++;
            model.selected_hunk_index = 0;
            model.selected_line_index = 0;
        }
        return {std::move(model), none()};
    }

    if (key.type == KeyMsg::Type::Character && key.character == 'K') {
        if (model.selected_file_index > 0) {
            model.selected_file_index--;
            model.selected_hunk_index = 0;
            model.selected_line_index = 0;
        }
        return {std::move(model), none()};
    }

    // Hunk navigation (j/k or arrows)
    if (key.type == KeyMsg::Type::ArrowDown ||
        (key.type == KeyMsg::Type::Character && key.character == 'j')) {
        auto file = selected_file_diff(model);
        if (file && !file->hunks.empty() && model.selected_hunk_index < file->hunks.size() - 1) {
            model.selected_hunk_index++;
            model.selected_line_index = 0;
        }
        return {std::move(model), none()};
    }

    if (key.type == KeyMsg::Type::ArrowUp ||
        (key.type == KeyMsg::Type::Character && key.character == 'k')) {
        if (model.selected_hunk_index > 0) {
            model.selected_hunk_index--;
            model.selected_line_index = 0;
        }
        return {std::move(model), none()};
    }

    // Line navigation (n/p for next/previous)
    if (key.type == KeyMsg::Type::Character && key.character == 'n') {
        auto hunk = selected_hunk(model);
        if (hunk && !hunk->lines.empty() && model.selected_line_index < hunk->lines.size() - 1) {
            model.selected_line_index++;
        }
        return {std::move(model), none()};
    }

    if (key.type == KeyMsg::Type::Character && key.character == 'p') {
        if (model.selected_line_index > 0) {
            model.selected_line_index--;
        }
        return {std::move(model), none()};
    }

    // Home/End - jump to first/last file
    if (key.type == KeyMsg::Type::Home ||
        (key.type == KeyMsg::Type::Character && key.character == 'g')) {
        model.selected_file_index = 0;
        model.selected_hunk_index = 0;
        model.selected_line_index = 0;
        return {std::move(model), none()};
    }

    if (key.type == KeyMsg::Type::End ||
        (key.type == KeyMsg::Type::Character && key.character == 'G')) {
        if (!model.file_diffs.empty()) {
            model.selected_file_index = model.file_diffs.size() - 1;
            model.selected_hunk_index = 0;
            model.selected_line_index = 0;
        }
        return {std::move(model), none()};
    }

    // Stage/unstage entire file (Space)
    if (key.type == KeyMsg::Type::Character && key.character == ' ') {
        auto file = selected_file_diff(model);
        if (file) {
            // Stage/unstage the entire file
            if (model.diff_type == DiffType::Unstaged || model.diff_type == DiffType::All) {
                return {std::move(model), async([repo_path = model.repo_path,
                                                 path = file->path]() -> std::optional<Msg> {
                            try {
                                auto repo = Repository::open(repo_path);
                                if (!repo) {
                                    return ErrorMsg{std::move(repo.error()), "stage file"};
                                }

                                auto result = ops::stage(*repo, ops::StageParams{.paths = {path}});
                                if (!result) {
                                    return ErrorMsg{std::move(result.error()), "stage file"};
                                }

                                return FileStagedMsg{path};
                            } catch (const std::exception& e) {
                                return ErrorMsg{make_error(Error::Code::Unknown,
                                                           "Failed to stage file", e.what()),
                                                "stage file"};
                            }
                        })};
            } else if (model.diff_type == DiffType::Staged) {
                return {
                    std::move(model),
                    async([repo_path = model.repo_path, path = file->path]() -> std::optional<Msg> {
                        try {
                            auto repo = Repository::open(repo_path);
                            if (!repo) {
                                return ErrorMsg{std::move(repo.error()), "unstage file"};
                            }

                            auto result = ops::unstage(*repo, ops::StageParams{.paths = {path}});
                            if (!result) {
                                return ErrorMsg{std::move(result.error()), "unstage file"};
                            }

                            return FileUnstagedMsg{path};
                        } catch (const std::exception& e) {
                            return ErrorMsg{make_error(Error::Code::Unknown,
                                                       "Failed to unstage file", e.what()),
                                            "unstage file"};
                        }
                    })};
            }
        }
        return {std::move(model), none()};
    }

    // Stage hunk (s)
    if (key.type == KeyMsg::Type::Character && key.character == 's') {
        auto file = selected_file_diff(model);
        auto hunk = selected_hunk(model);
        if (file && hunk) {
            model.notification = "Hunk staging not yet implemented";
            // TODO: Implement hunk staging when backend supports it
            // return {std::move(model), cmd_stage_hunk(model.repo_path, file->path, *hunk)};
        }
        return {std::move(model), none()};
    }

    // Unstage hunk (u)
    if (key.type == KeyMsg::Type::Character && key.character == 'u') {
        auto file = selected_file_diff(model);
        auto hunk = selected_hunk(model);
        if (file && hunk) {
            model.notification = "Hunk unstaging not yet implemented";
            // TODO: Implement hunk unstaging when backend supports it
            // return {std::move(model), cmd_unstage_hunk(model.repo_path, file->path, *hunk)};
        }
        return {std::move(model), none()};
    }

    // Toggle diff type (t)
    if (key.type == KeyMsg::Type::Character && key.character == 't') {
        switch (model.diff_type) {
            case DiffType::Unstaged:
                model.diff_type = DiffType::Staged;
                break;
            case DiffType::Staged:
                model.diff_type = DiffType::All;
                break;
            case DiffType::All:
                model.diff_type = DiffType::Unstaged;
                break;
        }
        model.is_loading = true;
        return {std::move(model), cmd_load_diff(model.repo_path, model.diff_type)};
    }

    // Refresh (r)
    if (key.type == KeyMsg::Type::Character && key.character == 'r') {
        model.is_loading = true;
        return {std::move(model), cmd_load_diff(model.repo_path, model.diff_type)};
    }

    // Toggle line numbers (l)
    if (key.type == KeyMsg::Type::Character && key.character == 'l') {
        model.show_line_numbers = !model.show_line_numbers;
        return {std::move(model), none()};
    }

    // Help (?)
    if (key.type == KeyMsg::Type::Character && key.character == '?') {
        model.mode = DiffMode::Help;
        return {std::move(model), none()};
    }

    return {std::move(model), none()};
}

// Helper: Handle help mode
static auto handle_help_input(DiffModel model, const KeyMsg& /* key */)
    -> std::pair<DiffModel, CmdBatch> {
    // Any key exits help
    model.mode = DiffMode::Normal;
    return {std::move(model), none()};
}

// Helper: Get currently selected file diff
auto selected_file_diff(const DiffModel& model) -> std::optional<domain::FileDiff> {
    if (model.selected_file_index < model.file_diffs.size()) {
        return model.file_diffs[model.selected_file_index];
    }
    return std::nullopt;
}

// Helper: Get currently selected hunk
auto selected_hunk(const DiffModel& model) -> std::optional<domain::DiffHunk> {
    auto file = selected_file_diff(model);
    if (file && model.selected_hunk_index < file->hunks.size()) {
        return file->hunks[model.selected_hunk_index];
    }
    return std::nullopt;
}

// Helper: Get currently selected line
auto selected_line(const DiffModel& model) -> std::optional<domain::DiffLine> {
    auto hunk = selected_hunk(model);
    if (hunk && model.selected_line_index < hunk->lines.size()) {
        return hunk->lines[model.selected_line_index];
    }
    return std::nullopt;
}

// Commands for async operations

auto cmd_load_diff(std::string repo_path, DiffType type) -> CmdBatch {
    return async([repo_path = std::move(repo_path), type]() -> std::optional<Msg> {
        try {
            auto repo = Repository::open(repo_path);
            if (!repo) {
                return DiffErrorMsg{std::move(repo.error())};
            }

            // Convert DiffType to ops::DiffParams::Mode
            ops::DiffParams::Mode mode;
            switch (type) {
                case DiffType::Unstaged:
                    mode = ops::DiffParams::Mode::Unstaged;
                    break;
                case DiffType::Staged:
                    mode = ops::DiffParams::Mode::Staged;
                    break;
                case DiffType::All:
                    mode = ops::DiffParams::Mode::All;
                    break;
            }

            auto result = ops::diff(*repo, ops::DiffParams{.mode = mode});
            if (!result) {
                return DiffErrorMsg{std::move(result.error())};
            }

            return DiffLoadedMsg{std::move(result->diffs)};

        } catch (const std::exception& e) {
            return DiffErrorMsg{make_error(Error::Code::Unknown, "Failed to load diff", e.what())};
        }
    });
}

// Placeholder implementations for hunk/line staging
// These will be implemented when the backend supports it

auto cmd_stage_hunk(std::string /* repo_path */, std::filesystem::path /* file_path */,
                    domain::DiffHunk /* hunk */) -> CmdBatch {
    // TODO: Implement hunk staging
    return none();
}

auto cmd_unstage_hunk(std::string /* repo_path */, std::filesystem::path /* file_path */,
                      domain::DiffHunk /* hunk */) -> CmdBatch {
    // TODO: Implement hunk unstaging
    return none();
}

auto cmd_stage_lines(std::string /* repo_path */, std::filesystem::path /* file_path */,
                     std::vector<domain::DiffLine> /* lines */) -> CmdBatch {
    // TODO: Implement line staging
    return none();
}

auto cmd_unstage_lines(std::string /* repo_path */, std::filesystem::path /* file_path */,
                       std::vector<domain::DiffLine> /* lines */) -> CmdBatch {
    // TODO: Implement line unstaging
    return none();
}

} // namespace repo::tui::models
