#pragma once

#include <repo/tui/models/diff.hpp>

#include <ftxui/dom/elements.hpp>

namespace repo::tui::views {

// Render the diff view
auto render_diff(const models::DiffModel& model) -> ftxui::Element;

// Helper: Render file list summary
auto render_diff_file_list(const models::DiffModel& model) -> ftxui::Element;

// Helper: Render a single file's diff
auto render_file_diff(const domain::FileDiff& file, bool is_selected, size_t selected_hunk,
                      size_t selected_line, bool show_line_numbers) -> ftxui::Element;

// Helper: Render a single hunk
auto render_diff_hunk(const domain::DiffHunk& hunk, bool is_selected, size_t selected_line,
                      bool show_line_numbers) -> ftxui::Element;

// Helper: Render a single diff line
auto render_diff_line_item(const domain::DiffLine& line, bool is_selected, bool show_line_numbers)
    -> ftxui::Element;

// Helper: Render help screen
auto render_diff_help_screen() -> ftxui::Element;

// Helper: Render status bar at bottom
auto render_diff_status_bar(const models::DiffModel& model) -> ftxui::Element;

} // namespace repo::tui::views
