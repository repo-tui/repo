#pragma once

#include <repo/tui/models/log.hpp>

#include <ftxui/dom/elements.hpp>

namespace repo::tui::views {

// Render the log view
auto render_log(const models::LogModel& model) -> ftxui::Element;

// Helper: Render commit list
auto render_commit_list(const models::LogModel& model) -> ftxui::Element;

// Helper: Render a single commit item
auto render_commit_item(const domain::Commit& commit, bool is_selected, models::LogFormat format)
    -> ftxui::Element;

// Helper: Render commit details view
auto render_commit_details(const models::LogModel& model) -> ftxui::Element;

// Helper: Render help screen
auto render_log_help_screen() -> ftxui::Element;

// Helper: Render status bar at bottom
auto render_log_status_bar(const models::LogModel& model) -> ftxui::Element;

} // namespace repo::tui::views
