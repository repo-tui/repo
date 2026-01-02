#include <repo/ops/branch.hpp>
#include <repo/ops/clean.hpp>
#include <repo/ops/commit.hpp>
#include <repo/ops/diff.hpp>
#include <repo/ops/init.hpp>
#include <repo/ops/list_commits.hpp>
#include <repo/ops/merge.hpp>
#include <repo/ops/rebase.hpp>
#include <repo/ops/remote.hpp>
#include <repo/ops/restore.hpp>
#include <repo/ops/rollback.hpp>
#include <repo/ops/select_commit.hpp>
#include <repo/ops/stage.hpp>
#include <repo/ops/stash.hpp>
#include <repo/ops/status.hpp>
#include <repo/ops/switch.hpp>
#include <repo/ops/tag.hpp>
#include <repo/ops/undo_commit.hpp>
#include <repo/repository.hpp>
#include <repo/tui/tui.hpp>

// Authentication
#include <repo/backend/credential_helper.hpp>
#include <repo/backend/oauth_device_flow.hpp>
#include <repo/backend/ssh_key_discovery.hpp>

#include <CLI/CLI.hpp>
#include <fmt/color.h>
#include <fmt/core.h>

#include <filesystem>
#include <iostream>
#include <sstream>
#include <unordered_map>

#include "../core/backend/subprocess_utils.hpp"

using namespace repo;

namespace {

// ANSI color helpers
constexpr auto green = fmt::fg(fmt::color::green);
constexpr auto red = fmt::fg(fmt::color::red);
constexpr auto yellow = fmt::fg(fmt::color::yellow);
constexpr auto cyan = fmt::fg(fmt::color::cyan);
constexpr auto gray = fmt::fg(fmt::color::gray);
constexpr auto bold = fmt::emphasis::bold;
constexpr auto dim = fmt::emphasis::faint;

// Init command
// Helper: Interactive project type selector with checkbox menu
auto select_project_types_interactive() -> std::vector<ops::ProjectType> {
    using namespace ops;

    std::vector<ProjectType> selected;
    auto categories = ops::get_project_type_categories();

    fmt::print(bold, "\nSelect project types:\n");
    fmt::print(dim, "(Enter numbers separated by commas, e.g., 3,15,20)\n\n");

    size_t index = 1;
    std::unordered_map<size_t, ProjectType> index_map;

    // Track selected languages for smart filtering
    std::vector<ProjectType> selected_languages;

    for (const auto& category : categories) {
        fmt::print(bold | cyan, "{}:\n", category.name);

        for (const auto& type : category.types) {
            // Smart filtering: hide irrelevant build systems
            if (category.name == "Build Systems") {
                if (!is_build_system_relevant(type, selected_languages)) {
                    continue; // Skip this build system
                }
            }

            index_map[index] = type;
            fmt::print("  {}. {}\n", index, project_type_name(type));
            index++;
        }
        fmt::print("\n");
    }

    fmt::print(bold, "Your selection: ");
    std::string input;
    std::getline(std::cin, input);

    if (input.empty()) {
        return selected; // No templates selected
    }

    // Parse comma-separated numbers
    std::istringstream iss(input);
    std::string token;
    while (std::getline(iss, token, ',')) {
        // Trim whitespace
        token.erase(0, token.find_first_not_of(" \t"));
        token.erase(token.find_last_not_of(" \t") + 1);

        try {
            size_t num = std::stoul(token);
            if (index_map.count(num) > 0) {
                auto type = index_map[num];
                selected.push_back(type);

                // Track languages for filtering build systems
                if (type == ProjectType::Cpp || type == ProjectType::Python ||
                    type == ProjectType::JavaScript || type == ProjectType::Java ||
                    type == ProjectType::Go || type == ProjectType::Rust ||
                    type == ProjectType::CSharp || type == ProjectType::Ruby ||
                    type == ProjectType::PHP || type == ProjectType::Swift ||
                    type == ProjectType::Kotlin) {
                    selected_languages.push_back(type);
                }
            }
        } catch (...) {
            // Invalid number, skip
        }
    }

    return selected;
}

auto cmd_init(const std::string& path_str, bool bare, bool interactive,
              const std::string& types_str) -> int {
    std::filesystem::path path =
        path_str.empty() ? std::filesystem::current_path() : std::filesystem::path(path_str);

    std::vector<ops::ProjectType> project_types;

    // Interactive mode
    if (interactive) {
        fmt::print(bold | cyan, "Initialize a new repository\n\n");
        fmt::print("Location: {}\n", path.string());

        project_types = select_project_types_interactive();

        if (!project_types.empty()) {
            fmt::print(green, "\n✓ ");
            fmt::print("Selected {} template(s)\n", project_types.size());
        }
    }
    // Parse --type flag
    else if (!types_str.empty()) {
        std::istringstream iss(types_str);
        std::string token;
        while (std::getline(iss, token, ',')) {
            // Trim whitespace
            token.erase(0, token.find_first_not_of(" \t"));
            token.erase(token.find_last_not_of(" \t") + 1);

            auto type = ops::parse_project_type(token);
            if (type) {
                project_types.push_back(*type);
            } else {
                fmt::print(stderr, yellow | bold, "Warning: ");
                fmt::print(stderr, "Unknown project type '{}', skipping\n", token);
            }
        }
    }

    // Call ops::init
    ops::InitParams params{.path = path,
                           .bare = bare,
                           .interactive = false, // We handle interactivity here
                           .project_types = project_types,
                           .initial_branch_name = std::nullopt};

    auto result = ops::init(params);
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    // Success output
    fmt::print(green, "✓ ");
    if (bare) {
        fmt::print("Initialized empty bare Git repository in {}\n", result->git_dir.string());
    } else {
        fmt::print("Initialized empty Git repository in {}\n", result->git_dir.string());
    }

    // Show created .gitignore info
    if (result->created_gitignore && !result->applied_templates.empty()) {
        fmt::print(green, "✓ ");
        fmt::print("Created .gitignore for: ");
        for (size_t i = 0; i < result->applied_templates.size(); ++i) {
            if (i > 0)
                fmt::print(", ");
            fmt::print(bold, "{}", ops::project_type_name(result->applied_templates[i]));
        }
        fmt::print("\n");
    }

    return 0;
}

// Find the git repository starting from current directory
auto find_repo() -> Result<Repository> {
    auto current_path = std::filesystem::current_path();

    // Try current directory and parent directories
    while (true) {
        auto git_dir = current_path / ".git";
        if (std::filesystem::exists(git_dir)) {
            return Repository::open(current_path);
        }

        auto parent = current_path.parent_path();
        if (parent == current_path) {
            // Reached root
            Error err;
            err.code = Error::Code::NotARepository;
            err.message = "Not a git repository (or any of the parent directories)";
            return std::unexpected(err);
        }
        current_path = parent;
    }
}

// Status command implementation
auto cmd_status() -> int {
    // Find repository
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    auto& repo = *repo_result;

    // Get status
    auto status_result = ops::status(repo);
    if (!status_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "Failed to get repository status: {}\n", status_result.error().message);
        return 1;
    }

    auto& status = *status_result;

    // Print status
    if (status.is_clean()) {
        fmt::print(green, "✓ ");
        fmt::print("Working tree clean\n");
        return 0;
    }

    // Staged changes
    auto staged = status.staged();
    if (!staged.empty()) {
        fmt::print(fmt::emphasis::bold | green, "Staged changes:\n");
        for (const auto& file : staged) {
            const char* status_char = "?";
            switch (file.index_status) {
                case domain::FileStatus::State::Added:
                    status_char = "A";
                    break;
                case domain::FileStatus::State::Modified:
                    status_char = "M";
                    break;
                case domain::FileStatus::State::Deleted:
                    status_char = "D";
                    break;
                case domain::FileStatus::State::Renamed:
                    status_char = "R";
                    break;
                default:
                    status_char = "?";
            }
            fmt::print(green, "  {} ", status_char);
            fmt::print("{}\n", file.path.string());
        }
        fmt::print("\n");
    }

    // Unstaged changes
    auto unstaged = status.unstaged();
    if (!unstaged.empty()) {
        fmt::print(fmt::emphasis::bold, "Unstaged changes:\n");
        for (const auto& file : unstaged) {
            const char* status_char = "?";
            auto color = yellow; // Default to yellow for modifications
            switch (file.worktree_status) {
                case domain::FileStatus::State::Modified:
                    status_char = "M";
                    color = yellow;
                    break;
                case domain::FileStatus::State::Deleted:
                    status_char = "D";
                    color = red;
                    break;
                default:
                    status_char = "?";
            }
            fmt::print(color, "  {} ", status_char);
            fmt::print("{}\n", file.path.string());
        }
        fmt::print("\n");
    }

    // Untracked files
    auto untracked = status.untracked();
    if (!untracked.empty()) {
        fmt::print(fmt::emphasis::bold | gray, "Untracked files:\n");
        for (const auto& file : untracked) {
            fmt::print(gray, "  ? ");
            fmt::print("{}\n", file.path.string());
        }
        fmt::print("\n");
    }

    return 0;
}

// Diff command
auto cmd_diff(bool staged, bool all) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    // Determine mode
    ops::DiffParams::Mode mode = ops::DiffParams::Mode::Unstaged;
    if (all) {
        mode = ops::DiffParams::Mode::All;
    } else if (staged) {
        mode = ops::DiffParams::Mode::Staged;
    }

    // Get diff
    auto result = ops::diff(*repo_result, {.mode = mode});
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    // Check if there are any changes
    if (result->diffs.empty()) {
        fmt::print(gray, "No changes to show\n");
        return 0;
    }

    // Print summary
    fmt::print(fmt::emphasis::bold, "{} file(s) changed, ", result->files_changed());
    fmt::print(green, "+{} ", result->total_additions());
    fmt::print(red, "-{}\n", result->total_deletions());
    fmt::print("\n");

    // Print each file diff
    for (const auto& file_diff : result->diffs) {
        // File header
        fmt::print(fmt::emphasis::bold, "diff --git a/{} b/{}\n", file_diff.path.string(),
                   file_diff.path.string());

        // Status indicator
        switch (file_diff.status) {
            case domain::FileDiff::Status::Added:
                fmt::print(green, "new file\n");
                break;
            case domain::FileDiff::Status::Deleted:
                fmt::print(red, "deleted file\n");
                break;
            case domain::FileDiff::Status::Modified:
                // No extra status line for modified
                break;
            case domain::FileDiff::Status::Renamed:
                fmt::print(yellow, "renamed from {}\n", file_diff.old_path->string());
                break;
            case domain::FileDiff::Status::Copied:
                fmt::print("copied from {}\n", file_diff.old_path->string());
                break;
            case domain::FileDiff::Status::TypeChanged:
                fmt::print(yellow, "type changed\n");
                break;
        }

        // Binary check
        if (file_diff.is_binary) {
            fmt::print(gray, "Binary file\n\n");
            continue;
        }

        // Print hunks
        for (const auto& hunk : file_diff.hunks) {
            // Hunk header
            fmt::print(fmt::fg(fmt::color::cyan), "{}\n", hunk.header);

            // Lines
            for (const auto& line : hunk.lines) {
                switch (line.origin) {
                    case domain::DiffLine::Origin::Addition:
                        fmt::print(green, "+{}", line.content);
                        break;
                    case domain::DiffLine::Origin::Deletion:
                        fmt::print(red, "-{}", line.content);
                        break;
                    case domain::DiffLine::Origin::Context:
                        fmt::print(" {}", line.content);
                        break;
                }
                // Add newline if content doesn't end with one
                if (line.content.empty() || line.content.back() != '\n') {
                    fmt::print("\n");
                }
            }
        }

        fmt::print("\n");
    }

    return 0;
}

