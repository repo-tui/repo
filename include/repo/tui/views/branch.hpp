#pragma once

#include <repo/tui/models/branch.hpp>

#include <ftxui/dom/elements.hpp>

namespace repo::tui::views {

// Render the branch view
auto render_branch(const models::BranchModel& model) -> ftxui::Element;

// Helper: Render branch list
auto render_branch_list(const models::BranchModel& model) -> ftxui::Element;

// Helper: Render a single branch item
auto render_branch_item(const domain::Branch& branch, bool is_selected, bool is_current)
    -> ftxui::Element;

// Helper: Render create branch dialog
auto render_create_branch_dialog(const models::BranchModel& model) -> ftxui::Element;

// Helper: Render rename branch dialog
auto render_rename_branch_dialog(const models::BranchModel& model) -> ftxui::Element;

// Helper: Render delete confirmation dialog
auto render_delete_confirmation(const models::BranchModel& model) -> ftxui::Element;

// Helper: Render merge confirmation dialog
auto render_merge_confirmation(const models::BranchModel& model) -> ftxui::Element;

// Helper: Render help screen
auto render_branch_help_screen() -> ftxui::Element;

// Helper: Render status bar at bottom
auto render_branch_status_bar(const models::BranchModel& model) -> ftxui::Element;

} // namespace repo::tui::views
