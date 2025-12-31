#include <repo/ops/branch.hpp>
#include <repo/ops/commit.hpp>
#include <repo/ops/remote.hpp>
#include <repo/ops/stage.hpp>
#include <repo/ops/stash.hpp>
#include <repo/ops/status.hpp>
#include <repo/ops/switch.hpp>
#include <repo/repository.hpp>
#include <repo/tui/models/command.hpp>

#include <algorithm>
#include <sstream>

namespace repo::tui::models {

// ============================================================================
// Command Parsing
// ============================================================================

auto parse_command(const std::string& input) -> ParsedCommand {
    ParsedCommand cmd;

    std::istringstream stream(input);
    std::string token;

    // Parse main command
    if (stream >> cmd.command) {
        // Parse subcommand (optional)
        if (stream >> token) {
            // Check if it's a flag or subcommand
            if (token[0] == '-') {
                // It's a flag, put it back
                stream.seekg(-static_cast<int>(token.length()) - 1, std::ios_base::cur);
            } else {
                cmd.subcommand = token;
            }
        }
    }

    // Parse remaining args and flags
    while (stream >> token) {
        if (token.starts_with("--")) {
            // Long flag
            std::string flag = token.substr(2);
            std::string value;

            // Check if flag has value (--flag=value or --flag value)
            auto eq_pos = flag.find('=');
            if (eq_pos != std::string::npos) {
                value = flag.substr(eq_pos + 1);
                flag = flag.substr(0, eq_pos);
            } else if (stream >> value && !value.starts_with("-")) {
                // Value is next token
            } else {
                value = "true"; // Boolean flag
            }

            cmd.flags.emplace_back(flag, value);
        } else if (token.starts_with("-") && token.length() > 1) {
            // Short flag
            std::string flag = token.substr(1);
            std::string value;

            if (stream >> value && !value.starts_with("-")) {
                cmd.flags.emplace_back(flag, value);
            } else {
                cmd.flags.emplace_back(flag, "true");
            }
        } else {
            // Positional argument
            cmd.args.push_back(token);
        }
    }

    return cmd;
}

// ============================================================================
// Command Suggestions
// ============================================================================

auto get_command_suggestions(const std::string& input) -> std::vector<std::string> {
    // List of all available commands
    static const std::vector<std::string> all_commands = {
        "status",      "stage",         "unstage",       "commit",     "branch create",
        "branch list", "branch delete", "branch switch", "switch",     "stash create",
        "stash list",  "stash apply",   "stash pop",     "stash drop", "remote add",
        "remote list", "remote remove", "fetch",         "push",       "pull",
        "help",        "quit"};

    std::vector<std::string> suggestions;

    for (const auto& cmd : all_commands) {
        if (cmd.starts_with(input)) {
            suggestions.push_back(cmd);
        }
    }

    return suggestions;
}

// ============================================================================
// Command Execution
// ============================================================================

auto execute_command(const ParsedCommand& cmd, std::string repo_path) -> tea::CmdBatch {
    return tea::async([cmd, repo_path = std::move(repo_path)]() -> std::optional<tea::Msg> {
        try {
            auto repo = Repository::open(repo_path);
            if (!repo) {
                return tea::CommandExecutedMsg{.command = cmd.command,
                                               .exit_code = 1,
                                               .output = "Error: " + repo.error().message};
            }

            // Route to appropriate command handler
            if (cmd.command == "status") {
                auto result = ops::status(*repo);
                if (!result) {
                    return tea::CommandExecutedMsg{.command = "status",
                                                   .exit_code = 1,
                                                   .output = "Error: " + result.error().message};
                }

                std::string output;
                output += "Staged: " + std::to_string(result->staged().size()) + " files\n";
                output += "Unstaged: " + std::to_string(result->unstaged().size()) + " files\n";
                output += "Untracked: " + std::to_string(result->untracked().size()) + " files";

                return tea::CommandExecutedMsg{
                    .command = "status", .exit_code = 0, .output = output};
            }

            else if (cmd.command == "stage") {
                if (cmd.args.empty()) {
                    return tea::CommandExecutedMsg{
                        .command = "stage", .exit_code = 1, .output = "Error: No files specified"};
                }

                std::vector<std::filesystem::path> paths;
                for (const auto& arg : cmd.args) {
                    paths.push_back(arg);
                }

                auto result = ops::stage(*repo, ops::StageParams{.paths = paths});
                if (!result) {
                    return tea::CommandExecutedMsg{.command = "stage",
                                                   .exit_code = 1,
                                                   .output = "Error: " + result.error().message};
                }

                return tea::CommandExecutedMsg{.command = "stage",
                                               .exit_code = 0,
                                               .output = "Staged " + std::to_string(paths.size()) +
                                                         " file(s)"};
            }

            else if (cmd.command == "unstage") {
                if (cmd.args.empty()) {
                    return tea::CommandExecutedMsg{.command = "unstage",
                                                   .exit_code = 1,
                                                   .output = "Error: No files specified"};
                }

                std::vector<std::filesystem::path> paths;
                for (const auto& arg : cmd.args) {
                    paths.push_back(arg);
                }

                auto result = ops::unstage(*repo, ops::StageParams{.paths = paths});
                if (!result) {
                    return tea::CommandExecutedMsg{.command = "unstage",
                                                   .exit_code = 1,
                                                   .output = "Error: " + result.error().message};
                }

                return tea::CommandExecutedMsg{.command = "unstage",
                                               .exit_code = 0,
                                               .output = "Unstaged " +
                                                         std::to_string(paths.size()) + " file(s)"};
            }

            else if (cmd.command == "commit") {
                // Find -m or --message flag
                std::string message;
                for (const auto& [flag, value] : cmd.flags) {
                    if (flag == "m" || flag == "message") {
                        message = value;
                        break;
                    }
                }

                if (message.empty()) {
                    return tea::CommandExecutedMsg{
                        .command = "commit",
                        .exit_code = 1,
                        .output = "Error: No commit message provided (use -m \"message\")"};
                }

                auto result = ops::commit(*repo, ops::CommitParams{.message = message});
                if (!result) {
                    return tea::CommandExecutedMsg{.command = "commit",
                                                   .exit_code = 1,
                                                   .output = "Error: " + result.error().message};
                }

                return tea::CommandExecutedMsg{
                    .command = "commit",
                    .exit_code = 0,
                    .output = "Created commit: " + result->commit.id.to_string().substr(0, 7)};
            }

            else if (cmd.command == "branch") {
                if (cmd.subcommand == "create") {
                    if (cmd.args.empty()) {
                        return tea::CommandExecutedMsg{.command = "branch create",
                                                       .exit_code = 1,
                                                       .output = "Error: No branch name specified"};
                    }

                    // Get HEAD as target
                    auto head = repo->head();
                    if (!head || !std::holds_alternative<domain::ObjectId>(head->target)) {
                        return tea::CommandExecutedMsg{.command = "branch create",
                                                       .exit_code = 1,
                                                       .output = "Error: Could not get HEAD"};
                    }

                    auto target_id = std::get<domain::ObjectId>(head->target);
                    auto result = ops::create_branch(
                        *repo, ops::CreateBranchParams{
                                   .name = cmd.args[0], .target = target_id, .force = false});

                    if (!result) {
                        return tea::CommandExecutedMsg{.command = "branch create",
                                                       .exit_code = 1,
                                                       .output =
                                                           "Error: " + result.error().message};
                    }

                    return tea::CommandExecutedMsg{.command = "branch create",
                                                   .exit_code = 0,
                                                   .output = "Created branch: " + cmd.args[0]};
                } else if (cmd.subcommand == "list" || cmd.subcommand.empty()) {
                    auto result = ops::list_branches(*repo);
                    if (!result) {
                        return tea::CommandExecutedMsg{.command = "branch list",
                                                       .exit_code = 1,
                                                       .output =
                                                           "Error: " + result.error().message};
                    }

                    std::string output;
                    for (const auto& branch : result->branches) {
                        if (branch.is_head) {
                            output += "* ";
                        } else {
                            output += "  ";
                        }
                        output += branch.name + "\n";
                    }

                    return tea::CommandExecutedMsg{
                        .command = "branch list", .exit_code = 0, .output = output};
                } else if (cmd.subcommand == "delete") {
                    if (cmd.args.empty()) {
                        return tea::CommandExecutedMsg{.command = "branch delete",
                                                       .exit_code = 1,
                                                       .output = "Error: No branch name specified"};
                    }

                    auto result =
                        ops::delete_branch(*repo, ops::DeleteBranchParams{.name = cmd.args[0]});

                    if (!result) {
                        return tea::CommandExecutedMsg{.command = "branch delete",
                                                       .exit_code = 1,
                                                       .output =
                                                           "Error: " + result.error().message};
                    }

                    return tea::CommandExecutedMsg{.command = "branch delete",
                                                   .exit_code = 0,
                                                   .output = "Deleted branch: " + cmd.args[0]};
                }
            }

            else if (cmd.command == "switch") {
                if (cmd.args.empty()) {
                    return tea::CommandExecutedMsg{.command = "switch",
                                                   .exit_code = 1,
                                                   .output = "Error: No branch name specified"};
                }

                auto result =
                    ops::switch_branch(*repo, ops::SwitchParams{.branch_name = cmd.args[0]});

                if (!result) {
                    return tea::CommandExecutedMsg{.command = "switch",
                                                   .exit_code = 1,
                                                   .output = "Error: " + result.error().message};
                }

                return tea::CommandExecutedMsg{.command = "switch",
                                               .exit_code = 0,
                                               .output = "Switched to branch: " + cmd.args[0]};
            }

            else if (cmd.command == "help") {
                std::string output = "Available commands:\n";
                output += "  status - Show repository status\n";
                output += "  stage <files...> - Stage files\n";
                output += "  unstage <files...> - Unstage files\n";
                output += "  commit -m \"message\" - Create commit\n";
                output += "  branch [list|create|delete] <name> - Branch operations\n";
                output += "  switch <branch> - Switch to branch\n";
                output += "  help - Show this help\n";
                output += "  quit - Exit TUI";

                return tea::CommandExecutedMsg{.command = "help", .exit_code = 0, .output = output};
            }

            else {
                return tea::CommandExecutedMsg{.command = cmd.command,
                                               .exit_code = 1,
                                               .output = "Error: Unknown command '" + cmd.command +
                                                         "'. Type 'help' for available commands."};
            }

            // Default fallthrough
            return tea::CommandExecutedMsg{
                .command = cmd.command, .exit_code = 1, .output = "Error: Command not implemented"};

        } catch (const std::exception& e) {
            return tea::CommandExecutedMsg{.command = cmd.command,
                                           .exit_code = 1,
                                           .output = std::string("Exception: ") + e.what()};
        }
    });
}

// ============================================================================
// Update Function
// ============================================================================

auto update_command(CommandModel model, tea::Msg msg) -> std::pair<CommandModel, tea::CmdBatch> {
    return std::visit(
        [&model](auto&& m) -> std::pair<CommandModel, tea::CmdBatch> {
            using T = std::decay_t<decltype(m)>;

            if constexpr (std::is_same_v<T, tea::KeyMsg>) {
                // Only process keys if in input mode
                if (model.mode != CommandMode::Input) {
                    return {std::move(model), tea::none()};
                }

                // Escape - cancel command mode
                if (m.type == tea::KeyMsg::Type::Escape) {
                    return {deactivate_command_mode(std::move(model)), tea::none()};
                }

                // Enter - execute command
                if (m.type == tea::KeyMsg::Type::Enter) {
                    if (!model.input_text.empty()) {
                        // Add to history
                        model.history.push_front(model.input_text);
                        if (model.history.size() > model.max_history) {
                            model.history.pop_back();
                        }

                        // Parse and execute
                        auto parsed = parse_command(model.input_text);
                        model.mode = CommandMode::Executing;
                        model.is_executing = true;

                        return {std::move(model), execute_command(parsed, model.repo_path)};
                    }
                    return {std::move(model), tea::none()};
                }

                // Backspace
                if (m.type == tea::KeyMsg::Type::Backspace) {
                    if (model.input_cursor_pos > 0 && !model.input_text.empty()) {
                        model.input_text.erase(model.input_cursor_pos - 1, 1);
                        model.input_cursor_pos--;
                    }
                    return {std::move(model), tea::none()};
                }

                // Delete
                if (m.type == tea::KeyMsg::Type::Delete) {
                    if (model.input_cursor_pos < model.input_text.size()) {
                        model.input_text.erase(model.input_cursor_pos, 1);
                    }
                    return {std::move(model), tea::none()};
                }

                // Arrow keys
                if (m.type == tea::KeyMsg::Type::ArrowLeft) {
                    if (model.input_cursor_pos > 0) {
                        model.input_cursor_pos--;
                    }
                    return {std::move(model), tea::none()};
                }

                if (m.type == tea::KeyMsg::Type::ArrowRight) {
                    if (model.input_cursor_pos < model.input_text.size()) {
                        model.input_cursor_pos++;
                    }
                    return {std::move(model), tea::none()};
                }

                // History navigation
                if (m.type == tea::KeyMsg::Type::ArrowUp) {
                    if (model.history_index < model.history.size()) {
                        model.input_text = model.history[model.history_index];
                        model.input_cursor_pos = model.input_text.size();
                        model.history_index++;
                    }
                    return {std::move(model), tea::none()};
                }

                if (m.type == tea::KeyMsg::Type::ArrowDown) {
                    if (model.history_index > 0) {
                        model.history_index--;
                        if (model.history_index == 0) {
                            model.input_text.clear();
                            model.input_cursor_pos = 0;
                        } else {
                            model.input_text = model.history[model.history_index - 1];
                            model.input_cursor_pos = model.input_text.size();
                        }
                    }
                    return {std::move(model), tea::none()};
                }

                if (m.type == tea::KeyMsg::Type::Home) {
                    model.input_cursor_pos = 0;
                    return {std::move(model), tea::none()};
                }

                if (m.type == tea::KeyMsg::Type::End) {
                    model.input_cursor_pos = model.input_text.size();
                    return {std::move(model), tea::none()};
                }

                // Regular character input
                if (m.type == tea::KeyMsg::Type::Character) {
                    model.input_text.insert(model.input_cursor_pos, 1, m.character);
                    model.input_cursor_pos++;

                    // Update suggestions
                    model.suggestions = get_command_suggestions(model.input_text);
                    model.selected_suggestion = 0;

                    return {std::move(model), tea::none()};
                }
            }

            else if constexpr (std::is_same_v<T, tea::CommandExecutedMsg>) {
                model.is_executing = false;
                model.result_message = m.output;

                if (m.exit_code == 0) {
                    // Success - deactivate command mode
                    return {deactivate_command_mode(std::move(model)), tea::none()};
                } else {
                    // Error - stay in command mode to show error
                    model.mode = CommandMode::Input;
                    return {std::move(model), tea::none()};
                }
            }

            return {std::move(model), tea::none()};
        },
        msg);
}

// ============================================================================
// Initialization
// ============================================================================

auto init_command(std::string repo_path) -> std::pair<CommandModel, tea::CmdBatch> {
    CommandModel model;
    model.repo_path = repo_path;
    model.mode = CommandMode::Inactive;

    return {std::move(model), tea::none()};
}

// ============================================================================
// Helpers
// ============================================================================

auto activate_command_mode(CommandModel model) -> CommandModel {
    model.mode = CommandMode::Input;
    model.input_text.clear();
    model.input_cursor_pos = 0;
    model.history_index = 0;
    model.suggestions.clear();
    model.result_message = std::nullopt;
    model.error = std::nullopt;

    return model;
}

auto deactivate_command_mode(CommandModel model) -> CommandModel {
    model.mode = CommandMode::Inactive;
    model.input_text.clear();
    model.input_cursor_pos = 0;
    model.suggestions.clear();

    return model;
}

} // namespace repo::tui::models
