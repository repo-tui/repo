#include <repo/ops/branch.hpp>
#include <repo/ops/merge.hpp>
#include <repo/ops/switch.hpp>
#include <repo/repository.hpp>
#include <repo/tui/models/branch.hpp>

#include <algorithm>

namespace repo::tui::models {

using namespace tea;

// Forward declarations
static auto handle_normal_input(BranchModel model, const KeyMsg& key)
    -> std::pair<BranchModel, CmdBatch>;
static auto handle_create_input(BranchModel model, const KeyMsg& key)
    -> std::pair<BranchModel, CmdBatch>;
static auto handle_rename_input(BranchModel model, const KeyMsg& key)
    -> std::pair<BranchModel, CmdBatch>;
static auto handle_delete_confirm(BranchModel model, const KeyMsg& key)
    -> std::pair<BranchModel, CmdBatch>;
static auto handle_merge_confirm(BranchModel model, const KeyMsg& key)
    -> std::pair<BranchModel, CmdBatch>;
static auto handle_help_input(BranchModel model, const KeyMsg& key)
    -> std::pair<BranchModel, CmdBatch>;

// Initialize the branch model
auto init_branch(std::string repo_path) -> std::pair<BranchModel, CmdBatch> {
    BranchModel model;
    model.repo_path = std::move(repo_path);
    model.is_loading = true;

    // Load branches immediately
    auto cmds = cmd_load_branches(model.repo_path);

    return {std::move(model), std::move(cmds)};
}

// Update function - processes messages and returns new model + commands
auto update_branch(BranchModel model, Msg msg) -> std::pair<BranchModel, CmdBatch> {
    // Handle different message types
    return std::visit(
        [&model](auto&& m) -> std::pair<BranchModel, CmdBatch> {
            using T = std::decay_t<decltype(m)>;

            // Keyboard input
            if constexpr (std::is_same_v<T, KeyMsg>) {
                switch (model.mode) {
                    case BranchMode::CreateInput:
                        return handle_create_input(std::move(model), m);
                    case BranchMode::RenameInput:
                        return handle_rename_input(std::move(model), m);
                    case BranchMode::DeleteConfirm:
                        return handle_delete_confirm(std::move(model), m);
                    case BranchMode::MergeConfirm:
                        return handle_merge_confirm(std::move(model), m);
                    case BranchMode::Help:
                        return handle_help_input(std::move(model), m);
                    default:
                        return handle_normal_input(std::move(model), m);
                }
            }

            // Branches loaded
            else if constexpr (std::is_same_v<T, BranchesLoadedMsg>) {
                model.branches = std::move(m.branches);
                model.is_loading = false;
                model.error = std::nullopt;

                // Determine current branch
                for (const auto& branch : model.branches) {
                    if (branch.is_head) {
                        model.current_branch = branch.name;
                        break;
                    }
                }

                apply_branch_filter(model);
                return {std::move(model), none()};
            }

            // Branch created
            else if constexpr (std::is_same_v<T, BranchCreatedMsg>) {
                model.notification = "Created branch: " + m.branch.name;
                model.mode = BranchMode::Normal;
                model.input_text.clear();
                // Reload branches to reflect changes
                return {std::move(model), cmd_load_branches(model.repo_path)};
            }

            // Branch deleted
            else if constexpr (std::is_same_v<T, BranchDeletedMsg>) {
                model.notification = "Deleted branch: " + m.name;
                model.mode = BranchMode::Normal;
                model.pending_delete_branch = std::nullopt;
                // Reload branches
                return {std::move(model), cmd_load_branches(model.repo_path)};
            }

            // Branch switched
            else if constexpr (std::is_same_v<T, BranchSwitchedMsg>) {
                model.notification = "Switched to: " + m.name;
                model.current_branch = m.name;
                // Reload branches to update is_head flags
                return {std::move(model), cmd_load_branches(model.repo_path)};
            }

            // Branch error
            else if constexpr (std::is_same_v<T, BranchErrorMsg>) {
                model.is_loading = false;
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
static auto handle_normal_input(BranchModel model, const KeyMsg& key)
    -> std::pair<BranchModel, CmdBatch> {
    // Navigation
    if (key.type == KeyMsg::Type::ArrowDown ||
        (key.type == KeyMsg::Type::Character && key.character == 'j')) {
        if (!model.filtered_branches.empty() &&
            model.selected_index < model.filtered_branches.size() - 1) {
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
        if (!model.filtered_branches.empty()) {
            model.selected_index = model.filtered_branches.size() - 1;
        }
        return {std::move(model), none()};
    }

    // Switch to branch (Enter or s)
    if (key.type == KeyMsg::Type::Enter ||
        (key.type == KeyMsg::Type::Character && key.character == 's')) {
        auto branch = selected_branch(model);
        if (branch && !branch->is_head) {
            return {std::move(model), cmd_switch_to_branch(model.repo_path, branch->name)};
        }
        return {std::move(model), none()};
    }

    // Create new branch (c)
    if (key.type == KeyMsg::Type::Character && key.character == 'c') {
        model.mode = BranchMode::CreateInput;
        model.input_text.clear();
        model.input_cursor_pos = 0;
        return {std::move(model), none()};
    }

    // Delete branch (d)
    if (key.type == KeyMsg::Type::Character && key.character == 'd') {
        auto branch = selected_branch(model);
        if (branch && !branch->is_head) {
            model.mode = BranchMode::DeleteConfirm;
            model.pending_delete_branch = *branch;
        } else if (branch && branch->is_head) {
            model.notification = "Cannot delete current branch";
        }
        return {std::move(model), none()};
    }

    // Rename branch (r)
    if (key.type == KeyMsg::Type::Character && key.character == 'r') {
        auto branch = selected_branch(model);
        if (branch) {
            model.mode = BranchMode::RenameInput;
            model.input_text = branch->name;
            model.input_cursor_pos = branch->name.size();
            model.pending_delete_branch = *branch; // Store the branch being renamed
        }
        return {std::move(model), none()};
    }

    // Merge branch (m)
    if (key.type == KeyMsg::Type::Character && key.character == 'm') {
        auto branch = selected_branch(model);
        if (branch && !branch->is_head) {
            model.mode = BranchMode::MergeConfirm;
            model.pending_merge_branch = *branch;
        } else if (branch && branch->is_head) {
            model.notification = "Cannot merge current branch into itself";
        }
        return {std::move(model), none()};
    }

    // Toggle filter (f)
    if (key.type == KeyMsg::Type::Character && key.character == 'f') {
        switch (model.filter) {
            case BranchFilter::All:
                model.filter = BranchFilter::Local;
                break;
            case BranchFilter::Local:
                model.filter = BranchFilter::Remote;
                break;
            case BranchFilter::Remote:
                model.filter = BranchFilter::All;
                break;
        }
        apply_branch_filter(model);
        model.selected_index = 0;
        return {std::move(model), none()};
    }

    // Refresh (R - capital)
    if (key.type == KeyMsg::Type::Character && key.character == 'R') {
        model.is_loading = true;
        return {std::move(model), cmd_load_branches(model.repo_path)};
    }

    // Help (?)
    if (key.type == KeyMsg::Type::Character && key.character == '?') {
        model.mode = BranchMode::Help;
        return {std::move(model), none()};
    }

    return {std::move(model), none()};
}

// Helper: Handle create input mode
static auto handle_create_input(BranchModel model, const KeyMsg& key)
    -> std::pair<BranchModel, CmdBatch> {
    // Escape - cancel
    if (key.type == KeyMsg::Type::Escape) {
        model.mode = BranchMode::Normal;
        model.input_text.clear();
        return {std::move(model), none()};
    }

    // Enter - create branch
    if (key.type == KeyMsg::Type::Enter) {
        if (!model.input_text.empty()) {
            auto branch_name = model.input_text;
            // Ask if user wants to switch to new branch (default: yes)
            return {std::move(model),
                    cmd_create_branch(model.repo_path, std::move(branch_name), true)};
        }
        return {std::move(model), none()};
    }

    // Character input
    if (key.type == KeyMsg::Type::Character) {
        model.input_text.insert(model.input_cursor_pos, 1, key.character);
        model.input_cursor_pos++;
        return {std::move(model), none()};
    }

    // Backspace
    if (key.type == KeyMsg::Type::Backspace && model.input_cursor_pos > 0) {
        model.input_text.erase(model.input_cursor_pos - 1, 1);
        model.input_cursor_pos--;
        return {std::move(model), none()};
    }

    // Delete
    if (key.type == KeyMsg::Type::Delete && model.input_cursor_pos < model.input_text.size()) {
        model.input_text.erase(model.input_cursor_pos, 1);
        return {std::move(model), none()};
    }

    // Arrow left/right for cursor movement
    if (key.type == KeyMsg::Type::ArrowLeft && model.input_cursor_pos > 0) {
        model.input_cursor_pos--;
        return {std::move(model), none()};
    }

    if (key.type == KeyMsg::Type::ArrowRight && model.input_cursor_pos < model.input_text.size()) {
        model.input_cursor_pos++;
        return {std::move(model), none()};
    }

    return {std::move(model), none()};
}

// Helper: Handle rename input mode
static auto handle_rename_input(BranchModel model, const KeyMsg& key)
    -> std::pair<BranchModel, CmdBatch> {
    // Escape - cancel
    if (key.type == KeyMsg::Type::Escape) {
        model.mode = BranchMode::Normal;
        model.input_text.clear();
        model.pending_delete_branch = std::nullopt;
        return {std::move(model), none()};
    }

    // Enter - rename branch
    if (key.type == KeyMsg::Type::Enter) {
        if (!model.input_text.empty() && model.pending_delete_branch) {
            auto old_name = model.pending_delete_branch->name;
            auto new_name = model.input_text;
            return {std::move(model),
                    cmd_rename_branch(model.repo_path, std::move(old_name), std::move(new_name))};
        }
        return {std::move(model), none()};
    }

    // Character input (same as create)
    if (key.type == KeyMsg::Type::Character) {
        model.input_text.insert(model.input_cursor_pos, 1, key.character);
        model.input_cursor_pos++;
        return {std::move(model), none()};
    }

    // Backspace
    if (key.type == KeyMsg::Type::Backspace && model.input_cursor_pos > 0) {
        model.input_text.erase(model.input_cursor_pos - 1, 1);
        model.input_cursor_pos--;
        return {std::move(model), none()};
    }

    // Delete
    if (key.type == KeyMsg::Type::Delete && model.input_cursor_pos < model.input_text.size()) {
        model.input_text.erase(model.input_cursor_pos, 1);
        return {std::move(model), none()};
    }

    // Arrow left/right
    if (key.type == KeyMsg::Type::ArrowLeft && model.input_cursor_pos > 0) {
        model.input_cursor_pos--;
        return {std::move(model), none()};
    }

    if (key.type == KeyMsg::Type::ArrowRight && model.input_cursor_pos < model.input_text.size()) {
        model.input_cursor_pos++;
        return {std::move(model), none()};
    }

    return {std::move(model), none()};
}

// Helper: Handle delete confirmation
static auto handle_delete_confirm(BranchModel model, const KeyMsg& key)
    -> std::pair<BranchModel, CmdBatch> {
    // Y/y - confirm delete
    if (key.type == KeyMsg::Type::Character && (key.character == 'y' || key.character == 'Y')) {
        if (model.pending_delete_branch) {
            auto branch_name = model.pending_delete_branch->name;
            return {std::move(model),
                    cmd_delete_branch(model.repo_path, std::move(branch_name), false)};
        }
    }

    // N/n or Escape - cancel
    if (key.type == KeyMsg::Type::Character && (key.character == 'n' || key.character == 'N')) {
        model.mode = BranchMode::Normal;
        model.pending_delete_branch = std::nullopt;
        return {std::move(model), none()};
    }

    if (key.type == KeyMsg::Type::Escape) {
        model.mode = BranchMode::Normal;
        model.pending_delete_branch = std::nullopt;
        return {std::move(model), none()};
    }

    return {std::move(model), none()};
}

// Helper: Handle merge confirmation
static auto handle_merge_confirm(BranchModel model, const KeyMsg& key)
    -> std::pair<BranchModel, CmdBatch> {
    // Y/y - confirm merge
    if (key.type == KeyMsg::Type::Character && (key.character == 'y' || key.character == 'Y')) {
        if (model.pending_merge_branch) {
            auto branch_name = model.pending_merge_branch->name;
            return {std::move(model), cmd_merge_branch(model.repo_path, std::move(branch_name))};
        }
    }

    // N/n or Escape - cancel
    if (key.type == KeyMsg::Type::Character && (key.character == 'n' || key.character == 'N')) {
        model.mode = BranchMode::Normal;
        model.pending_merge_branch = std::nullopt;
        return {std::move(model), none()};
    }

    if (key.type == KeyMsg::Type::Escape) {
        model.mode = BranchMode::Normal;
        model.pending_merge_branch = std::nullopt;
        return {std::move(model), none()};
    }

    return {std::move(model), none()};
}

// Helper: Handle help mode
static auto handle_help_input(BranchModel model, const KeyMsg& /* key */)
    -> std::pair<BranchModel, CmdBatch> {
    // Any key exits help
    model.mode = BranchMode::Normal;
    return {std::move(model), none()};
}

// Helper: Apply current filter to branches
auto apply_branch_filter(BranchModel& model) -> void {
    model.filtered_branches.clear();

    for (const auto& branch : model.branches) {
        bool include = false;

        switch (model.filter) {
            case BranchFilter::All:
                include = true;
                break;

            case BranchFilter::Local:
                include = !branch.is_remote;
                break;

            case BranchFilter::Remote:
                include = branch.is_remote;
                break;
        }

        if (include) {
            model.filtered_branches.push_back(branch);
        }
    }

    // Adjust selection if out of bounds
    if (!model.filtered_branches.empty() &&
        model.selected_index >= model.filtered_branches.size()) {
        model.selected_index = model.filtered_branches.size() - 1;
    }
}

// Helper: Get currently selected branch
auto selected_branch(const BranchModel& model) -> std::optional<domain::Branch> {
    if (model.selected_index < model.filtered_branches.size()) {
        return model.filtered_branches[model.selected_index];
    }
    return std::nullopt;
}

// Commands for async operations

auto cmd_load_branches(std::string repo_path) -> CmdBatch {
    return async([repo_path = std::move(repo_path)]() -> std::optional<Msg> {
        try {
            auto repo = Repository::open(repo_path);
            if (!repo) {
                return BranchErrorMsg{std::move(repo.error())};
            }

            auto result = ops::list_branches(*repo);
            if (!result) {
                return BranchErrorMsg{std::move(result.error())};
            }

            return BranchesLoadedMsg{std::move(result->branches)};

        } catch (const std::exception& e) {
            return BranchErrorMsg{
                make_error(Error::Code::Unknown, "Failed to load branches", e.what())};
        }
    });
}

auto cmd_switch_to_branch(std::string repo_path, std::string branch_name) -> CmdBatch {
    return async([repo_path = std::move(repo_path),
                  branch_name = std::move(branch_name)]() -> std::optional<Msg> {
        try {
            auto repo = Repository::open(repo_path);
            if (!repo) {
                return BranchErrorMsg{std::move(repo.error())};
            }

            auto result = ops::switch_branch(*repo, ops::SwitchParams{.branch_name = branch_name});

            if (!result) {
                return BranchErrorMsg{std::move(result.error())};
            }

            return BranchSwitchedMsg{branch_name};

        } catch (const std::exception& e) {
            return BranchErrorMsg{
                make_error(Error::Code::Unknown, "Failed to switch branch", e.what())};
        }
    });
}

auto cmd_create_branch(std::string repo_path, std::string branch_name, bool switch_to) -> CmdBatch {
    return async([repo_path = std::move(repo_path), branch_name = std::move(branch_name),
                  switch_to]() -> std::optional<Msg> {
        try {
            auto repo = Repository::open(repo_path);
            if (!repo) {
                return BranchErrorMsg{std::move(repo.error())};
            }

            // Get current HEAD as target
            auto head = repo->backend().get_head(repo->repo_handle());
            if (!head) {
                return BranchErrorMsg{std::move(head.error())};
            }

            // Extract ObjectId from variant
            if (!std::holds_alternative<domain::ObjectId>(head->target)) {
                return BranchErrorMsg{
                    make_error(Error::Code::Unknown, "HEAD is not a direct reference")};
            }

            auto target_id = std::get<domain::ObjectId>(head->target);

            auto result = ops::create_branch(
                *repo,
                ops::CreateBranchParams{.name = branch_name, .target = target_id, .force = false});

            if (!result) {
                return BranchErrorMsg{std::move(result.error())};
            }

            // Switch to new branch if requested
            if (switch_to) {
                auto switch_result =
                    ops::switch_branch(*repo, ops::SwitchParams{.branch_name = branch_name});

                if (!switch_result) {
                    return BranchErrorMsg{std::move(switch_result.error())};
                }

                return BranchSwitchedMsg{branch_name};
            }

            return BranchCreatedMsg{result->branch};

        } catch (const std::exception& e) {
            return BranchErrorMsg{
                make_error(Error::Code::Unknown, "Failed to create branch", e.what())};
        }
    });
}

auto cmd_delete_branch(std::string repo_path, std::string branch_name, bool /* force */)
    -> CmdBatch {
    return async([repo_path = std::move(repo_path),
                  branch_name = std::move(branch_name)]() -> std::optional<Msg> {
        try {
            auto repo = Repository::open(repo_path);
            if (!repo) {
                return BranchErrorMsg{std::move(repo.error())};
            }

            auto result = ops::delete_branch(*repo, ops::DeleteBranchParams{.name = branch_name});

            if (!result) {
                return BranchErrorMsg{std::move(result.error())};
            }

            return BranchDeletedMsg{branch_name};

        } catch (const std::exception& e) {
            return BranchErrorMsg{
                make_error(Error::Code::Unknown, "Failed to delete branch", e.what())};
        }
    });
}

auto cmd_rename_branch(std::string repo_path, std::string old_name, std::string new_name)
    -> CmdBatch {
    return async([repo_path = std::move(repo_path), old_name = std::move(old_name),
                  new_name = std::move(new_name)]() -> std::optional<Msg> {
        try {
            auto repo = Repository::open(repo_path);
            if (!repo) {
                return BranchErrorMsg{std::move(repo.error())};
            }

            auto result = ops::rename_branch(*repo, ops::RenameBranchParams{.old_name = old_name,
                                                                            .new_name = new_name,
                                                                            .force = false});

            if (!result) {
                return BranchErrorMsg{std::move(result.error())};
            }

            return BranchCreatedMsg{result->branch};

        } catch (const std::exception& e) {
            return BranchErrorMsg{
                make_error(Error::Code::Unknown, "Failed to rename branch", e.what())};
        }
    });
}

auto cmd_merge_branch(std::string repo_path, std::string branch_name) -> CmdBatch {
    return async([repo_path = std::move(repo_path),
                  branch_name = std::move(branch_name)]() -> std::optional<Msg> {
        try {
            auto repo = Repository::open(repo_path);
            if (!repo) {
                return BranchErrorMsg{std::move(repo.error())};
            }

            auto result = ops::merge(
                *repo, ops::MergeParams{.source = branch_name,
                                        .strategy = ops::MergeParams::Strategy::FastForward,
                                        .commit = true});

            if (!result) {
                return BranchErrorMsg{std::move(result.error())};
            }

            return NotificationMsg{"Merged branch: " + branch_name,
                                   NotificationMsg::Level::Success};

        } catch (const std::exception& e) {
            return BranchErrorMsg{
                make_error(Error::Code::Unknown, "Failed to merge branch", e.what())};
        }
    });
}

} // namespace repo::tui::models
