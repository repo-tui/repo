#include <repo/tui/render/primitives.hpp>
#include <repo/tui/render/style.hpp>
#include <repo/tui/views/status.hpp>

#include <ftxui/dom/elements.hpp>

#include <sstream>

namespace repo::tui::views {

using namespace ftxui;
using namespace repo::tui::render;

// Render the status view
auto render_status(const models::StatusModel& model) -> Element {
    // Show help screen if requested
    if (model.mode == models::StatusMode::Help) {
        return render_help_screen();
    }

    // Show commit dialog if in commit mode
    if (model.mode == models::StatusMode::CommitInput) {
        return vbox({render_file_list(model) | flex, separator(), render_commit_dialog(model)});
    }

    // Normal mode - show file list and status
    std::vector<Element> elements;

    // Title
    elements.push_back(text("Status") | bold | color(Style::color_scheme().highlight) | center);
    elements.push_back(separator());

    // Status summary
    elements.push_back(render_status_summary(model));
    elements.push_back(separator());

    // File list
    if (model.is_loading) {
        elements.push_back(loading("Loading repository status...") | flex | center);
    } else if (model.error) {
        elements.push_back(error_display("Error", model.error->message) | flex | center);
    } else if (model.filtered_files.empty()) {
        std::string empty_msg;
        switch (model.filter) {
            case models::FileFilter::All:
                empty_msg = "No changes (working tree clean)";
                break;
            case models::FileFilter::Staged:
                empty_msg = "No staged files";
                break;
            case models::FileFilter::Unstaged:
                empty_msg = "No unstaged files";
                break;
            case models::FileFilter::Untracked:
                empty_msg = "No untracked files";
                break;
        }
        elements.push_back(empty_state(empty_msg, "Press 'r' to refresh") | flex | center);
    } else {
        elements.push_back(render_file_list(model) | flex);
    }

    // Status bar at bottom
    elements.push_back(separator());
    elements.push_back(render_status_bar(model));

    // Notification banner (if any)
    if (model.notification) {
        return vbox({render_notification(*model.notification), vbox(std::move(elements))});
    }

    return vbox(std::move(elements));
}

// Helper: Render file list
auto render_file_list(const models::StatusModel& model) -> Element {
    std::vector<Element> rows;

    for (size_t i = 0; i < model.filtered_files.size(); ++i) {
        bool is_selected = (i == model.selected_index);
        rows.push_back(render_file_item(model.filtered_files[i], is_selected));
    }

    return vbox(std::move(rows));
}

// Helper: Render a single file item
auto render_file_item(const domain::FileStatus& file, bool is_selected) -> Element {
    // Determine status icon and color
    std::string index_icon;
    std::string workdir_icon;

    // Index status (left column)
    switch (file.index_status) {
        case domain::FileStatus::State::Added:
            index_icon = "A";
            break;
        case domain::FileStatus::State::Modified:
            index_icon = "M";
            break;
        case domain::FileStatus::State::Deleted:
            index_icon = "D";
            break;
        case domain::FileStatus::State::Renamed:
            index_icon = "R";
            break;
        default:
            index_icon = " ";
            break;
    }

    // Worktree status (right column)
    switch (file.worktree_status) {
        case domain::FileStatus::State::Modified:
            workdir_icon = "M";
            break;
        case domain::FileStatus::State::Deleted:
            workdir_icon = "D";
            break;
        case domain::FileStatus::State::Untracked:
            workdir_icon = "?";
            break;
        default:
            workdir_icon = " ";
            break;
    }

    // Build the file row
    auto elem = hbox({text(index_icon) | color(Style::file_status_color(index_icon)),
                      text(workdir_icon) | color(Style::file_status_color(workdir_icon)), text(" "),
                      text(file.path.string())});

    // Highlight if selected
    if (is_selected) {
        elem = elem | bgcolor(Style::color_scheme().selected_bg) |
               color(Style::color_scheme().selected);
    }

    return elem;
}

// Helper: Render status summary
auto render_status_summary(const models::StatusModel& model) -> Element {
    size_t staged_count = 0;
    size_t unstaged_count = 0;
    size_t untracked_count = 0;

    for (const auto& file : model.files) {
        if (file.is_staged()) {
            staged_count++;
        }
        if (file.is_unstaged()) {
            unstaged_count++;
        }
        if (file.is_untracked()) {
            untracked_count++;
        }
    }

    // Current filter indicator
    std::string filter_name;
    switch (model.filter) {
        case models::FileFilter::All:
            filter_name = "All";
            break;
        case models::FileFilter::Staged:
            filter_name = "Staged";
            break;
        case models::FileFilter::Unstaged:
            filter_name = "Unstaged";
            break;
        case models::FileFilter::Untracked:
            filter_name = "Untracked";
            break;
    }

    return hbox({text("Staged: ") | bold,
                 text(std::to_string(staged_count)) | color(Style::color_scheme().added),
                 text("  "), text("Unstaged: ") | bold,
                 text(std::to_string(unstaged_count)) | color(Style::color_scheme().modified),
                 text("  "), text("Untracked: ") | bold,
                 text(std::to_string(untracked_count)) | color(Style::color_scheme().untracked),
                 text("  "), text("Filter: ") | bold,
                 text(filter_name) | color(Style::color_scheme().info)});
}

// Helper: Render commit dialog
auto render_commit_dialog(const models::StatusModel& model) -> Element {
    // Show cursor in commit message
    std::string display_message = model.commit_message;
    if (model.commit_cursor_pos <= display_message.size()) {
        display_message.insert(model.commit_cursor_pos, "|");
    }

    return vbox({text("Commit Message") | bold | color(Style::color_scheme().highlight), text(""),
                 text(display_message.empty() ? "(type your commit message)" : display_message),
                 text(""),
                 hbox({text("Ctrl+Enter: ") | bold, text("commit  "), text("Esc: ") | bold,
                       text("cancel")}) |
                     dim}) |
           border | borderStyled(ROUNDED);
}

// Helper: Render help screen
auto render_help_screen() -> Element {
    return vbox({text("Status View - Keyboard Shortcuts") | bold | center |
                     color(Style::color_scheme().highlight),
                 text(""),

                 text("Navigation:") | bold,
                 hbox({text("  j/↓"), filler(), text("Move down")}),
                 hbox({text("  k/↑"), filler(), text("Move up")}),
                 hbox({text("  g/Home"), filler(), text("Jump to top")}),
                 hbox({text("  G/End"), filler(), text("Jump to bottom")}),
                 hbox({text("  PgUp/PgDn"), filler(), text("Page up/down")}),
                 text(""),

                 text("Actions:") | bold,
                 hbox({text("  Space"), filler(), text("Toggle stage/unstage")}),
                 hbox({text("  s"), filler(), text("Stage file")}),
                 hbox({text("  u"), filler(), text("Unstage file")}),
                 hbox({text("  a"), filler(), text("Stage all")}),
                 hbox({text("  A"), filler(), text("Unstage all")}),
                 hbox({text("  c"), filler(), text("Commit staged files")}),
                 text(""),

                 text("View:") | bold,
                 hbox({text("  r"), filler(), text("Refresh status")}),
                 hbox({text("  f"), filler(), text("Cycle file filter")}),
                 hbox({text("  ?"), filler(), text("Toggle help")}),
                 text(""),

                 text("Press any key to close help") | center | dim}) |
           border | borderStyled(HEAVY) | center;
}

// Helper: Render status bar
auto render_status_bar(const models::StatusModel& model) -> Element {
    std::vector<KeyHint> hints;

    if (model.mode == models::StatusMode::Normal) {
        hints = {{"↑↓", "navigate"}, {"Space", "stage/unstage"},
                 {"c", "commit"},    {"r", "refresh"},
                 {"f", "filter"},    {"?", "help"}};
    } else if (model.mode == models::StatusMode::CommitInput) {
        hints = {{"Ctrl+Enter", "commit"}, {"Esc", "cancel"}};
    }

    return key_hints(hints);
}

// Helper: Render notification
auto render_notification(const std::string& message) -> Element {
    return notification(message, Style::color_scheme().info, true);
}

} // namespace repo::tui::views
