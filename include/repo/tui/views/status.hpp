#pragma once

#include <repo/tui/models/status.hpp>

#include <ftxui/dom/elements.hpp>

namespace repo::tui::views {

// Render the status view
auto render_status(const models::StatusModel& model) -> ftxui::Element;

// Helper: Render file list
auto render_file_list(const models::StatusModel& model) -> ftxui::Element;

// Helper: Render a single file item
auto render_file_item(const domain::FileStatus& file, bool is_selected) -> ftxui::Element;

// Helper: Render status summary (counts)
auto render_status_summary(const models::StatusModel& model) -> ftxui::Element;

// Helper: Render commit input dialog
auto render_commit_dialog(const models::StatusModel& model) -> ftxui::Element;

// Helper: Render help screen
auto render_help_screen() -> ftxui::Element;

// Helper: Render status bar at bottom
auto render_status_bar(const models::StatusModel& model) -> ftxui::Element;

// Helper: Render notification banner
auto render_notification(const std::string& message) -> ftxui::Element;

} // namespace repo::tui::views
