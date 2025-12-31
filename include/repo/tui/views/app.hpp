#pragma once

#include <repo/tui/models/app.hpp>

#include <ftxui/dom/elements.hpp>

namespace repo::tui::views {

// Render the entire application
auto render_app(const models::AppModel& model) -> ftxui::Element;

// Helper: Render tab bar with view names
auto render_tab_bar(const models::AppModel& model) -> ftxui::Element;

// Helper: Render global status bar at bottom
auto render_global_status_bar(const models::AppModel& model) -> ftxui::Element;

// Helper: Get current view content
auto render_current_view(const models::AppModel& model) -> ftxui::Element;

} // namespace repo::tui::views