// Clean command
auto cmd_clean(bool dry_run, bool include_directories, bool force, bool include_ignored) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    auto result = ops::clean(*repo_result, {.dry_run = dry_run,
                                            .include_directories = include_directories,
                                            .force = force,
                                            .include_ignored = include_ignored});

    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    if (dry_run) {
        fmt::print(fmt::emphasis::bold | yellow, "Would remove:\n");
    } else {
        fmt::print(green, "✓ ");
        fmt::print("Removed {} file(s) and {} directory(ies)\n", result->removed_files.size(),
                   result->removed_directories.size());
    }

    // List removed files
    for (const auto& file : result->removed_files) {
        if (dry_run) {
            fmt::print(yellow, "  ");
        } else {
            fmt::print(gray, "  ");
        }
        fmt::print("{}\n", file.string());
    }

    // List removed directories
    for (const auto& dir : result->removed_directories) {
        if (dry_run) {
            fmt::print(yellow, "  {} ", dir.string());
            fmt::print(gray, "(directory)\n");
        } else {
            fmt::print(gray, "  {} (directory)\n", dir.string());
        }
    }

    if (dry_run && result->total_removed() > 0) {
        fmt::print(fmt::emphasis::bold | yellow, "\nUse --force to actually remove these files\n");
    }

    return 0;
}

// Commit list command implementation
auto cmd_commit_list(int max_count) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    auto& repo = *repo_result;

    ops::ListCommitsParams params;
    if (max_count > 0) {
        params.max_count = max_count;
    }

    auto result = ops::list_commits(repo, params);
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    auto& commit_result = *result;
    for (const auto& commit : commit_result.commits) {
        fmt::print(yellow, "{} ", commit.id.to_short());
        fmt::print("{}\n", commit.message);
        fmt::print(gray, "  {} <{}> {}\n", commit.author.name, commit.author.email,
                   commit.author.format_time());
    }

    return 0;
}

// Commit amend command
auto cmd_commit_amend(const std::string& message) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    ops::AmendCommitParams params;
    if (!message.empty()) {
        params.message = message;
    }

    auto result = ops::amend_commit(*repo_result, params);
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    fmt::print(green, "✓ ");
    fmt::print("Amended commit {}\n", result->commit.id.to_short());

    return 0;
}

// Commit show command
auto cmd_commit_show(const std::string& ref) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    auto result = ops::show_commit(*repo_result, {.ref = ref});
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    const auto& commit = result->commit;

    // Print commit header
    fmt::print(yellow, "commit {}\n", commit.id.to_string());
    fmt::print("Author: {} <{}>\n", commit.author.name, commit.author.email);
    fmt::print("Date:   {}\n\n", commit.author.format_time());

    // Print commit message (indented)
    fmt::print("    {}\n\n", commit.message);

    // TODO: Add diff output when backend support is added

    return 0;
}

// Commit select command implementation
auto cmd_commit_select(const std::string& commit_id, bool no_commit) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    auto& repo = *repo_result;

    auto oid_result = domain::ObjectId::from_string(commit_id);
    if (!oid_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "Invalid commit ID: {}\n", commit_id);
        return 1;
    }

    ops::SelectCommitParams params{.commit = *oid_result, .no_commit = no_commit};

    auto result = ops::select_commit(repo, params);
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    fmt::print(green, "✓ ");
    if (no_commit) {
        fmt::print("Changes applied to working directory\n");
    } else {
        fmt::print("Commit selected successfully\n");
    }

    return 0;
}

// Commit undo command implementation
auto cmd_commit_undo(const std::string& commit_id, bool no_commit) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    auto& repo = *repo_result;

    auto oid_result = domain::ObjectId::from_string(commit_id);
    if (!oid_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "Invalid commit ID: {}\n", commit_id);
        return 1;
    }

    ops::UndoCommitParams params{.commit = *oid_result, .no_commit = no_commit};

    auto result = ops::undo_commit(repo, params);
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    fmt::print(green, "✓ ");
    if (no_commit) {
        fmt::print("Changes reverted in working directory\n");
    } else {
        fmt::print("Commit undone successfully\n");
    }

    return 0;
}

// Branch list command
auto cmd_branch_list(bool include_remote) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    auto result = ops::list_branches(*repo_result, {.include_remote = include_remote});
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    for (const auto& branch : result->branches) {
        if (branch.is_head) {
            fmt::print(green, "* ");
        } else {
            fmt::print("  ");
        }
        fmt::print("{}\n", branch.name);
    }

    return 0;
}

// Branch create command
auto cmd_branch_create(const std::string& name, const std::string& target, bool force) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    auto oid_result = domain::ObjectId::from_string(target);
    if (!oid_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "Invalid commit ID: {}\n", target);
        return 1;
    }

    auto result =
        ops::create_branch(*repo_result, {.name = name, .target = *oid_result, .force = force});
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    fmt::print(green, "✓ ");
    fmt::print("Created branch '{}'\n", name);
    return 0;
}

// Branch delete command
auto cmd_branch_delete(const std::string& name) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    auto result = ops::delete_branch(*repo_result, {.name = name});
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    fmt::print(green, "✓ ");
    fmt::print("Deleted branch '{}'\n", name);
    return 0;
}

// Branch rename command
auto cmd_branch_rename(const std::string& old_name, const std::string& new_name, bool force)
    -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    auto result = ops::rename_branch(*repo_result,
                                     {.old_name = old_name, .new_name = new_name, .force = force});
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);

        // Print helpful detail if available
        if (result.error().detail) {
            fmt::print(stderr, "\n{}\n", *result.error().detail);
        }
        return 1;
    }

    fmt::print(green, "✓ ");
    fmt::print("Renamed branch '{}' to '{}'\n", old_name, new_name);
    return 0;
}

// Branch set-default command (for empty repositories)
auto cmd_branch_set_default(const std::string& name) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    auto result = ops::set_default_branch(*repo_result, {.branch_name = name});
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);

        // Print helpful detail if available
        if (result.error().detail) {
            fmt::print(stderr, "\n{}\n", *result.error().detail);
        }
        return 1;
    }

    fmt::print(green, "✓ ");
    fmt::print("Default branch set to '{}'\n", name);
    fmt::print(dim, "This branch will be created when you make your first commit.\n");
    return 0;
}

// Branch switch command
auto cmd_branch_switch(const std::string& name) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    auto result = ops::switch_branch(*repo_result, {.branch_name = name});
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    fmt::print(green, "✓ ");
    fmt::print("Switched to branch '{}'\n", name);
    return 0;
}

// Branch merge command
auto cmd_branch_merge(const std::string& source, const std::string& strategy_str,
                      const std::string& message) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    // Parse strategy
    ops::MergeParams::Strategy strategy = ops::MergeParams::Strategy::FastForward;
    if (strategy_str == "no-ff") {
        strategy = ops::MergeParams::Strategy::NoFastForward;
    } else if (strategy_str == "ff-only") {
        strategy = ops::MergeParams::Strategy::FastForwardOnly;
    } else if (!strategy_str.empty() && strategy_str != "ff") {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "Invalid merge strategy '{}'. Use: ff, no-ff, or ff-only\n",
                   strategy_str);
        return 1;
    }

    // Perform merge
    auto result = ops::merge(
        *repo_result, {.source = source, .strategy = strategy, .commit = true, .message = message});

    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    // Print result based on status
    fmt::print(green, "✓ ");
    switch (result->status) {
        case ops::MergeResult::Status::FastForward:
            fmt::print("Fast-forward merge completed\n");
            break;
        case ops::MergeResult::Status::MergeCommit:
            fmt::print("Merge commit created\n");
            break;
        case ops::MergeResult::Status::UpToDate:
            fmt::print("Already up to date\n");
            break;
        case ops::MergeResult::Status::Conflicts:
            fmt::print(yellow, "⚠ ");
            fmt::print("Merge completed with conflicts:\n");
            for (const auto& conflict : result->conflicts) {
                fmt::print("  {}\n", conflict.string());
            }
            return 1;
        case ops::MergeResult::Status::Staged:
            fmt::print("Changes staged (use 'repo commit' to complete merge)\n");
            break;
    }

    if (!result->commit_id.empty()) {
        fmt::print(gray, "  commit: {}\n", result->commit_id);
    }

    return 0;
}

// Branch rebase command
auto cmd_branch_rebase(const std::string& onto) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    // Perform rebase
    auto result = ops::rebase(*repo_result, {.onto = onto, .interactive = false});

    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    // Check for conflicts
    if (result->has_conflicts()) {
        fmt::print(yellow, "⚠ ");
        fmt::print("Rebase aborted due to conflicts:\n");
        for (const auto& conflict : result->conflicts) {
            fmt::print("  {}\n", conflict);
        }
        return 1;
    }

    // Success
    fmt::print(green, "✓ ");
    fmt::print("Successfully rebased {} commit(s) onto '{}'\n", result->commits_replayed, onto);
    fmt::print(gray, "  new HEAD: {}\n", result->new_head.to_short());

    return 0;
}

// Stage files command
auto cmd_stage(const std::vector<std::string>& paths) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    std::vector<std::filesystem::path> file_paths;
    for (const auto& p : paths) {
        file_paths.push_back(p);
    }

    auto result = ops::stage(*repo_result, {.paths = file_paths});
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    fmt::print(green, "✓ ");
    fmt::print("Staged {} file(s)\n", result->staged.size());
    return 0;
}

