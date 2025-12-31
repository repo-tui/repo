#pragma once

#include <repo/tui/models/command.hpp>

#include <ftxui/dom/elements.hpp>

namespace repo::tui::views {

// Render command input line (shown at bottom when active)
auto render_command_input(const models::CommandModel& model) -> ftxui::Element;

// Render command suggestions/autocomplete
auto render_command_suggestions(const models::CommandModel& model) -> ftxui::Element;

// Render command result message
auto render_command_result(const models::CommandModel& model) -> ftxui::Element;

} // namespace repo::tui::views
