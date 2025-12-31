#include <repo/ops/branch.hpp>
#include <repo/ops/diff.hpp>
#include <repo/ops/list_commits.hpp>
#include <repo/ops/select_commit.hpp>
#include <repo/ops/switch.hpp>
#include <repo/repository.hpp>
#include <repo/tui/models/log.hpp>

#include <algorithm>

namespace repo::tui::models {

using namespace tea;

// Forward declarations
static auto handle_normal_input(LogModel model, const KeyMsg& key) -> std::pair<LogModel, CmdBatch>;
static auto handle_details_input(LogModel model, const KeyMsg& key)
    -> std::pair<LogModel, CmdBatch>;
static auto handle_help_input(LogModel model, const KeyMsg& key) -> std::pair<LogModel, CmdBatch>;

// Initialize the log model
auto init_log(std::string repo_path) -> std::pair<LogModel, CmdBatch> {
    LogModel model;
    model.repo_path = std::move(repo_path);
    model.is_loading = true;

    // Load initial commits
    auto cmds = cmd_load_log(model.repo_path, model.page_size);

    return {std::move(model), std::move(cmds)};
}

// Update function - processes messages and returns new model + commands
auto update_log(LogModel model, Msg msg) -> std::pair<LogModel, CmdBatch> {
    // Handle different message types
    return std::visit(
        [&model](auto&& m) -> std::pair<LogModel, CmdBatch> {
            using T = std::decay_t<decltype(m)>;

            // Keyboard input
            if constexpr (std::is_same_v<T, KeyMsg>) {
                if (model.mode == LogMode::Details) {
                    return handle_details_input(std::move(model), m);
                } else if (model.mode == LogMode::Help) {
                    return handle_help_input(std::move(model), m);
                } else {
                    return handle_normal_input(std::move(model), m);
                }
            }

            // Log loaded
            else if constexpr (std::is_same_v<T, LogLoadedMsg>) {
                model.commits = std::move(m.commits);
                model.has_more = m.has_more;
                model.is_loading = false;
                model.is_loading_more = false;
                model.error = std::nullopt;
                return {std::move(model), none()};
            }

            // Log load error
            else if constexpr (std::is_same_v<T, LogErrorMsg>) {
                model.is_loading = false;
                model.is_loading_more = false;
                model.error = std::move(m.error);
                return {std::move(model), none()};
            }

            // Diff loaded (for commit details)
            else if constexpr (std::is_same_v<T, DiffLoadedMsg>) {
                // Build diff string from diffs
                std::string diff_text;
                for (const auto& file_diff : m.diffs) {
                    auto old_path =
                        file_diff.old_path ? file_diff.old_path->string() : file_diff.path.string();
                    diff_text +=
                        "diff --git a/" + old_path + " b/" + file_diff.path.string() + "\n";
                    for (const auto& hunk : file_diff.hunks) {
                        diff_text += hunk.header + "\n";
                        for (const auto& line : hunk.lines) {
                            diff_text += line.content + "\n";
                        }
                    }
                }
                model.detail_diff = std::move(diff_text);
                return {std::move(model), none()};
            }

            // Branch created
            else if constexpr (std::is_same_v<T, BranchCreatedMsg>) {
                model.notification = "Created branch: " + m.branch.name;
                return {std::move(model), none()};
            }

            // Branch switched
            else if constexpr (std::is_same_v<T, BranchSwitchedMsg>) {
                model.notification = "Switched to: " + m.name;
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
static auto handle_normal_input(LogModel model, const KeyMsg& key)
    -> std::pair<LogModel, CmdBatch> {
    // Navigation
    if (key.type == KeyMsg::Type::ArrowDown ||
        (key.type == KeyMsg::Type::Character && key.character == 'j')) {
        if (!model.commits.empty() && model.selected_index < model.commits.size() - 1) {
            model.selected_index++;

            // Load more commits if near the end
            if (model.has_more && model.selected_index >= model.commits.size() - 5 &&
                !model.is_loading_more) {
                model.is_loading_more = true;
                return {
                    std::move(model),
                    cmd_load_more_commits(model.repo_path, model.commits.size(), model.page_size)};
            }
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
        if (!model.commits.empty()) {
            model.selected_index = model.commits.size() - 1;
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
        if (!model.commits.empty()) {
            model.selected_index = std::min(model.selected_index + 10, model.commits.size() - 1);
        }
        return {std::move(model), none()};
    }

    // Show commit details (Enter or d)
    if (key.type == KeyMsg::Type::Enter ||
        (key.type == KeyMsg::Type::Character && key.character == 'd')) {
        auto commit = selected_commit(model);
        if (commit) {
            model.mode = LogMode::Details;
            model.detail_commit = *commit;
            // Load diff for this commit
            return {std::move(model), cmd_load_commit_details(model.repo_path, commit->id)};
        }
        return {std::move(model), none()};
    }

    // Cherry-pick commit (s for select)
    if (key.type == KeyMsg::Type::Character && key.character == 's') {
        auto commit = selected_commit(model);
        if (commit) {
            return {std::move(model), cmd_cherry_pick_commit(model.repo_path, commit->id)};
        }
        return {std::move(model), none()};
    }

    // Checkout commit (o for checkout)
    if (key.type == KeyMsg::Type::Character && key.character == 'o') {
        auto commit = selected_commit(model);
        if (commit) {
            return {std::move(model), cmd_checkout_commit(model.repo_path, commit->id)};
        }
        return {std::move(model), none()};
    }

    // Toggle format (f)
    if (key.type == KeyMsg::Type::Character && key.character == 'f') {
        switch (model.format) {
            case LogFormat::Compact:
                model.format = LogFormat::Medium;
                break;
            case LogFormat::Medium:
                model.format = LogFormat::Full;
                break;
            case LogFormat::Full:
                model.format = LogFormat::Compact;
                break;
        }
        return {std::move(model), none()};
    }

    // Refresh (r)
    if (key.type == KeyMsg::Type::Character && key.character == 'r') {
        model.is_loading = true;
        model.selected_index = 0;
        return {std::move(model), cmd_load_log(model.repo_path, model.page_size)};
    }

    // Help (?)
    if (key.type == KeyMsg::Type::Character && key.character == '?') {
        model.mode = LogMode::Help;
        return {std::move(model), none()};
    }

    return {std::move(model), none()};
}

// Helper: Handle details mode
static auto handle_details_input(LogModel model, const KeyMsg& key)
    -> std::pair<LogModel, CmdBatch> {
    // Escape or q - exit details
    if (key.type == KeyMsg::Type::Escape ||
        (key.type == KeyMsg::Type::Character && key.character == 'q')) {
        model.mode = LogMode::Normal;
        model.detail_commit = std::nullopt;
        model.detail_diff = std::nullopt;
        return {std::move(model), none()};
    }

    // Scroll in details (j/k)
    // For now, just exit on any other key
    return {std::move(model), none()};
}

// Helper: Handle help mode
static auto handle_help_input(LogModel model, const KeyMsg& /* key */)
    -> std::pair<LogModel, CmdBatch> {
    // Any key exits help
    model.mode = LogMode::Normal;
    return {std::move(model), none()};
}

// Helper: Get currently selected commit
auto selected_commit(const LogModel& model) -> std::optional<domain::Commit> {
    if (model.selected_index < model.commits.size()) {
        return model.commits[model.selected_index];
    }
    return std::nullopt;
}

// Commands for async operations

auto cmd_load_log(std::string repo_path, size_t max_count, std::optional<std::string> branch)
    -> CmdBatch {
    return async([repo_path = std::move(repo_path), max_count,
                  branch = std::move(branch)]() -> std::optional<Msg> {
        try {
            auto repo = Repository::open(repo_path);
            if (!repo) {
                return LogErrorMsg{std::move(repo.error())};
            }

            // Determine reference to start from
            std::string ref = branch.value_or("HEAD");

            auto result = ops::list_commits(
                *repo, ops::ListCommitsParams{.ref_name = ref, .max_count = max_count});

            if (!result) {
                return LogErrorMsg{std::move(result.error())};
            }

            return LogLoadedMsg{
                std::move(result->commits),
                false // No pagination support yet
            };

        } catch (const std::exception& e) {
            return LogErrorMsg{make_error(Error::Code::Unknown, "Failed to load log", e.what())};
        }
    });
}

auto cmd_load_more_commits(std::string repo_path, size_t /* skip */, size_t max_count) -> CmdBatch {
    return async([repo_path = std::move(repo_path), max_count]() -> std::optional<Msg> {
        try {
            auto repo = Repository::open(repo_path);
            if (!repo) {
                return LogErrorMsg{std::move(repo.error())};
            }

            // Note: list_commits doesn't support skip parameter in current impl
            // For now, just load more commits - the model will append them
            auto result = ops::list_commits(
                *repo, ops::ListCommitsParams{.ref_name = "HEAD", .max_count = max_count});

            if (!result) {
                return LogErrorMsg{std::move(result.error())};
            }

            return LogLoadedMsg{
                std::move(result->commits),
                false // No more pagination support yet
            };

        } catch (const std::exception& e) {
            return LogErrorMsg{
                make_error(Error::Code::Unknown, "Failed to load more commits", e.what())};
        }
    });
}

auto cmd_load_commit_details(std::string repo_path, domain::ObjectId /* commit_id */) -> CmdBatch {
    return async([repo_path = std::move(repo_path)]() -> std::optional<Msg> {
        try {
            auto repo = Repository::open(repo_path);
            if (!repo) {
                return ErrorMsg{std::move(repo.error()), "load commit details"};
            }

            // For now, just return empty diff since the diff operation
            // doesn't support commit-to-commit diffs yet
            // TODO: Implement proper commit diff
            return DiffLoadedMsg{std::vector<domain::FileDiff>{}};

        } catch (const std::exception& e) {
            return ErrorMsg{
                make_error(Error::Code::Unknown, "Failed to load commit details", e.what()),
                "load commit details"};
        }
    });
}

auto cmd_cherry_pick_commit(std::string repo_path, domain::ObjectId commit_id) -> CmdBatch {
    return async([repo_path = std::move(repo_path), commit_id]() -> std::optional<Msg> {
        try {
            auto repo = Repository::open(repo_path);
            if (!repo) {
                return ErrorMsg{std::move(repo.error()), "cherry-pick"};
            }

            auto result = ops::select_commit(*repo, ops::SelectCommitParams{.commit = commit_id});

            if (!result) {
                return ErrorMsg{std::move(result.error()), "cherry-pick"};
            }

            return NotificationMsg{"Cherry-picked commit: " + commit_id.to_string().substr(0, 7),
                                   NotificationMsg::Level::Success};

        } catch (const std::exception& e) {
            return ErrorMsg{
                make_error(Error::Code::Unknown, "Failed to cherry-pick commit", e.what()),
                "cherry-pick"};
        }
    });
}

auto cmd_create_branch_at_commit(std::string repo_path, std::string branch_name,
                                 domain::ObjectId commit_id) -> CmdBatch {
    return async([repo_path = std::move(repo_path), branch_name = std::move(branch_name),
                  commit_id]() -> std::optional<Msg> {
        try {
            auto repo = Repository::open(repo_path);
            if (!repo) {
                return ErrorMsg{std::move(repo.error()), "create branch"};
            }

            auto result = ops::create_branch(
                *repo,
                ops::CreateBranchParams{.name = branch_name, .target = commit_id, .force = false});

            if (!result) {
                return ErrorMsg{std::move(result.error()), "create branch"};
            }

            return BranchCreatedMsg{result->branch};

        } catch (const std::exception& e) {
            return ErrorMsg{make_error(Error::Code::Unknown, "Failed to create branch", e.what()),
                            "create branch"};
        }
    });
}

auto cmd_checkout_commit(std::string repo_path, domain::ObjectId commit_id) -> CmdBatch {
    return async([repo_path = std::move(repo_path), commit_id]() -> std::optional<Msg> {
        try {
            auto repo = Repository::open(repo_path);
            if (!repo) {
                return ErrorMsg{std::move(repo.error()), "checkout"};
            }

            auto result = ops::switch_branch(
                *repo, ops::SwitchParams{
                           .branch_name = commit_id.to_string(),
                           .detach = true // Detach HEAD when checking out a commit
                       });

            if (!result) {
                return ErrorMsg{std::move(result.error()), "checkout"};
            }

            return BranchSwitchedMsg{commit_id.to_string().substr(0, 7)};

        } catch (const std::exception& e) {
            return ErrorMsg{make_error(Error::Code::Unknown, "Failed to checkout commit", e.what()),
                            "checkout"};
        }
    });
}

} // namespace repo::tui::models
