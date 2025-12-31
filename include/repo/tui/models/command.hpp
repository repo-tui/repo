#pragma once

#include <repo/error.hpp>
#include <repo/tui/tea/cmd.hpp>
#include <repo/tui/tea/msg.hpp>

#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace repo::tui::models {

// Command mode state
enum class CommandMode {
    Inactive,  // Not in command mode
    Input,     // Entering command
    Executing, // Command is executing
};

// Parsed command structure
struct ParsedCommand {
    std::string command;           // Main command (e.g., "commit", "branch")
    std::string subcommand;        // Subcommand (e.g., "create", "list")
    std::vector<std::string> args; // Positional arguments
    std::vector<std::pair<std::string, std::string>> flags; // Flag arguments (--flag value)
};

// Command mode model
struct CommandModel {
    // Mode state
    CommandMode mode = CommandMode::Inactive;

    // Input state
    std::string input_text;
    size_t input_cursor_pos = 0;

    // Command history
    std::deque<std::string> history; // Previous commands
    size_t history_index = 0;        // Current position in history
    size_t max_history = 100;        // Maximum history size

    // Command suggestions (for autocomplete)
    std::vector<std::string> suggestions;
    size_t selected_suggestion = 0;

    // Execution state
    bool is_executing = false;
    std::optional<Error> error;

    // Result display
    std::optional<std::string> result_message;

    // Repository path
    std::string repo_path;
};

// Parse command string into structured command
auto parse_command(const std::string& input) -> ParsedCommand;

// Get command suggestions based on current input
auto get_command_suggestions(const std::string& input) -> std::vector<std::string>;

// Execute a parsed command
auto execute_command(const ParsedCommand& cmd, std::string repo_path) -> tea::CmdBatch;

// Update function for command mode
auto update_command(CommandModel model, tea::Msg msg) -> std::pair<CommandModel, tea::CmdBatch>;

// Initialize command mode
auto init_command(std::string repo_path) -> std::pair<CommandModel, tea::CmdBatch>;

// Helper: Activate command mode
auto activate_command_mode(CommandModel model) -> CommandModel;

// Helper: Deactivate command mode
auto deactivate_command_mode(CommandModel model) -> CommandModel;

} // namespace repo::tui::models