// Unstage files command
auto cmd_unstage(const std::vector<std::string>& paths) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    std::vector<std::filesystem::path> file_paths;
    for (const auto& p : paths) {
        file_paths.push_back(p);
    }

    auto result = ops::unstage(*repo_result, {.paths = file_paths});
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    fmt::print(green, "✓ ");
    fmt::print("Unstaged {} file(s)\n", result->staged.size());
    return 0;
}

// Restore files command
auto cmd_restore(const std::vector<std::string>& paths, bool staged) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    std::vector<std::filesystem::path> file_paths;
    for (const auto& p : paths) {
        file_paths.push_back(p);
    }

    auto result = ops::restore(*repo_result, {.paths = file_paths, .staged = staged});
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    fmt::print(green, "✓ ");
    fmt::print("Restored {} file(s)\n", result->restored.size());
    return 0;
}

// Remote list command
auto cmd_remote_list() -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    auto result = ops::list_remotes(*repo_result);
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    for (const auto& remote : result->remotes) {
        fmt::print("{}\t{}\n", remote.name, remote.url);
    }
    return 0;
}

// Remote add command
auto cmd_remote_add(const std::string& name, const std::string& url) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    auto result = ops::add_remote(*repo_result, {.name = name, .url = url});
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    fmt::print(green, "✓ ");
    fmt::print("Added remote '{}'\n", name);
    return 0;
}

// Remote remove command
auto cmd_remote_remove(const std::string& name) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    auto result = ops::remove_remote(*repo_result, {.name = name});
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    fmt::print(green, "✓ ");
    fmt::print("Removed remote '{}'\n", name);
    return 0;
}

// Remote show command
auto cmd_remote_show(const std::string& name) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    auto result = ops::list_remotes(*repo_result);
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    // Find the remote by name
    const domain::Remote* found_remote = nullptr;
    for (const auto& remote : result->remotes) {
        if (remote.name == name) {
            found_remote = &remote;
            break;
        }
    }

    if (!found_remote) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "Remote '{}' not found\n", name);
        return 1;
    }

    // Display remote details
    fmt::print(fmt::emphasis::bold, "* remote {}\n", found_remote->name);
    fmt::print("  Fetch URL: {}\n", found_remote->url);

    if (found_remote->push_url.has_value()) {
        fmt::print("  Push  URL: {}\n", *found_remote->push_url);
    } else {
        fmt::print("  Push  URL: {}\n", found_remote->url);
    }

    if (!found_remote->fetch_refspecs.empty()) {
        fmt::print("  Fetch refspecs:\n");
        for (const auto& refspec : found_remote->fetch_refspecs) {
            fmt::print("    {}\n", refspec);
        }
    }

    if (!found_remote->push_refspecs.empty()) {
        fmt::print("  Push refspecs:\n");
        for (const auto& refspec : found_remote->push_refspecs) {
            fmt::print("    {}\n", refspec);
        }
    }

    return 0;
}

// Remote fetch command
auto cmd_remote_fetch(const std::string& remote, bool prune, bool no_tags) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    // Perform fetch
    auto result = ops::fetch(*repo_result,
                             {.remote = remote, .refspec = "", .prune = prune, .tags = !no_tags});

    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    // Display results
    fmt::print(green, "✓ ");
    fmt::print("Fetch completed from '{}'\n", remote.empty() ? "origin" : remote);

    if (result->received_objects > 0) {
        fmt::print(gray, "  Received {} objects, {} bytes\n", result->received_objects,
                   result->received_bytes);
    }

    if (!result->updated_refs.empty()) {
        fmt::print(gray, "  Updated {} reference(s)\n", result->updated_refs.size());
    }

    return 0;
}

// Remote push command
auto cmd_remote_push(const std::string& remote, bool force, bool set_upstream) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    // Perform push
    auto result =
        ops::push(*repo_result,
                  {.remote = remote, .refspec = "", .force = force, .set_upstream = set_upstream});

    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    // Display results
    fmt::print(green, "✓ ");
    fmt::print("Push completed to '{}'\n", remote.empty() ? "origin" : remote);

    if (!result->updated_refs.empty()) {
        fmt::print(gray, "  Updated {} reference(s)\n", result->updated_refs.size());
    }

    return 0;
}

// Remote pull command
auto cmd_remote_pull(const std::string& remote, bool rebase, bool prune, bool no_tags) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    // Perform pull
    auto result = ops::pull(*repo_result,
                            {.remote = remote, .rebase = rebase, .prune = prune, .tags = !no_tags});

    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    // Display fetch results
    if (result->fetch_result.received_objects > 0) {
        fmt::print(gray, "Fetched {} objects, {} bytes\n", result->fetch_result.received_objects,
                   result->fetch_result.received_bytes);
    }

    // Display merge results
    if (result->updated) {
        fmt::print(green, "✓ ");
        fmt::print("Updated branch ({})\n", result->merge_type);
    } else {
        fmt::print(gray, "Already up to date\n");
    }

    return 0;
}

// Tag list command
auto cmd_tag_list() -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    auto result = ops::list_tags(*repo_result);
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    for (const auto& tag : result->tags) {
        fmt::print("{}\n", tag.name);
    }
    return 0;
}

// Tag delete command
auto cmd_tag_delete(const std::string& name) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    auto result = ops::delete_tag(*repo_result, {.name = name});
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    fmt::print(green, "✓ ");
    fmt::print("Deleted tag '{}'\n", name);
    return 0;
}

// Tag create command
auto cmd_tag_create(const std::string& name, const std::string& target_ref,
                    const std::string& message, bool force) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    auto& repo = *repo_result;

    // Resolve target to ObjectId
    domain::ObjectId target_oid;
    if (target_ref.empty()) {
        // Default to HEAD
        auto head = repo.backend().get_head(repo.repo_handle());
        if (!head) {
            fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
            fmt::print(stderr, "Failed to get HEAD: {}\n", head.error().message);
            return 1;
        }

        if (std::holds_alternative<domain::ObjectId>(head->target)) {
            target_oid = std::get<domain::ObjectId>(head->target);
        } else {
            // Symbolic reference, resolve it
            auto target_name = std::get<std::string>(head->target);
            auto target = repo.backend().resolve_reference(repo.repo_handle(), target_name);
            if (!target || !std::holds_alternative<domain::ObjectId>(target->target)) {
                fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
                fmt::print(stderr, "Failed to resolve HEAD target\n");
                return 1;
            }
            target_oid = std::get<domain::ObjectId>(target->target);
        }
    } else {
        // Try to parse as ObjectId first
        auto oid_result = domain::ObjectId::from_string(target_ref);
        if (oid_result) {
            target_oid = *oid_result;
        } else {
            // Try to resolve as reference
            auto ref = repo.backend().resolve_reference(repo.repo_handle(), target_ref);
            if (!ref) {
                fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
                fmt::print(stderr, "Invalid target: {}\n", target_ref);
                return 1;
            }

            if (std::holds_alternative<domain::ObjectId>(ref->target)) {
                target_oid = std::get<domain::ObjectId>(ref->target);
            } else {
                // Symbolic reference, resolve it
                auto target_name = std::get<std::string>(ref->target);
                auto target = repo.backend().resolve_reference(repo.repo_handle(), target_name);
                if (!target || !std::holds_alternative<domain::ObjectId>(target->target)) {
                    fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
                    fmt::print(stderr, "Failed to resolve reference target\n");
                    return 1;
                }
                target_oid = std::get<domain::ObjectId>(target->target);
            }
        }
    }

    // Create default signature (for annotated tags)
    domain::Signature sig{.name = "Test User",
                          .email = "test@example.com",
                          .when = std::chrono::system_clock::now(),
                          .tz_offset = std::chrono::minutes(0)};

    // Create the tag
    auto result = ops::create_tag(
        repo,
        {.name = name, .target = target_oid, .message = message, .tagger = sig, .force = force});

    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    fmt::print(green, "✓ ");
    if (message.empty()) {
        fmt::print("Created lightweight tag '{}'\n", name);
    } else {
        fmt::print("Created annotated tag '{}'\n", name);
    }
    return 0;
}

// Tag show command
auto cmd_tag_show(const std::string& name) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    auto result = ops::list_tags(*repo_result);
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    // Find the tag by name
    const domain::Tag* found_tag = nullptr;
    for (const auto& tag : result->tags) {
        if (tag.name == name) {
            found_tag = &tag;
            break;
        }
    }

    if (!found_tag) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "Tag '{}' not found\n", name);
        return 1;
    }

    // Display tag details
    fmt::print(yellow, "tag {}\n", found_tag->name);

    if (found_tag->is_annotated && found_tag->tag_object_id.has_value()) {
        fmt::print("Tag:    {}\n", found_tag->tag_object_id->to_string());
    }

    if (found_tag->is_annotated && found_tag->tagger.has_value()) {
        fmt::print("Tagger: {} <{}>\n", found_tag->tagger->name, found_tag->tagger->email);
        fmt::print("Date:   {}\n\n", found_tag->tagger->format_time());
    }

    if (found_tag->is_annotated && found_tag->message.has_value()) {
        fmt::print("{}\n\n", *found_tag->message);
    }

    fmt::print("commit {}\n", found_tag->target.to_string());

    if (!found_tag->is_annotated) {
        fmt::print(gray, "(lightweight tag)\n");
    }

    return 0;
}

// Commit create command
auto cmd_commit_create(const std::string& message) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    auto result = ops::commit(*repo_result, {.message = message});
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    fmt::print(green, "✓ ");
    fmt::print("Created commit {}\n", result->commit.id.to_short());
    fmt::print("  {} file(s) changed, +{} -{}\n", result->files_changed, result->insertions,
               result->deletions);
    return 0;
}

// Commit rollback command
auto cmd_commit_rollback(const std::string& target, const std::string& mode) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    auto oid_result = domain::ObjectId::from_string(target);
    if (!oid_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "Invalid commit ID: {}\n", target);
        return 1;
    }

    ops::RollbackMode rollback_mode = ops::RollbackMode::Mixed;
    if (mode == "soft") {
        rollback_mode = ops::RollbackMode::Soft;
    } else if (mode == "hard") {
        rollback_mode = ops::RollbackMode::Hard;
    }

    auto result = ops::rollback(*repo_result, {.target = *oid_result, .mode = rollback_mode});
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    fmt::print(green, "✓ ");
    fmt::print("Rolled back to {}\n", target);
    return 0;
}

