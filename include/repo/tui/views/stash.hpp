#pragma once

#include <repo/tui/models/stash.hpp>

#include <ftxui/dom/elements.hpp>

namespace repo::tui::views {

// Render the stash view
auto render_stash(const models::StashModel& model) -> ftxui::Element;

// Helper: Render stash list
auto render_stash_list(const models::StashModel& model) -> ftxui::Element;

// Helper: Render a single stash item
auto render_stash_item(const domain::Stash& stash, bool is_selected) -> ftxui::Element;

// Helper: Render create stash dialog
auto render_create_stash_dialog(const models::StashModel& model) -> ftxui::Element;

// Helper: Render apply confirmation dialog
auto render_apply_confirmation(const models::StashModel& model) -> ftxui::Element;

// Helper: Render pop confirmation dialog
auto render_pop_confirmation(const models::StashModel& model) -> ftxui::Element;

// Helper: Render drop confirmation dialog
auto render_drop_confirmation(const models::StashModel& model) -> ftxui::Element;

// Helper: Render stash details view
auto render_stash_details(const models::StashModel& model) -> ftxui::Element;

// Helper: Render help screen
auto render_stash_help_screen() -> ftxui::Element;

// Helper: Render status bar at bottom
auto render_stash_status_bar(const models::StashModel& model) -> ftxui::Element;

} // namespace repo::tui::views
