#include <repo/tui/render/primitives.hpp>
#include <repo/tui/render/style.hpp>
#include <repo/tui/views/log.hpp>

#include <ftxui/dom/elements.hpp>

#include <chrono>
#include <iomanip>
#include <sstream>

namespace repo::tui::views {

using namespace ftxui;
using namespace repo::tui::render;

// Helper: Format timestamp
static auto format_timestamp(const domain::Signature& sig) -> std::string {
    auto time = sig.when;
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::hours>(now - time).count();

    if (diff < 24) {
        return text_helpers::format_time_ago(time);
    } else if (diff < 24 * 7) {
        auto days = diff / 24;
        return std::to_string(days) + " day" + (days == 1 ? "" : "s") + " ago";
    } else {
        // Format as date
        auto time_t = std::chrono::system_clock::to_time_t(time);
        std::tm tm = *std::localtime(&time_t);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d");
        return oss.str();
    }
}

// Render the log view
auto render_log(const models::LogModel& model) -> Element {
    // Show help screen if requested
    if (model.mode == models::LogMode::Help) {
        return render_log_help_screen();
    }

    // Show commit details if in details mode
    if (model.mode == models::LogMode::Details) {
        return render_commit_details(model);
    }

    // Normal mode - show commit list
    std::vector<Element> elements;

    // Title
    elements.push_back(text("Commit Log") | bold | color(Style::color_scheme().highlight) | center);
    elements.push_back(separator());

    // Commit list
    if (model.is_loading) {
        elements.push_back(loading("Loading commit history...") | flex | center);
    } else if (model.error) {
        elements.push_back(error_display("Error", model.error->message) | flex | center);
    } else if (model.commits.empty()) {
        elements.push_back(empty_state("No commits found", "Press 'r' to refresh") | flex | center);
    } else {
        elements.push_back(render_commit_list(model) | flex);

        // Show loading indicator if loading more
        if (model.is_loading_more) {
            elements.push_back(text("Loading more commits...") | dim | center);
        }
    }

    // Status bar at bottom
    elements.push_back(separator());
    elements.push_back(render_log_status_bar(model));

    // Notification banner (if any)
    if (model.notification) {
        return vbox({notification(*model.notification, Style::color_scheme().info, true),
                     vbox(std::move(elements))});
    }

    return vbox(std::move(elements));
}

// Helper: Render commit list
auto render_commit_list(const models::LogModel& model) -> Element {
    std::vector<Element> rows;

    for (size_t i = 0; i < model.commits.size(); ++i) {
        bool is_selected = (i == model.selected_index);
        rows.push_back(render_commit_item(model.commits[i], is_selected, model.format));
    }

    return vbox(std::move(rows));
}

// Helper: Render a single commit item
auto render_commit_item(const domain::Commit& commit, bool is_selected, models::LogFormat format)
    -> Element {
    auto hash_short = commit.id.to_string().substr(0, 7);

    Element elem;

    switch (format) {
        case models::LogFormat::Compact: {
            // One line: hash + message
            elem = hbox({text(hash_short) | color(Style::color_scheme().commit_hash), text(" "),
                         text(commit.summary())});
            break;
        }

        case models::LogFormat::Medium: {
            // Hash, author, date, message
            elem = hbox({text(hash_short) | color(Style::color_scheme().commit_hash), text(" "),
                         text(commit.author.name.substr(0, 15)) |
                             color(Style::color_scheme().commit_author),
                         text(" "),
                         text(format_timestamp(commit.author)) |
                             color(Style::color_scheme().commit_date) | dim,
                         text(" "), text(commit.summary())});
            break;
        }

        case models::LogFormat::Full: {
            // Full details - multiple lines
            elem = vbox({hbox({text("commit "), text(commit.id.to_string()) |
                                                    color(Style::color_scheme().commit_hash)}),
                         hbox({text("Author: "),
                               text(commit.author.name + " <" + commit.author.email + ">") |
                                   color(Style::color_scheme().commit_author)}),
                         hbox({text("Date:   "), text(format_timestamp(commit.author)) |
                                                     color(Style::color_scheme().commit_date)}),
                         text(""), text("    " + commit.summary()), text("")});
            break;
        }
    }

    // Highlight if selected
    if (is_selected) {
        elem = elem | bgcolor(Style::color_scheme().selected_bg) |
               color(Style::color_scheme().selected);
    }

    return elem;
}

// Helper: Render commit details view
auto render_commit_details(const models::LogModel& model) -> Element {
    if (!model.detail_commit) {
        return text("No commit selected") | center;
    }

    const auto& commit = *model.detail_commit;
    std::vector<Element> elements;

    // Commit header
    elements.push_back(text("Commit Details") | bold | color(Style::color_scheme().highlight) |
                       center);
    elements.push_back(separator());

    // Commit information
    elements.push_back(
        hbox({text("Commit: ") | bold,
              text(commit.id.to_string()) | color(Style::color_scheme().commit_hash)}));

    elements.push_back(
        hbox({text("Author: ") | bold, text(commit.author.name + " <" + commit.author.email + ">") |
                                           color(Style::color_scheme().commit_author)}));

    elements.push_back(
        hbox({text("Date:   ") | bold,
              text(format_timestamp(commit.author)) | color(Style::color_scheme().commit_date)}));

    if (commit.is_merge()) {
        elements.push_back(
            hbox({text("Merge:  ") | bold, text("Yes") | color(Style::color_scheme().warning)}));
    }

    elements.push_back(text(""));

    // Commit message
    elements.push_back(text("Message:") | bold);
    elements.push_back(text(commit.message) | border);

    elements.push_back(text(""));
    elements.push_back(separator());

    // Diff (if loaded)
    if (model.detail_diff) {
        elements.push_back(text("Changes:") | bold);
        elements.push_back(text(""));

        // Split diff into lines and render
        std::istringstream diff_stream(*model.detail_diff);
        std::string line;
        std::vector<Element> diff_lines;

        while (std::getline(diff_stream, line) && diff_lines.size() < 50) {
            Color line_color = Style::color_scheme().foreground;

            if (line.starts_with("+")) {
                line_color = Style::color_scheme().diff_added;
            } else if (line.starts_with("-")) {
                line_color = Style::color_scheme().diff_removed;
            } else if (line.starts_with("@@")) {
                line_color = Style::color_scheme().diff_hunk;
            }

            diff_lines.push_back(text(line) | color(line_color));
        }

        if (diff_lines.empty()) {
            diff_lines.push_back(text("(loading diff...)") | dim);
        }

        elements.push_back(vbox(std::move(diff_lines)) | flex);
    } else {
        elements.push_back(loading("Loading diff...") | center);
    }

    elements.push_back(separator());
    elements.push_back(hbox({text("q/Esc: ") | bold, text("back to list")}) | dim);

    return vbox(std::move(elements));
}

// Helper: Render help screen
auto render_log_help_screen() -> Element {
    return vbox({text("Log View - Keyboard Shortcuts") | bold | center |
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
                 hbox({text("  Enter/d"), filler(), text("Show commit details")}),
                 hbox({text("  s"), filler(), text("Select commit")}),
                 hbox({text("  o"), filler(), text("Checkout commit")}),
                 text(""),

                 text("View:") | bold,
                 hbox({text("  f"), filler(), text("Cycle format (compact/medium/full)")}),
                 hbox({text("  r"), filler(), text("Refresh log")}),
                 hbox({text("  ?"), filler(), text("Toggle help")}),
                 text(""),

                 text("Details View:") | bold,
                 hbox({text("  q/Esc"), filler(), text("Back to list")}),
                 text(""),

                 text("Press any key to close help") | center | dim}) |
           border | borderStyled(HEAVY) | center;
}

// Helper: Render status bar
auto render_log_status_bar(const models::LogModel& model) -> Element {
    std::vector<KeyHint> hints;

    if (model.mode == models::LogMode::Normal) {
        hints = {{"↑↓", "navigate"}, {"Enter", "details"}, {"s", "select"},
                 {"f", "format"},    {"r", "refresh"},     {"?", "help"}};
    } else if (model.mode == models::LogMode::Details) {
        hints = {{"q/Esc", "back"}};
    }

    auto hint_elem = key_hints(hints);

    // Add commit count info
    if (!model.commits.empty()) {
        auto info = hbox({text("Commits: ") | bold, text(std::to_string(model.commits.size())),
                          text(model.has_more ? "+" : ""), text("  "), hint_elem});
        return info;
    }

    return hint_elem;
}

} // namespace repo::tui::views