// Stash create command
auto cmd_stash_create(const std::string& message) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    // Create default signature
    domain::Signature sig{.name = "Stasher",
                          .email = "stasher@example.com",
                          .when = std::chrono::system_clock::now(),
                          .tz_offset = std::chrono::minutes{0}};

    auto result = ops::create_stash(*repo_result, {.message = message, .stasher = sig});
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    fmt::print(green, "✓ ");
    fmt::print("Created stash: {}\n", message);
    return 0;
}

// Stash list command
auto cmd_stash_list() -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    auto result = ops::list_stashes(*repo_result);
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    for (size_t i = 0; i < result->stashes.size(); ++i) {
        const auto& stash = result->stashes[i];
        fmt::print("stash@{{{}}}: {}\n", i, stash.message);
    }
    return 0;
}

// Stash show command
auto cmd_stash_show(int index) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    auto result = ops::list_stashes(*repo_result);
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    // Find stash by index
    if (index < 0 || static_cast<size_t>(index) >= result->stashes.size()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "Stash index {} not found\n", index);
        return 1;
    }

    const auto& stash = result->stashes[index];

    // Display stash details
    fmt::print(yellow, "stash@{{{}}}: {}\n", index, stash.message);
    fmt::print("Author: {} <{}>\n", stash.author.name, stash.author.email);
    fmt::print("Date:   {}\n\n", stash.author.format_time());
    fmt::print("Commit: {}\n", stash.commit_id.to_string());

    return 0;
}

// Stash apply command
auto cmd_stash_apply(int index) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    auto result = ops::apply_stash(*repo_result, {.index = static_cast<size_t>(index)});
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    fmt::print(green, "✓ ");
    fmt::print("Applied stash@{{{}}}\n", index);
    return 0;
}

// Stash pop command
auto cmd_stash_pop(int index) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    auto result = ops::pop_stash(*repo_result, {.index = static_cast<size_t>(index)});
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    fmt::print(green, "✓ ");
    fmt::print("Popped stash@{{{}}}\n", index);
    return 0;
}

// Stash drop command
auto cmd_stash_drop(int index) -> int {
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", repo_result.error().message);
        return 1;
    }

    auto result = ops::drop_stash(*repo_result, {.index = static_cast<size_t>(index)});
    if (!result.has_value()) {
        fmt::print(stderr, fmt::emphasis::bold | red, "Error: ");
        fmt::print(stderr, "{}\n", result.error().message);
        return 1;
    }

    fmt::print(green, "✓ ");
    fmt::print("Dropped stash@{{{}}}\n", index);
    return 0;
}

// Auth commands
auto cmd_auth_status() -> int {
    fmt::print(bold | cyan, "Authentication Status\n");
    fmt::print(bold | cyan, "====================\n\n");

    // Check credential helper
    fmt::print(bold, "Git Credential Helper:\n");
    auto helper_check = repo::backend::CredentialHelper().is_available();
    if (helper_check) {
        fmt::print(green, "  ✓ ");
        fmt::print("Configured and available\n");

        // Get the configured helper using modern C++
        auto git_binary = repo::backend::find_git_binary();
        if (git_binary) {
            auto result =
                repo::backend::run_subprocess(*git_binary + " config --get credential.helper");
            if (result && result->success()) {
                std::string helper_name = result->stdout_output;
                if (!helper_name.empty() && helper_name.back() == '\n') {
                    helper_name.pop_back();
                }
                if (!helper_name.empty()) {
                    fmt::print("    Helper: {}\n", helper_name);
                }
            }
        }
    } else {
        fmt::print(yellow, "  ⚠ ");
        fmt::print("Not configured\n");
        fmt::print("    Setup: git config --global credential.helper <helper>\n");
    }

    // Check SSH keys
    fmt::print("\n");
    fmt::print(bold, "SSH Keys:\n");
    auto ssh_keys = repo::backend::SSHKeyDiscovery::discover_keys();
    if (ssh_keys.empty()) {
        fmt::print(yellow, "  ⚠ ");
        fmt::print("No SSH keys found in ~/.ssh/\n");
        fmt::print("    Generate: ssh-keygen -t ed25519 -C \"your_email@example.com\"\n");
    } else {
        fmt::print(green, "  ✓ ");
        fmt::print("Found {} key pair(s)\n", ssh_keys.size());
        for (const auto& key : ssh_keys) {
            fmt::print("    • {} ({}{})\n", key.private_key.filename().string(), key.key_type,
                       key.is_encrypted ? ", encrypted" : "");
        }
    }

    // Check ssh-agent using modern C++
    fmt::print("\n");
    fmt::print(bold, "SSH Agent:\n");
    auto ssh_add = repo::backend::find_binary("ssh-add");
    if (ssh_add) {
        auto result = repo::backend::run_subprocess(*ssh_add + " -l");
        if (result && result->success()) {
            // Count lines in output (each line = one key)
            int key_count = 0;
            std::istringstream iss(result->stdout_output);
            std::string line;
            while (std::getline(iss, line)) {
                if (!line.empty()) {
                    key_count++;
                }
            }
            if (key_count > 0) {
                fmt::print(green, "  ✓ ");
                fmt::print("Running with {} key(s) loaded\n", key_count);
            } else {
                fmt::print(gray, "  • ");
                fmt::print("Not running or no keys loaded\n");
            }
        } else {
            fmt::print(gray, "  • ");
            fmt::print("Not running or no keys loaded\n");
        }
    } else {
        fmt::print(gray, "  • ");
        fmt::print("Not running or no keys loaded\n");
    }

    fmt::print("\n");
    return 0;
}

auto cmd_auth_test(const std::string& url) -> int {
    fmt::print(bold, "Testing authentication for: ");
    fmt::print("{}\n\n", url);

    // Detect provider
    auto provider = repo::backend::OAuthDeviceFlow::detect_provider(url);
    if (provider) {
        std::string provider_name =
            *provider == repo::backend::OAuthDeviceFlow::Provider::GitHub ? "GitHub" : "GitLab";
        fmt::print("Provider: {}\n", provider_name);
    }

    // Determine URL type
    bool is_ssh = url.find("git@") != std::string::npos || url.find("ssh://") != std::string::npos;
    bool is_https = url.find("https://") != std::string::npos;

    if (is_ssh) {
        fmt::print("Protocol: SSH\n\n");

        // Check SSH keys
        auto keys = repo::backend::SSHKeyDiscovery::discover_keys();
        if (keys.empty()) {
            fmt::print(yellow, "⚠ Warning: ");
            fmt::print("No SSH keys found in ~/.ssh/\n");
            fmt::print("\nGenerate a key:\n");
            fmt::print("  ssh-keygen -t ed25519 -C \"your_email@example.com\"\n");
            return 1;
        }

        fmt::print(green, "✓ ");
        fmt::print("Found {} SSH key pair(s)\n", keys.size());

        // Extract host from URL
        std::string host;
        if (url.find("git@") == 0) {
            auto colon_pos = url.find(':');
            if (colon_pos != std::string::npos) {
                host = url.substr(4, colon_pos - 4);
            }
        }

        if (!host.empty()) {
            fmt::print("\nTest SSH connection:\n");
            fmt::print("  ssh -T git@{}\n", host);
        }

    } else if (is_https) {
        fmt::print("Protocol: HTTPS\n\n");

        // Check credential helper
        auto helper = repo::backend::CredentialHelper();
        if (helper.is_available()) {
            fmt::print(green, "✓ ");
            fmt::print("Credential helper is configured\n");
        } else {
            fmt::print(yellow, "⚠ Warning: ");
            fmt::print("No credential helper configured\n");
            fmt::print("\nSetup credential helper:\n");
            fmt::print("  macOS:  git config --global credential.helper osxkeychain\n");
            fmt::print("  Linux:  git config --global credential.helper libsecret\n");
            return 1;
        }
    }

    fmt::print("\n");
    fmt::print(green, "✓ ");
    fmt::print("Authentication setup looks good!\n");
    fmt::print("\nTo actually test the connection, try:\n");
    fmt::print("  repo remote fetch\n");

    return 0;
}

auto cmd_auth_login(const std::string& provider_name) -> int {
    fmt::print(bold | cyan, "GitHub Authentication\n");
    fmt::print(bold | cyan, "====================\n\n");

    // Check if credential helper is available
    auto helper = repo::backend::CredentialHelper();
    if (!helper.is_available()) {
        fmt::print(yellow, "⚠ Warning: ");
        fmt::print("No credential helper configured. Token will not be saved.\n");
        fmt::print("Setup: git config --global credential.helper osxkeychain\n\n");
    }

    // Determine provider (default to GitHub)
    auto provider = repo::backend::OAuthDeviceFlow::Provider::GitHub;
    if (provider_name == "gitlab") {
        provider = repo::backend::OAuthDeviceFlow::Provider::GitLab;
        fmt::print(bold, "Provider: GitLab\n\n");
    } else {
        fmt::print(bold, "Provider: GitHub\n\n");
    }

    // Start OAuth device flow with default scopes (repo + workflow for GitHub)
    fmt::print("Starting OAuth device flow...\n");
    fmt::print("Scopes: {}\n\n", provider == repo::backend::OAuthDeviceFlow::Provider::GitHub
                                     ? "repo, workflow"
                                     : "write_repository, read_user");

    std::string url = provider == repo::backend::OAuthDeviceFlow::Provider::GitHub
                          ? "https://github.com"
                          : "https://gitlab.com";

    // Use default scopes (pass empty string)
    auto result = repo::backend::OAuthDeviceFlow::authenticate(provider, url, "", "");

    if (!result) {
        fmt::print(red, "✗ Authentication failed: ");
        fmt::print("{}\n", result.error().message);
        if (result.error().detail) {
            fmt::print("\n{}\n", *result.error().detail);
        }
        return 1;
    }

    auto credential = *result;
    std::string token = credential.password; // OAuth token is stored in password field
    fmt::print(green, "\n✓ Authentication successful!\n");
    fmt::print("  Logged in as: {}\n\n", credential.username);

    // Store token in credential helper if available
    if (helper.is_available()) {
        auto store_result = helper.approve(url, credential);
        if (store_result) {
            fmt::print(green, "✓ ");
            fmt::print("Token saved to credential helper\n");
            fmt::print("  Username: {}\n", credential.username);
            fmt::print("  Token: {}...\n", token.substr(0, 8));
            fmt::print("\nYou can now push/pull from {} repositories.\n",
                       provider == repo::backend::OAuthDeviceFlow::Provider::GitHub ? "GitHub"
                                                                                    : "GitLab");
        } else {
            fmt::print(yellow, "⚠ ");
            fmt::print("Failed to save token: {}\n", store_result.error().message);
            fmt::print("\nYour credentials:\n");
            fmt::print("  Username: {}\n", credential.username);
            fmt::print("  Token: {}\n", token);
            fmt::print("\nSave it manually or use it as password when prompted.\n");
        }
    } else {
        fmt::print("Your credentials:\n");
        fmt::print("  Username: {}\n", credential.username);
        fmt::print("  Token: {}\n", token);
        fmt::print("\nUse this token as password when git prompts for credentials.\n");
    }

    return 0;
}

