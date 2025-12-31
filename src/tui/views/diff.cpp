#include <repo/tui/render/primitives.hpp>
#include <repo/tui/render/style.hpp>
#include <repo/tui/views/diff.hpp>

#include <ftxui/dom/elements.hpp>

#include <sstream>

namespace repo::tui::views {

using namespace ftxui;
using namespace repo::tui::render;

// Helper: Get diff type name
static auto diff_type_name(models::DiffType type) -> std::string {
    switch (type) {
        case models::DiffType::Unstaged:
            return "Unstaged Changes";
        case models::DiffType::Staged:
            return "Staged Changes";
        case models::DiffType::All:
            return "All Changes";
    }
    return "Unknown";
}

// Helper: Get file status icon
static auto file_status_icon(const domain::FileDiff& file) -> std::string {
    switch (file.status) {
        case domain::FileDiff::Status::Added:
            return "[+]";
        case domain::FileDiff::Status::Deleted:
            return "[-]";
        case domain::FileDiff::Status::Modified:
            return "[M]";
        case domain::FileDiff::Status::Renamed:
            return "[R]";
        case domain::FileDiff::Status::Copied:
            return "[C]";
        case domain::FileDiff::Status::TypeChanged:
            return "[T]";
    }
    return "[?]";
}

// Helper: Get file status color
static auto file_status_color(const domain::FileDiff& file) -> Color {
    switch (file.status) {
        case domain::FileDiff::Status::Added:
            return Style::color_scheme().added;
        case domain::FileDiff::Status::Deleted:
            return Style::color_scheme().deleted;
        case domain::FileDiff::Status::Modified:
            return Style::color_scheme().modified;
        case domain::FileDiff::Status::Renamed:
            return Style::color_scheme().renamed;
        default:
            return Style::color_scheme().foreground;
    }
}

// Render the diff view
auto render_diff(const models::DiffModel& model) -> Element {
    // Show help screen if requested
    if (model.mode == models::DiffMode::Help) {
        return render_diff_help_screen();
    }

    // Normal mode - show diff
    std::vector<Element> elements;

    // Title with diff type
    elements.push_back(text(diff_type_name(model.diff_type)) | bold |
                       color(Style::color_scheme().highlight) | center);
    elements.push_back(separator());

    // File list summary if multiple files
    if (model.file_diffs.size() > 1) {
        elements.push_back(render_diff_file_list(model));
        elements.push_back(separator());
    }

    // Main diff content
    if (model.is_loading) {
        elements.push_back(loading("Loading diff...") | flex | center);
    } else if (model.error) {
        elements.push_back(error_display("Error", model.error->message) | flex | center);
    } else if (model.file_diffs.empty()) {
        std::string empty_msg = "No changes to display";
        elements.push_back(empty_state(empty_msg, "Press 't' to toggle diff type") | flex | center);
    } else {
        // Render the selected file's diff
        auto file = selected_file_diff(model);
        if (file) {
            elements.push_back(render_file_diff(*file, true, model.selected_hunk_index,
                                                model.selected_line_index,
                                                model.show_line_numbers) |
                               flex);
        }
    }

    // Status bar at bottom
    elements.push_back(separator());
    elements.push_back(render_diff_status_bar(model));

    // Notification banner (if any)
    if (model.notification) {
        return vbox({notification(*model.notification, Style::color_scheme().info, true),
                     vbox(std::move(elements))});
    }

    return vbox(std::move(elements));
}

// Helper: Render file list summary
auto render_diff_file_list(const models::DiffModel& model) -> Element {
    std::vector<Element> file_items;

    for (size_t i = 0; i < model.file_diffs.size(); ++i) {
        const auto& file = model.file_diffs[i];
        bool is_selected = (i == model.selected_file_index);

        auto elem = hbox({text(file_status_icon(file)) | color(file_status_color(file)), text(" "),
                          text(file.path.string()), text(" "),
                          text("(+" + std::to_string(file.additions) + " -" +
                               std::to_string(file.deletions) + ")") |
                              dim});

        if (is_selected) {
            elem = elem | bgcolor(Style::color_scheme().selected_bg);
        }

        file_items.push_back(elem);
    }

    return vbox(std::move(file_items));
}

// Helper: Render a single file's diff
auto render_file_diff(const domain::FileDiff& file, bool /* is_selected */, size_t selected_hunk,
                      size_t selected_line, bool show_line_numbers) -> Element {
    std::vector<Element> elements;

    // File header
    std::string header = "diff --git a/" + file.path.string() + " b/" + file.path.string();
    elements.push_back(text(header) | color(Style::color_scheme().diff_hunk) | bold);

    if (file.old_path) {
        elements.push_back(text("renamed from " + file.old_path->string()) | dim);
    }

    elements.push_back(text(""));

    // Binary file check
    if (file.is_binary) {
        elements.push_back(text("Binary file - no diff available") | center | dim);
        return vbox(std::move(elements));
    }

    // Render hunks
    for (size_t i = 0; i < file.hunks.size(); ++i) {
        bool hunk_selected = (i == selected_hunk);
        elements.push_back(render_diff_hunk(file.hunks[i], hunk_selected,
                                            hunk_selected ? selected_line : 0, show_line_numbers));
        elements.push_back(text("")); // Spacing between hunks
    }

    return vbox(std::move(elements));
}

// Helper: Render a single hunk
auto render_diff_hunk(const domain::DiffHunk& hunk, bool is_selected, size_t selected_line,
                      bool show_line_numbers) -> Element {
    std::vector<Element> elements;

    // Hunk header
    elements.push_back(text(hunk.header) | color(Style::color_scheme().diff_hunk) | bold |
                       (is_selected ? bgcolor(Style::color_scheme().selected_bg) : nothing));

    // Render lines
    for (size_t i = 0; i < hunk.lines.size(); ++i) {
        bool line_selected = is_selected && (i == selected_line);
        elements.push_back(render_diff_line_item(hunk.lines[i], line_selected, show_line_numbers));
    }

    return vbox(std::move(elements));
}

// Helper: Render a single diff line
auto render_diff_line_item(const domain::DiffLine& line, bool is_selected, bool show_line_numbers)
    -> Element {
    // Determine color and prefix based on origin
    Color line_color;
    std::string prefix;

    switch (line.origin) {
        case domain::DiffLine::Origin::Addition:
            prefix = "+";
            line_color = Style::color_scheme().diff_added;
            break;
        case domain::DiffLine::Origin::Deletion:
            prefix = "-";
            line_color = Style::color_scheme().diff_removed;
            break;
        case domain::DiffLine::Origin::Context:
        default:
            prefix = " ";
            line_color = Style::color_scheme().diff_context;
            break;
    }

    // Build line content
    std::vector<Element> line_parts;

    // Line numbers (if enabled)
    if (show_line_numbers) {
        std::string old_num = line.old_lineno ? std::to_string(*line.old_lineno) : "    ";
        std::string new_num = line.new_lineno ? std::to_string(*line.new_lineno) : "    ";

        line_parts.push_back(text(text_helpers::pad_left(old_num, 4)) |
                             color(Style::color_scheme().dimmed));
        line_parts.push_back(text(" "));
        line_parts.push_back(text(text_helpers::pad_left(new_num, 4)) |
                             color(Style::color_scheme().dimmed));
        line_parts.push_back(text(" "));
    }

    // Prefix and content
    line_parts.push_back(text(prefix));
    line_parts.push_back(text(line.content));

    auto elem = hbox(std::move(line_parts)) | color(line_color);

    // Highlight if selected
    if (is_selected) {
        elem = elem | bgcolor(Style::color_scheme().selected_bg);
    }

    return elem;
}

// Helper: Render help screen
auto render_diff_help_screen() -> Element {
    return vbox({text("Diff View - Keyboard Shortcuts") | bold | center |
                     color(Style::color_scheme().highlight),
                 text(""),

                 text("Navigation:") | bold,
                 hbox({text("  J/K"), filler(), text("Next/previous file")}),
                 hbox({text("  j/k/↑↓"), filler(), text("Next/previous hunk")}),
                 hbox({text("  n/p"), filler(), text("Next/previous line")}),
                 hbox({text("  g/Home"), filler(), text("Jump to first file")}),
                 hbox({text("  G/End"), filler(), text("Jump to last file")}),
                 text(""),

                 text("Actions:") | bold,
                 hbox({text("  Space"), filler(), text("Stage/unstage entire file")}),
                 hbox({text("  s"), filler(), text("Stage current hunk (TODO)")}),
                 hbox({text("  u"), filler(), text("Unstage current hunk (TODO)")}),
                 text(""),

                 text("View:") | bold,
                 hbox({text("  t"), filler(), text("Toggle diff type (unstaged/staged/all)")}),
                 hbox({text("  l"), filler(), text("Toggle line numbers")}),
                 hbox({text("  r"), filler(), text("Refresh diff")}),
                 hbox({text("  ?"), filler(), text("Toggle help")}),
                 text(""),

                 text("Press any key to close help") | center | dim}) |
           border | borderStyled(HEAVY) | center;
}

// Helper: Render status bar
auto render_diff_status_bar(const models::DiffModel& model) -> Element {
    std::vector<KeyHint> hints = {
        {"J/K", "files"},     {"j/k", "hunks"},      {"Space", "stage/unstage"},
        {"t", "toggle type"}, {"l", "line numbers"}, {"r", "refresh"},
        {"?", "help"}};

    auto hint_elem = key_hints(hints);

    // Add file count and stats
    if (!model.file_diffs.empty()) {
        size_t total_additions = 0;
        size_t total_deletions = 0;

        for (const auto& file : model.file_diffs) {
            total_additions += file.additions;
            total_deletions += file.deletions;
        }

        auto stats =
            hbox({text("Files: ") | bold, text(std::to_string(model.file_diffs.size())), text("  "),
                  text("+") | color(Style::color_scheme().added),
                  text(std::to_string(total_additions)) | color(Style::color_scheme().added),
                  text("  "), text("-") | color(Style::color_scheme().deleted),
                  text(std::to_string(total_deletions)) | color(Style::color_scheme().deleted),
                  text("  "), hint_elem});
        return stats;
    }

    return hint_elem;
}

} // namespace repo::tui::views
