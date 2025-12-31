#pragma once

#include <repo/tui/models/remote.hpp>

#include <ftxui/dom/elements.hpp>

namespace repo::tui::views {

// Render the remote view
auto render_remote(const models::RemoteModel& model) -> ftxui::Element;

// Helper: Render remote list
auto render_remote_list(const models::RemoteModel& model) -> ftxui::Element;

// Helper: Render a single remote item
auto render_remote_item(const domain::Remote& remote, bool is_selected) -> ftxui::Element;

// Helper: Render add remote dialog
auto render_add_remote_dialog(const models::RemoteModel& model) -> ftxui::Element;

// Helper: Render remove confirmation dialog
auto render_remove_confirmation(const models::RemoteModel& model) -> ftxui::Element;

// Helper: Render operation progress (fetch/push/pull)
auto render_operation_progress(const models::RemoteModel& model, const std::string& operation)
    -> ftxui::Element;

// Helper: Render help screen
auto render_remote_help_screen() -> ftxui::Element;

// Helper: Render status bar at bottom
auto render_remote_status_bar(const models::RemoteModel& model) -> ftxui::Element;

} // namespace repo::tui::views