auto cmd_auth_clear(const std::string& url) -> int {
    fmt::print(bold, "Clearing stored credentials for: ");
    fmt::print("{}\n\n", url);

    auto helper = repo::backend::CredentialHelper();
    if (!helper.is_available()) {
        fmt::print(red, "✗ Error: ");
        fmt::print("No credential helper is configured\n");
        return 1;
    }

    auto result = helper.reject(url);
    if (result) {
        fmt::print(green, "✓ ");
        fmt::print("Cleared stored credentials\n");
        fmt::print("\nNext authentication attempt will prompt for new credentials.\n");
        return 0;
    } else {
        fmt::print(red, "✗ Error: ");
        fmt::print("{}\n", result.error().message);
        if (result.error().detail) {
            fmt::print("\n{}\n", *result.error().detail);
        }
        return 1;
    }
}

} // anonymous namespace

auto main(int argc, char* argv[]) -> int {
    // Check for interactive mode flag first
    if (argc == 2 && (std::string(argv[1]) == "-i" || std::string(argv[1]) == "--interactive")) {
        return tui::run();
    }

    CLI::App app{"repo - Modern Git interface with intuitive commands", "repo"};
    app.require_subcommand(1);

    // Global options
    app.set_version_flag("--version", "0.1.0");

    bool launch_tui = false;
    app.add_flag("-i,--interactive", launch_tui, "Launch interactive TUI");

    app.footer("\nEXAMPLES:\n"
               "  repo status\n"
               "  repo stage file.txt\n"
               "  repo commit list\n"
               "  repo branch list\n"
               "  repo switch main\n"
               "  repo -i                  # Launch interactive TUI\n"
               "  repo tui                 # Launch interactive TUI\n");

    // Status command
    auto* status_cmd = app.add_subcommand("status", "Show the working tree status");
    status_cmd->footer("\nEXAMPLES:\n"
                       "  repo status\n"
                       "\n"
                       "OUTPUT FORMAT:\n"
                       "  Staged changes      - Files ready to commit (green)\n"
                       "  Unstaged changes    - Modified (yellow) or deleted (red)\n"
                       "  Untracked files     - New files not tracked (gray)\n");
    status_cmd->callback([]() { std::exit(cmd_status()); });

    // Commit domain
    auto* commit_cmd = app.add_subcommand("commit", "Commit operations");
    commit_cmd->require_subcommand(1);
    commit_cmd->footer("\nEXAMPLES:\n"
                       "  repo commit create -m \"Add feature\"\n"
                       "  repo commit amend -m \"Updated message\"\n"
                       "  repo commit list\n"
                       "  repo commit show HEAD\n"
                       "  repo commit show abc123\n"
                       "  repo commit select abc123\n"
                       "  repo commit undo abc123\n"
                       "  repo commit rollback abc123 --mode=hard\n");

    // commit create
    auto* commit_create_cmd = commit_cmd->add_subcommand("create", "Create a new commit");
    std::string commit_message;
    commit_create_cmd->add_option("-m,--message", commit_message, "Commit message")->required();
    commit_create_cmd->footer("\nEXAMPLES:\n"
                              "  repo commit create -m \"Add feature\"\n"
                              "  repo commit create --message \"Fix bug\"\n");
    commit_create_cmd->callback(
        [&commit_message]() { std::exit(cmd_commit_create(commit_message)); });

    // commit amend
    auto* commit_amend_cmd = commit_cmd->add_subcommand("amend", "Amend the last commit");
    std::string amend_message;
    commit_amend_cmd->add_option("-m,--message", amend_message,
                                 "New commit message (empty = keep existing)");
    commit_amend_cmd->footer("\nEXAMPLES:\n"
                             "  repo commit amend\n"
                             "  repo commit amend -m \"Updated message\"\n"
                             "  repo commit amend --message \"Fix typo\"\n");
    commit_amend_cmd->callback([&amend_message]() { std::exit(cmd_commit_amend(amend_message)); });

    // commit list
    auto* commit_list_cmd = commit_cmd->add_subcommand("list", "List commit history");
    int max_count = 0;
    commit_list_cmd->add_option("--max-count,-n", max_count, "Maximum number of commits to show");
    commit_list_cmd->footer("\nEXAMPLES:\n"
                            "  repo commit list\n"
                            "  repo commit list --max-count=10\n"
                            "  repo commit list -n 5\n");
    commit_list_cmd->callback([&max_count]() { std::exit(cmd_commit_list(max_count)); });

    // commit show
    auto* commit_show_cmd = commit_cmd->add_subcommand("show", "Show commit details");
    std::string show_ref = "HEAD";
    commit_show_cmd->add_option("ref", show_ref, "Commit ID, branch name, or ref (default: HEAD)");
    commit_show_cmd->footer("\nEXAMPLES:\n"
                            "  repo commit show\n"
                            "  repo commit show HEAD\n"
                            "  repo commit show abc123\n"
                            "  repo commit show main\n");
    commit_show_cmd->callback([&show_ref]() { std::exit(cmd_commit_show(show_ref)); });

    // commit select (cherry-pick)
    auto* commit_select_cmd =
        commit_cmd->add_subcommand("select", "Apply a commit to current branch (cherry-pick)");
    std::string select_commit_id;
    bool select_no_commit = false;
    commit_select_cmd->add_option("commit", select_commit_id, "Commit ID to select")->required();
    commit_select_cmd->add_flag("--no-commit", select_no_commit,
                                "Apply changes without creating a commit");
    commit_select_cmd->footer("\nEXAMPLES:\n"
                              "  repo commit select abc123\n"
                              "  repo commit select abc123 --no-commit\n");
    commit_select_cmd->callback([&select_commit_id, &select_no_commit]() {
        std::exit(cmd_commit_select(select_commit_id, select_no_commit));
    });

    // commit undo (revert)
    auto* commit_undo_cmd =
        commit_cmd->add_subcommand("undo", "Undo a commit by creating a new reverting commit");
    std::string undo_commit_id;
    bool undo_no_commit = false;
    commit_undo_cmd->add_option("commit", undo_commit_id, "Commit ID to undo")->required();
    commit_undo_cmd->add_flag("--no-commit", undo_no_commit,
                              "Revert changes without creating a commit");
    commit_undo_cmd->footer("\nEXAMPLES:\n"
                            "  repo commit undo abc123\n"
                            "  repo commit undo abc123 --no-commit\n");
    commit_undo_cmd->callback([&undo_commit_id, &undo_no_commit]() {
        std::exit(cmd_commit_undo(undo_commit_id, undo_no_commit));
    });

    // commit rollback (reset)
    auto* commit_rollback_cmd =
        commit_cmd->add_subcommand("rollback", "Rollback to a previous commit (reset)");
    std::string rollback_target;
    std::string rollback_mode = "mixed";
    commit_rollback_cmd->add_option("target", rollback_target, "Target commit ID")->required();
    commit_rollback_cmd->add_option("--mode", rollback_mode, "Rollback mode: soft, mixed, or hard")
        ->check(CLI::IsMember({"soft", "mixed", "hard"}));
    commit_rollback_cmd->footer("\nEXAMPLES:\n"
                                "  repo commit rollback abc123\n"
                                "  repo commit rollback abc123 --mode=soft\n"
                                "  repo commit rollback abc123 --mode=hard\n");
    commit_rollback_cmd->callback([&rollback_target, &rollback_mode]() {
        std::exit(cmd_commit_rollback(rollback_target, rollback_mode));
    });

    // Stash domain
    auto* stash_cmd = app.add_subcommand("stash", "Stash operations");
    stash_cmd->require_subcommand(1);
    stash_cmd->footer("\nEXAMPLES:\n"
                      "  repo stash create \"WIP: feature\"\n"
                      "  repo stash list\n"
                      "  repo stash show 0\n"
                      "  repo stash apply 0\n"
                      "  repo stash pop 0\n"
                      "  repo stash drop 0\n");

    // stash create
    auto* stash_create_cmd = stash_cmd->add_subcommand("create", "Create a new stash");
    std::string stash_message;
    stash_create_cmd->add_option("message", stash_message, "Stash message")->required();
    stash_create_cmd->footer("\nEXAMPLES:\n"
                             "  repo stash create \"WIP: feature\"\n");
    stash_create_cmd->callback([&stash_message]() { std::exit(cmd_stash_create(stash_message)); });

    // stash list
    auto* stash_list_cmd = stash_cmd->add_subcommand("list", "List all stashes");
    stash_list_cmd->footer("\nEXAMPLES:\n"
                           "  repo stash list\n");
    stash_list_cmd->callback([]() { std::exit(cmd_stash_list()); });

    // stash show
    auto* stash_show_cmd = stash_cmd->add_subcommand("show", "Show stash details");
    int show_stash_index = 0;
    stash_show_cmd->add_option("index", show_stash_index, "Stash index (default: 0)");
    stash_show_cmd->footer("\nEXAMPLES:\n"
                           "  repo stash show\n"
                           "  repo stash show 1\n");
    stash_show_cmd->callback(
        [&show_stash_index]() { std::exit(cmd_stash_show(show_stash_index)); });

    // stash apply
    auto* stash_apply_cmd = stash_cmd->add_subcommand("apply", "Apply a stash without removing it");
    int apply_index = 0;
    stash_apply_cmd->add_option("index", apply_index, "Stash index (default: 0)");
    stash_apply_cmd->footer("\nEXAMPLES:\n"
                            "  repo stash apply\n"
                            "  repo stash apply 1\n");
    stash_apply_cmd->callback([&apply_index]() { std::exit(cmd_stash_apply(apply_index)); });

    // stash pop
    auto* stash_pop_cmd = stash_cmd->add_subcommand("pop", "Apply and remove a stash");
    int pop_index = 0;
    stash_pop_cmd->add_option("index", pop_index, "Stash index (default: 0)");
    stash_pop_cmd->footer("\nEXAMPLES:\n"
                          "  repo stash pop\n"
                          "  repo stash pop 1\n");
    stash_pop_cmd->callback([&pop_index]() { std::exit(cmd_stash_pop(pop_index)); });

    // stash drop
    auto* stash_drop_cmd = stash_cmd->add_subcommand("drop", "Remove a stash without applying it");
    int drop_index = 0;
    stash_drop_cmd->add_option("index", drop_index, "Stash index (default: 0)");
    stash_drop_cmd->footer("\nEXAMPLES:\n"
                           "  repo stash drop\n"
                           "  repo stash drop 1\n");
    stash_drop_cmd->callback([&drop_index]() { std::exit(cmd_stash_drop(drop_index)); });

    // Branch domain
    auto* branch_cmd = app.add_subcommand("branch", "Branch operations");
    branch_cmd->require_subcommand(1);
    branch_cmd->footer("\nEXAMPLES:\n"
                       "  repo branch list\n"
                       "  repo branch create feature abc123\n"
                       "  repo branch rename old-name new-name\n"
                       "  repo branch switch main\n"
                       "  repo branch merge feature\n"
                       "  repo branch rebase main\n"
                       "  repo branch delete old-feature\n");

    // branch list
    auto* branch_list_cmd = branch_cmd->add_subcommand("list", "List branches");
    bool include_remote = false;
    branch_list_cmd->add_flag("--remote,-r", include_remote, "Include remote branches");
    branch_list_cmd->footer("\nEXAMPLES:\n"
                            "  repo branch list\n"
                            "  repo branch list --remote\n");
    branch_list_cmd->callback([&include_remote]() { std::exit(cmd_branch_list(include_remote)); });

    // branch create
    auto* branch_create_cmd = branch_cmd->add_subcommand("create", "Create a new branch");
    std::string branch_name, branch_target;
    bool branch_force = false;
    branch_create_cmd->add_option("name", branch_name, "Branch name")->required();
    branch_create_cmd->add_option("target", branch_target, "Target commit ID")->required();
    branch_create_cmd->add_flag("--force,-f", branch_force, "Overwrite if exists");
    branch_create_cmd->footer("\nEXAMPLES:\n"
                              "  repo branch create feature abc123\n"
                              "  repo branch create feature abc123 --force\n");
    branch_create_cmd->callback([&branch_name, &branch_target, &branch_force]() {
        std::exit(cmd_branch_create(branch_name, branch_target, branch_force));
    });

    // branch delete
    auto* branch_delete_cmd = branch_cmd->add_subcommand("delete", "Delete a branch");
    std::string delete_branch_name;
    branch_delete_cmd->add_option("name", delete_branch_name, "Branch name")->required();
    branch_delete_cmd->footer("\nEXAMPLES:\n"
                              "  repo branch delete old-feature\n");
    branch_delete_cmd->callback(
        [&delete_branch_name]() { std::exit(cmd_branch_delete(delete_branch_name)); });

    // branch rename
    auto* branch_rename_cmd = branch_cmd->add_subcommand("rename", "Rename a branch");
    std::string rename_old_name, rename_new_name;
    bool rename_force = false;
    branch_rename_cmd->add_option("old", rename_old_name, "Current branch name")->required();
    branch_rename_cmd->add_option("new", rename_new_name, "New branch name")->required();
    branch_rename_cmd->add_flag("--force,-f", rename_force, "Overwrite if new name exists");
    branch_rename_cmd->footer("\nEXAMPLES:\n"
                              "  repo branch rename old-name new-name\n"
                              "  repo branch rename feature better-feature\n"
                              "  repo branch rename old new --force\n");
    branch_rename_cmd->callback([&rename_old_name, &rename_new_name, &rename_force]() {
        std::exit(cmd_branch_rename(rename_old_name, rename_new_name, rename_force));
    });

    // branch set-default
    auto* branch_set_default_cmd =
        branch_cmd->add_subcommand("set-default", "Set default branch for empty repository");
    std::string set_default_name;
    branch_set_default_cmd->add_option("name", set_default_name, "Branch name")->required();
    branch_set_default_cmd->footer("\nEXAMPLES:\n"
                                   "  repo branch set-default main\n"
                                   "  repo branch set-default develop\n\n"
                                   "NOTE: Only works on repositories with no commits yet.\n"
                                   "      For existing branches, use 'repo branch rename'.\n");
    branch_set_default_cmd->callback(
        [&set_default_name]() { std::exit(cmd_branch_set_default(set_default_name)); });

    // branch switch
    auto* branch_switch_cmd = branch_cmd->add_subcommand("switch", "Switch to a branch");
    std::string switch_branch_name;
    branch_switch_cmd->add_option("name", switch_branch_name, "Branch name")->required();
    branch_switch_cmd->footer("\nEXAMPLES:\n"
                              "  repo branch switch main\n"
                              "  repo branch switch feature\n");
    branch_switch_cmd->callback(
        [&switch_branch_name]() { std::exit(cmd_branch_switch(switch_branch_name)); });

    // branch merge
    auto* branch_merge_cmd =
        branch_cmd->add_subcommand("merge", "Merge a branch into current branch");
    std::string merge_source;
    std::string merge_strategy;
    std::string merge_message;
    branch_merge_cmd->add_option("source", merge_source, "Branch or commit to merge")->required();
    branch_merge_cmd->add_option("--strategy,-s", merge_strategy,
                                 "Merge strategy: ff, no-ff, ff-only");
    branch_merge_cmd->add_option("--message,-m", merge_message, "Custom merge commit message");
    branch_merge_cmd->footer("\nEXAMPLES:\n"
                             "  repo branch merge feature\n"
                             "  repo branch merge feature --strategy no-ff\n"
                             "  repo branch merge feature --strategy ff-only\n"
                             "  repo branch merge abc123 --message \"Merge hotfix\"\n");
    branch_merge_cmd->callback([&merge_source, &merge_strategy, &merge_message]() {
        std::exit(cmd_branch_merge(merge_source, merge_strategy, merge_message));
    });

    // branch rebase
    auto* branch_rebase_cmd =
        branch_cmd->add_subcommand("rebase", "Rebase current branch onto another");
    std::string rebase_onto;
    branch_rebase_cmd->add_option("onto", rebase_onto, "Branch or commit to rebase onto")
        ->required();
    branch_rebase_cmd->footer("\nEXAMPLES:\n"
                              "  repo branch rebase main\n"
                              "  repo branch rebase origin/main\n"
                              "  repo branch rebase abc123\n");
    branch_rebase_cmd->callback([&rebase_onto]() { std::exit(cmd_branch_rebase(rebase_onto)); });

    // File domain
    auto* file_cmd = app.add_subcommand("file", "File operations");
    file_cmd->require_subcommand(1);
    file_cmd->footer("\nEXAMPLES:\n"
                     "  repo file stage file.txt\n"
                     "  repo file unstage file.txt\n"
                     "  repo file restore file.txt\n"
                     "  repo file diff\n"
                     "  repo file clean --dry-run\n"
                     "  repo file clean --force\n");

    // file stage
    auto* file_stage_cmd = file_cmd->add_subcommand("stage", "Stage files for commit");
    std::vector<std::string> stage_paths;
    file_stage_cmd->add_option("paths", stage_paths, "Files to stage")->required();
    file_stage_cmd->footer("\nEXAMPLES:\n"
                           "  repo file stage file.txt\n"
                           "  repo file stage file1.txt file2.txt\n");
    file_stage_cmd->callback([&stage_paths]() { std::exit(cmd_stage(stage_paths)); });

    // file unstage
    auto* file_unstage_cmd = file_cmd->add_subcommand("unstage", "Unstage files");
    std::vector<std::string> unstage_paths;
    file_unstage_cmd->add_option("paths", unstage_paths, "Files to unstage")->required();
    file_unstage_cmd->footer("\nEXAMPLES:\n"
                             "  repo file unstage file.txt\n"
                             "  repo file unstage file1.txt file2.txt\n");
    file_unstage_cmd->callback([&unstage_paths]() { std::exit(cmd_unstage(unstage_paths)); });

    // file restore
    auto* file_restore_cmd = file_cmd->add_subcommand("restore", "Restore files");
    std::vector<std::string> restore_paths;
    bool restore_staged = false;
    file_restore_cmd->add_option("paths", restore_paths, "Files to restore")->required();
    file_restore_cmd->add_flag("--staged,-S", restore_staged, "Restore staged changes");
    file_restore_cmd->footer("\nEXAMPLES:\n"
                             "  repo file restore file.txt\n"
                             "  repo file restore --staged file.txt\n");
    file_restore_cmd->callback([&restore_paths, &restore_staged]() {
        std::exit(cmd_restore(restore_paths, restore_staged));
    });

    // file diff
    auto* file_diff_cmd = file_cmd->add_subcommand("diff", "Show file changes");
    bool file_diff_staged = false;
    bool file_diff_all = false;
    file_diff_cmd->add_flag("--staged,-S", file_diff_staged, "Show staged changes");
    file_diff_cmd->add_flag("--all,-a", file_diff_all, "Show all changes (staged + unstaged)");
    file_diff_cmd->footer("\nEXAMPLES:\n"
                          "  repo file diff\n"
                          "  repo file diff --staged\n"
                          "  repo file diff --all\n");
    file_diff_cmd->callback([&file_diff_staged, &file_diff_all]() {
        std::exit(cmd_diff(file_diff_staged, file_diff_all));
    });

    // file clean
    auto* file_clean_cmd = file_cmd->add_subcommand("clean", "Remove untracked files");
    bool clean_dry_run = false;
    bool clean_directories = false;
    bool clean_force = false;
    bool clean_ignored = false;
    file_clean_cmd->add_flag("--dry-run,-n", clean_dry_run,
                             "Show what would be removed without deleting");
    file_clean_cmd->add_flag("--directories,-d", clean_directories,
                             "Remove untracked directories too");
    file_clean_cmd->add_flag("--force,-f", clean_force,
                             "Actually remove files (required unless --dry-run)");
    file_clean_cmd->add_flag("--ignored,-x", clean_ignored, "Also remove ignored files");
    file_clean_cmd->footer("\nEXAMPLES:\n"
                           "  repo file clean --dry-run\n"
                           "  repo file clean --force\n"
                           "  repo file clean --force --directories\n");
    file_clean_cmd->callback([&clean_dry_run, &clean_directories, &clean_force, &clean_ignored]() {
        std::exit(cmd_clean(clean_dry_run, clean_directories, clean_force, clean_ignored));
    });

    // Remote domain
    auto* remote_cmd = app.add_subcommand("remote", "Remote repository operations");
    remote_cmd->require_subcommand(1);
    remote_cmd->footer("\nEXAMPLES:\n"
                       "  repo remote list\n"
                       "  repo remote show origin\n"
                       "  repo remote add origin https://github.com/user/repo.git\n"
                       "  repo remote remove origin\n");

    // remote list
    auto* remote_list_cmd = remote_cmd->add_subcommand("list", "List remote repositories");
    remote_list_cmd->footer("\nEXAMPLES:\n"
                            "  repo remote list\n");
    remote_list_cmd->callback([]() { std::exit(cmd_remote_list()); });

    // remote add
    auto* remote_add_cmd = remote_cmd->add_subcommand("add", "Add a remote repository");
    std::string remote_name, remote_url;
    remote_add_cmd->add_option("name", remote_name, "Remote name")->required();
    remote_add_cmd->add_option("url", remote_url, "Remote URL")->required();
    remote_add_cmd->footer("\nEXAMPLES:\n"
                           "  repo remote add origin https://github.com/user/repo.git\n");
    remote_add_cmd->callback(
        [&remote_name, &remote_url]() { std::exit(cmd_remote_add(remote_name, remote_url)); });

    // remote remove
    auto* remote_remove_cmd = remote_cmd->add_subcommand("remove", "Remove a remote repository");
    std::string remove_remote_name;
    remote_remove_cmd->add_option("name", remove_remote_name, "Remote name")->required();
    remote_remove_cmd->footer("\nEXAMPLES:\n"
                              "  repo remote remove origin\n");
    remote_remove_cmd->callback(
        [&remove_remote_name]() { std::exit(cmd_remote_remove(remove_remote_name)); });

    // remote show
    auto* remote_show_cmd = remote_cmd->add_subcommand("show", "Show remote details");
    std::string show_remote_name;
    remote_show_cmd->add_option("name", show_remote_name, "Remote name")->required();
    remote_show_cmd->footer("\nEXAMPLES:\n"
                            "  repo remote show origin\n");
    remote_show_cmd->callback(
        [&show_remote_name]() { std::exit(cmd_remote_show(show_remote_name)); });

    // remote fetch
    auto* remote_fetch_cmd = remote_cmd->add_subcommand("fetch", "Fetch from a remote repository");
    std::string fetch_remote_name;
    bool fetch_prune = false;
    bool fetch_no_tags = false;
    remote_fetch_cmd->add_option("remote", fetch_remote_name, "Remote name (default: origin)");
    remote_fetch_cmd->add_flag("--prune,-p", fetch_prune,
                               "Remove remote-tracking refs that no longer exist");
    remote_fetch_cmd->add_flag("--no-tags", fetch_no_tags, "Don't fetch tags");
    remote_fetch_cmd->footer("\nEXAMPLES:\n"
                             "  repo remote fetch\n"
                             "  repo remote fetch origin\n"
                             "  repo remote fetch --prune\n");
    remote_fetch_cmd->callback([&fetch_remote_name, &fetch_prune, &fetch_no_tags]() {
        std::exit(cmd_remote_fetch(fetch_remote_name, fetch_prune, fetch_no_tags));
    });

    // remote push
    auto* remote_push_cmd = remote_cmd->add_subcommand("push", "Push to a remote repository");
    std::string push_remote_name;
    bool push_force = false;
    bool push_set_upstream = false;
    remote_push_cmd->add_option("remote", push_remote_name, "Remote name (default: origin)");
    remote_push_cmd->add_flag("--force,-f", push_force, "Allow non-fast-forward updates");
    remote_push_cmd->add_flag("--set-upstream,-u", push_set_upstream,
                              "Set upstream tracking for current branch");
    remote_push_cmd->footer("\nEXAMPLES:\n"
                            "  repo remote push\n"
                            "  repo remote push origin\n"
                            "  repo remote push --force\n"
                            "  repo remote push --set-upstream\n");
    remote_push_cmd->callback([&push_remote_name, &push_force, &push_set_upstream]() {
        std::exit(cmd_remote_push(push_remote_name, push_force, push_set_upstream));
    });

    // remote pull
    auto* remote_pull_cmd = remote_cmd->add_subcommand("pull", "Pull from a remote repository");
    std::string pull_remote_name;
    bool pull_rebase = false;
    bool pull_prune = false;
    bool pull_no_tags = false;
    remote_pull_cmd->add_option("remote", pull_remote_name, "Remote name (default: origin)");
    remote_pull_cmd->add_flag("--rebase,-r", pull_rebase,
                              "Rebase instead of merge (not yet implemented)");
    remote_pull_cmd->add_flag("--prune,-p", pull_prune,
                              "Remove remote-tracking refs that no longer exist");
    remote_pull_cmd->add_flag("--no-tags", pull_no_tags, "Don't fetch tags");
    remote_pull_cmd->footer("\nEXAMPLES:\n"
                            "  repo remote pull\n"
                            "  repo remote pull origin\n"
                            "  repo remote pull --prune\n");
    remote_pull_cmd->callback([&pull_remote_name, &pull_rebase, &pull_prune, &pull_no_tags]() {
        std::exit(cmd_remote_pull(pull_remote_name, pull_rebase, pull_prune, pull_no_tags));
    });

    // Tag domain
    auto* tag_cmd = app.add_subcommand("tag", "Tag operations");
    tag_cmd->require_subcommand(1);
    tag_cmd->footer("\nEXAMPLES:\n"
                    "  repo tag list\n"
                    "  repo tag show v1.0.0\n"
                    "  repo tag create v1.0.0\n"
                    "  repo tag create v1.0.0 -m \"Release version\"\n"
                    "  repo tag delete v1.0.0\n");

    // tag list
    auto* tag_list_cmd = tag_cmd->add_subcommand("list", "List tags");
    tag_list_cmd->footer("\nEXAMPLES:\n"
                         "  repo tag list\n");
    tag_list_cmd->callback([]() { std::exit(cmd_tag_list()); });

    // tag create
    auto* tag_create_cmd = tag_cmd->add_subcommand("create", "Create a tag");
    std::string create_tag_name;
    std::string create_tag_target;
    std::string create_tag_message;
    bool create_tag_force = false;
    tag_create_cmd->add_option("name", create_tag_name, "Tag name")->required();
    tag_create_cmd->add_option("target", create_tag_target, "Target commit (default: HEAD)");
    tag_create_cmd->add_option("-m,--message", create_tag_message,
                               "Tag message (creates annotated tag)");
    tag_create_cmd->add_flag("-f,--force", create_tag_force, "Replace existing tag");
    tag_create_cmd->footer("\nEXAMPLES:\n"
                           "  repo tag create v1.0.0              # Lightweight tag at HEAD\n"
                           "  repo tag create v1.0.0 abc123       # Lightweight tag at commit\n"
                           "  repo tag create v1.0.0 -m \"Release\" # Annotated tag\n");
    tag_create_cmd->callback(
        [&create_tag_name, &create_tag_target, &create_tag_message, &create_tag_force]() {
            std::exit(cmd_tag_create(create_tag_name, create_tag_target, create_tag_message,
                                     create_tag_force));
        });

    // tag delete
    auto* tag_delete_cmd = tag_cmd->add_subcommand("delete", "Delete a tag");
    std::string delete_tag_name;
    tag_delete_cmd->add_option("name", delete_tag_name, "Tag name")->required();
    tag_delete_cmd->footer("\nEXAMPLES:\n"
                           "  repo tag delete v1.0.0\n");
    tag_delete_cmd->callback([&delete_tag_name]() { std::exit(cmd_tag_delete(delete_tag_name)); });

    // tag show
    auto* tag_show_cmd = tag_cmd->add_subcommand("show", "Show tag details");
    std::string show_tag_name;
    tag_show_cmd->add_option("name", show_tag_name, "Tag name")->required();
    tag_show_cmd->footer("\nEXAMPLES:\n"
                         "  repo tag show v1.0.0\n");
    tag_show_cmd->callback([&show_tag_name]() { std::exit(cmd_tag_show(show_tag_name)); });

    // Top-level shortcuts
    auto* stage_shortcut = app.add_subcommand("stage", "Stage files (shortcut for 'file stage')");
    std::vector<std::string> shortcut_stage_paths;
    stage_shortcut->add_option("paths", shortcut_stage_paths, "Files to stage")->required();
    stage_shortcut->footer("\nEXAMPLES:\n"
                           "  repo stage file.txt\n"
                           "  repo stage file1.txt file2.txt\n");
    stage_shortcut->callback(
        [&shortcut_stage_paths]() { std::exit(cmd_stage(shortcut_stage_paths)); });

    auto* unstage_shortcut =
        app.add_subcommand("unstage", "Unstage files (shortcut for 'file unstage')");
    std::vector<std::string> shortcut_unstage_paths;
    unstage_shortcut->add_option("paths", shortcut_unstage_paths, "Files to unstage")->required();
    unstage_shortcut->footer("\nEXAMPLES:\n"
                             "  repo unstage file.txt\n"
                             "  repo unstage file1.txt file2.txt\n");
    unstage_shortcut->callback(
        [&shortcut_unstage_paths]() { std::exit(cmd_unstage(shortcut_unstage_paths)); });

    auto* restore_shortcut =
        app.add_subcommand("restore", "Restore files (shortcut for 'file restore')");
    std::vector<std::string> shortcut_restore_paths;
    bool shortcut_restore_staged = false;
    restore_shortcut->add_option("paths", shortcut_restore_paths, "Files to restore")->required();
    restore_shortcut->add_flag("--staged,-S", shortcut_restore_staged, "Restore staged changes");
    restore_shortcut->footer("\nEXAMPLES:\n"
                             "  repo restore file.txt\n"
                             "  repo restore --staged file.txt\n");
    restore_shortcut->callback([&shortcut_restore_paths, &shortcut_restore_staged]() {
        std::exit(cmd_restore(shortcut_restore_paths, shortcut_restore_staged));
    });

    auto* diff_shortcut = app.add_subcommand("diff", "Show changes (shortcut for 'file diff')");
    bool shortcut_diff_staged = false;
    bool shortcut_diff_all = false;
    diff_shortcut->add_flag("--staged,-S", shortcut_diff_staged, "Show staged changes");
    diff_shortcut->add_flag("--all,-a", shortcut_diff_all, "Show all changes (staged + unstaged)");
    diff_shortcut->footer("\nEXAMPLES:\n"
                          "  repo diff\n"
                          "  repo diff --staged\n"
                          "  repo diff --all\n");
    diff_shortcut->callback([&shortcut_diff_staged, &shortcut_diff_all]() {
        std::exit(cmd_diff(shortcut_diff_staged, shortcut_diff_all));
    });

    auto* switch_shortcut =
        app.add_subcommand("switch", "Switch branch (shortcut for 'branch switch')");
    std::string shortcut_switch_name;
    switch_shortcut->add_option("name", shortcut_switch_name, "Branch name")->required();
    switch_shortcut->footer("\nEXAMPLES:\n"
                            "  repo switch main\n"
                            "  repo switch feature\n");
    switch_shortcut->callback(
        [&shortcut_switch_name]() { std::exit(cmd_branch_switch(shortcut_switch_name)); });

    auto* merge_shortcut =
        app.add_subcommand("merge", "Merge a branch (shortcut for 'branch merge')");
    std::string shortcut_merge_source;
    std::string shortcut_merge_strategy;
    std::string shortcut_merge_message;
    merge_shortcut->add_option("source", shortcut_merge_source, "Branch or commit to merge")
        ->required();
    merge_shortcut->add_option("--strategy,-s", shortcut_merge_strategy,
                               "Merge strategy: ff, no-ff, ff-only");
    merge_shortcut->add_option("--message,-m", shortcut_merge_message,
                               "Custom merge commit message");
    merge_shortcut->footer("\nEXAMPLES:\n"
                           "  repo merge feature\n"
                           "  repo merge feature --strategy no-ff\n"
                           "  repo merge feature --message \"Merge feature branch\"\n");
    merge_shortcut->callback(
        [&shortcut_merge_source, &shortcut_merge_strategy, &shortcut_merge_message]() {
            std::exit(cmd_branch_merge(shortcut_merge_source, shortcut_merge_strategy,
                                       shortcut_merge_message));
        });

    auto* fetch_shortcut =
        app.add_subcommand("fetch", "Fetch from remote (shortcut for 'remote fetch')");
    std::string shortcut_fetch_remote;
    bool shortcut_fetch_prune = false;
    bool shortcut_fetch_no_tags = false;
    fetch_shortcut->add_option("remote", shortcut_fetch_remote, "Remote name (default: origin)");
    fetch_shortcut->add_flag("--prune,-p", shortcut_fetch_prune,
                             "Remove remote-tracking refs that no longer exist");
    fetch_shortcut->add_flag("--no-tags", shortcut_fetch_no_tags, "Don't fetch tags");
    fetch_shortcut->footer("\nEXAMPLES:\n"
                           "  repo fetch\n"
                           "  repo fetch origin\n"
                           "  repo fetch --prune\n");
    fetch_shortcut->callback(
        [&shortcut_fetch_remote, &shortcut_fetch_prune, &shortcut_fetch_no_tags]() {
            std::exit(cmd_remote_fetch(shortcut_fetch_remote, shortcut_fetch_prune,
                                       shortcut_fetch_no_tags));
        });

    auto* push_shortcut = app.add_subcommand("push", "Push to remote (shortcut for 'remote push')");
    std::string shortcut_push_remote;
    bool shortcut_push_force = false;
    bool shortcut_push_set_upstream = false;
    push_shortcut->add_option("remote", shortcut_push_remote, "Remote name (default: origin)");
    push_shortcut->add_flag("--force,-f", shortcut_push_force, "Allow non-fast-forward updates");
    push_shortcut->add_flag("--set-upstream,-u", shortcut_push_set_upstream,
                            "Set upstream tracking for current branch");
    push_shortcut->footer("\nEXAMPLES:\n"
                          "  repo push\n"
                          "  repo push origin\n"
                          "  repo push --force\n"
                          "  repo push --set-upstream\n");
    push_shortcut->callback(
        [&shortcut_push_remote, &shortcut_push_force, &shortcut_push_set_upstream]() {
            std::exit(cmd_remote_push(shortcut_push_remote, shortcut_push_force,
                                      shortcut_push_set_upstream));
        });

    auto* pull_shortcut =
        app.add_subcommand("pull", "Pull from remote (shortcut for 'remote pull')");
    std::string shortcut_pull_remote;
    bool shortcut_pull_rebase = false;
    bool shortcut_pull_prune = false;
    bool shortcut_pull_no_tags = false;
    pull_shortcut->add_option("remote", shortcut_pull_remote, "Remote name (default: origin)");
    pull_shortcut->add_flag("--rebase,-r", shortcut_pull_rebase,
                            "Rebase instead of merge (not yet implemented)");
    pull_shortcut->add_flag("--prune,-p", shortcut_pull_prune,
                            "Remove remote-tracking refs that no longer exist");
    pull_shortcut->add_flag("--no-tags", shortcut_pull_no_tags, "Don't fetch tags");
    pull_shortcut->footer("\nEXAMPLES:\n"
                          "  repo pull\n"
                          "  repo pull origin\n"
                          "  repo pull --prune\n");
    pull_shortcut->callback([&shortcut_pull_remote, &shortcut_pull_rebase, &shortcut_pull_prune,
                             &shortcut_pull_no_tags]() {
        std::exit(cmd_remote_pull(shortcut_pull_remote, shortcut_pull_rebase, shortcut_pull_prune,
                                  shortcut_pull_no_tags));
    });

    auto* log_shortcut =
        app.add_subcommand("log", "Show commit history (shortcut for 'commit list')");
    int shortcut_log_max_count = 0;
    log_shortcut->add_option("--max-count,-n", shortcut_log_max_count,
                             "Limit number of commits to show");
    log_shortcut->footer("\nEXAMPLES:\n"
                         "  repo log\n"
                         "  repo log --max-count 10\n"
                         "  repo log -n 5\n");
    log_shortcut->callback(
        [&shortcut_log_max_count]() { std::exit(cmd_commit_list(shortcut_log_max_count)); });

    auto* show_shortcut =
        app.add_subcommand("show", "Show commit details (shortcut for 'commit show')");
    std::string shortcut_show_ref = "HEAD";
    show_shortcut->add_option("ref", shortcut_show_ref, "Commit reference (default: HEAD)");
    show_shortcut->footer("\nEXAMPLES:\n"
                          "  repo show\n"
                          "  repo show HEAD\n"
                          "  repo show abc123\n"
                          "  repo show main\n");
    show_shortcut->callback(
        [&shortcut_show_ref]() { std::exit(cmd_commit_show(shortcut_show_ref)); });

    // Init command
    auto* init_cmd = app.add_subcommand("init", "Initialize a new repository");
    std::string init_path;
    bool init_bare = false;
    bool init_interactive = false;
    std::string init_types;
    init_cmd->add_option("path", init_path, "Path to initialize (default: current directory)");
    init_cmd->add_flag("--bare", init_bare, "Create a bare repository");
    init_cmd->add_flag("-i,--interactive", init_interactive,
                       "Interactive mode with .gitignore template selection");
    init_cmd->add_option("--type", init_types,
                         "Project types for .gitignore (comma-separated: cpp,cmake,macos)");
    init_cmd->footer("\nEXAMPLES:\n"
                     "  repo init                              # Simple init, no .gitignore\n"
                     "  repo init -i                           # Interactive template selection\n"
                     "  repo init --type cpp,cmake,macos       # With specific templates\n"
                     "  repo init --type js,node,vscode        # JavaScript project\n"
                     "  repo init my-project --bare\n");
    init_cmd->callback([&init_path, &init_bare, &init_interactive, &init_types]() {
        std::exit(cmd_init(init_path, init_bare, init_interactive, init_types));
    });

    // Auth domain
    auto* auth_cmd = app.add_subcommand("auth", "Authentication management");
    auth_cmd->require_subcommand(1);
    auth_cmd->footer("\nEXAMPLES:\n"
                     "  repo auth login\n"
                     "  repo auth status\n"
                     "  repo auth test https://github.com/user/repo.git\n"
                     "  repo auth clear https://github.com/user/repo.git\n");

    // auth login
    auto* auth_login_cmd = auth_cmd->add_subcommand("login", "Login to GitHub/GitLab using OAuth");
    std::string auth_provider = "github";
    auth_login_cmd->add_option("--provider", auth_provider, "Provider (github or gitlab)")
        ->default_val("github");
    auth_login_cmd->footer("\nEXAMPLES:\n"
                           "  repo auth login\n"
                           "  repo auth login --provider gitlab\n"
                           "\n"
                           "Uses OAuth device flow to authenticate with GitHub or GitLab.\n"
                           "Token will be saved to your credential helper for future use.\n");
    auth_login_cmd->callback([&auth_provider]() { std::exit(cmd_auth_login(auth_provider)); });

    // auth status
    auto* auth_status_cmd = auth_cmd->add_subcommand("status", "Show authentication status");
    auth_status_cmd->footer("\nEXAMPLES:\n"
                            "  repo auth status\n"
                            "\n"
                            "Shows configured authentication methods:\n"
                            "  • Git credential helper configuration\n"
                            "  • SSH keys in ~/.ssh/\n"
                            "  • SSH agent status\n");
    auth_status_cmd->callback([]() { std::exit(cmd_auth_status()); });

    // auth test
    auto* auth_test_cmd = auth_cmd->add_subcommand("test", "Test authentication for a URL");
    std::string auth_test_url;
    auth_test_cmd->add_option("url", auth_test_url, "Remote URL to test")->required();
    auth_test_cmd->footer("\nEXAMPLES:\n"
                          "  repo auth test https://github.com/user/repo.git\n"
                          "  repo auth test git@github.com:user/repo.git\n"
                          "\n"
                          "Checks if authentication is properly configured for the given URL.\n");
    auth_test_cmd->callback([&auth_test_url]() { std::exit(cmd_auth_test(auth_test_url)); });

    // auth clear
    auto* auth_clear_cmd = auth_cmd->add_subcommand("clear", "Clear stored credentials");
    std::string auth_clear_url;
    auth_clear_cmd->add_option("url", auth_clear_url, "URL to clear credentials for")->required();
    auth_clear_cmd->footer("\nEXAMPLES:\n"
                           "  repo auth clear https://github.com/user/repo.git\n"
                           "\n"
                           "Removes stored credentials for the given URL.\n"
                           "Next authentication will prompt for new credentials.\n");
    auth_clear_cmd->callback([&auth_clear_url]() { std::exit(cmd_auth_clear(auth_clear_url)); });

    // Version command
    auto* version_cmd = app.add_subcommand("version", "Show repo version");
    version_cmd->footer("\nEXAMPLES:\n"
                        "  repo version\n");
    version_cmd->callback([]() {
        fmt::print("repo version 0.1.0\n");
        std::exit(0);
    });

    // TUI command
    auto* tui_cmd = app.add_subcommand("tui", "Launch interactive TUI");
    tui_cmd->footer("\nEXAMPLES:\n"
                    "  repo tui\n"
                    "  repo -i\n");
    tui_cmd->callback([]() { std::exit(tui::run()); });

    // Parse and run
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }

    return 0;
}
