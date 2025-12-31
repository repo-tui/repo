#include <repo/tui/render/primitives.hpp>

#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace repo::tui::render {

using namespace ftxui;

namespace text_helpers {

auto truncate(const std::string& text, size_t max_width, const std::string& ellipsis)
    -> std::string {
    if (text.length() <= max_width) {
        return text;
    }
    if (max_width <= ellipsis.length()) {
        return ellipsis.substr(0, max_width);
    }
    return text.substr(0, max_width - ellipsis.length()) + ellipsis;
}

auto pad_right(const std::string& text, size_t width, char fill) -> std::string {
    if (text.length() >= width) {
        return text;
    }
    return text + std::string(width - text.length(), fill);
}

auto pad_left(const std::string& text, size_t width, char fill) -> std::string {
    if (text.length() >= width) {
        return text;
    }
    return std::string(width - text.length(), fill) + text;
}

auto wrap(const std::string& text, size_t max_width) -> std::vector<std::string> {
    std::vector<std::string> lines;
    std::string current_line;

    std::istringstream stream(text);
    std::string word;

    while (stream >> word) {
        if (current_line.empty()) {
            current_line = word;
        } else if (current_line.length() + 1 + word.length() <= max_width) {
            current_line += " " + word;
        } else {
            lines.push_back(current_line);
            current_line = word;
        }
    }

    if (!current_line.empty()) {
        lines.push_back(current_line);
    }

    return lines;
}

auto format_size(size_t bytes) -> std::string {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    size_t unit_index = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unit_index < 4) {
        size /= 1024.0;
        unit_index++;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << size << " " << units[unit_index];
    return oss.str();
}

auto format_time_ago(std::chrono::system_clock::time_point time) -> std::string {
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - time);

    auto seconds = diff.count();

    if (seconds < 60) {
        return std::to_string(seconds) + " seconds ago";
    } else if (seconds < 3600) {
        auto minutes = seconds / 60;
        return std::to_string(minutes) + " minute" + (minutes == 1 ? "" : "s") + " ago";
    } else if (seconds < 86400) {
        auto hours = seconds / 3600;
        return std::to_string(hours) + " hour" + (hours == 1 ? "" : "s") + " ago";
    } else if (seconds < 2592000) {
        auto days = seconds / 86400;
        return std::to_string(days) + " day" + (days == 1 ? "" : "s") + " ago";
    } else if (seconds < 31536000) {
        auto months = seconds / 2592000;
        return std::to_string(months) + " month" + (months == 1 ? "" : "s") + " ago";
    } else {
        auto years = seconds / 31536000;
        return std::to_string(years) + " year" + (years == 1 ? "" : "s") + " ago";
    }
}

} // namespace text_helpers

// Table implementation

Table::Table(std::vector<Column> columns) : columns_(std::move(columns)) {}

auto Table::add_row(std::vector<std::string> cells) -> void {
    rows_.push_back(std::move(cells));
}

auto Table::render() -> Element {
    std::vector<Element> rows;

    // Render header
    std::vector<Element> header_cells;
    for (const auto& col : columns_) {
        header_cells.push_back(text(text_helpers::pad_right(col.header, col.width)) | bold |
                               color(Style::color_scheme().highlight));
    }
    rows.push_back(hbox(std::move(header_cells)));

    // Render separator
    std::string sep_line;
    for (const auto& col : columns_) {
        sep_line += std::string(col.width, '-');
    }
    rows.push_back(separator());

    // Render data rows
    for (const auto& row : rows_) {
        std::vector<Element> row_cells;
        for (size_t i = 0; i < columns_.size() && i < row.size(); ++i) {
            std::string cell_text = columns_[i].align_right
                                        ? text_helpers::pad_left(row[i], columns_[i].width)
                                        : text_helpers::pad_right(row[i], columns_[i].width);

            row_cells.push_back(text(cell_text) | color(columns_[i].color));
        }
        rows.push_back(hbox(std::move(row_cells)));
    }

    return vbox(std::move(rows));
}

// Status bar

auto status_bar(const std::string& left_text, const std::string& right_text, Color bg_color)
    -> Element {
    return hbox({text(left_text) | color(Color::White), filler(),
                 text(right_text) | color(Color::White)}) |
           bgcolor(bg_color);
}

// Key hints

auto key_hints(const std::vector<KeyHint>& hints) -> Element {
    std::vector<Element> elements;

    for (size_t i = 0; i < hints.size(); ++i) {
        if (i > 0) {
            elements.push_back(text("  "));
        }

        elements.push_back(text(hints[i].key) | bold | color(Style::color_scheme().highlight));
        elements.push_back(text(": " + hints[i].description));
    }

    return hbox(std::move(elements)) | color(Style::color_scheme().dimmed);
}

// Progress bar

auto progress_bar(float fraction, size_t width, Color color_) -> Element {
    fraction = std::clamp(fraction, 0.0f, 1.0f);
    size_t filled = static_cast<size_t>(fraction * width);

    std::string bar = std::string(filled, '=') + std::string(width - filled, '-');
    return text(bar) | color(color_);
}

// Spinner

auto spinner(size_t frame) -> Element {
    const char* frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    return text(frames[frame % 10]) | color(Style::color_scheme().info);
}

// Empty state

auto empty_state(const std::string& message, const std::string& hint) -> Element {
    std::vector<Element> elements;
    elements.push_back(text(message) | center | color(Style::color_scheme().dimmed));

    if (!hint.empty()) {
        elements.push_back(text(""));
        elements.push_back(text(hint) | center | dim);
    }

    return vbox(std::move(elements)) | center | flex;
}

// Error display

auto error_display(const std::string& title, const std::string& message) -> Element {
    return vbox({text(title) | bold | color(Style::color_scheme().error), text(""),
                 text(message) | color(Style::color_scheme().foreground)}) |
           border | borderStyled(HEAVY) | color(Style::color_scheme().error);
}

// Notification

auto notification(const std::string& message, Color color_, bool dismissible) -> Element {
    auto content = text(message);

    if (dismissible) {
        content = hbox({content, text(" "), text("[x]") | bold});
    }

    return content | bgcolor(color_) | color(Color::White);
}

// Loading indicator

auto loading(const std::string& message) -> Element {
    return hbox({spinner(0), text(" " + message)}) | center | color(Style::color_scheme().info);
}

// Separator

auto separator(size_t width) -> Element {
    if (width > 0) {
        return ftxui::text(std::string(width, '-')) | color(Style::color_scheme().border);
    }
    return ftxui::separator() | color(Style::color_scheme().border);
}

// Diff rendering

auto render_diff_line(const DiffLine& line) -> Element {
    Color line_color;
    std::string prefix;

    switch (line.type) {
        case DiffLine::Type::Added:
            prefix = "+ ";
            line_color = Style::color_scheme().diff_added;
            break;
        case DiffLine::Type::Removed:
            prefix = "- ";
            line_color = Style::color_scheme().diff_removed;
            break;
        case DiffLine::Type::Header:
            prefix = "@@ ";
            line_color = Style::color_scheme().diff_hunk;
            break;
        case DiffLine::Type::Context:
        default:
            prefix = "  ";
            line_color = Style::color_scheme().diff_context;
            break;
    }

    return text(prefix + line.content) | color(line_color);
}

auto render_diff_lines(const std::vector<DiffLine>& lines) -> Element {
    std::vector<Element> elements;
    for (const auto& line : lines) {
        elements.push_back(render_diff_line(line));
    }
    return vbox(std::move(elements));
}

// File tree rendering

auto FileTree::render_node(const Node& node, bool is_selected) -> Element {
    std::string indent(node.depth * 2, ' ');
    std::string icon = node.is_directory ? (node.is_expanded ? "▼ " : "▶ ") : "  ";

    auto elem = text(indent + icon + node.name);

    if (is_selected) {
        elem = elem | bgcolor(Style::color_scheme().selected_bg);
    }

    if (node.is_directory) {
        elem = elem | bold;
    }

    return elem;
}

auto FileTree::render_tree(const std::vector<Node>& roots, size_t selected_index) -> Element {
    std::vector<Element> elements;
    size_t current_index = 0;

    std::function<void(const std::vector<Node>&)> render_nodes;
    render_nodes = [&](const std::vector<Node>& nodes) {
        for (const auto& node : nodes) {
            bool is_selected = current_index == selected_index;
            elements.push_back(render_node(node, is_selected));
            current_index++;

            if (node.is_expanded && !node.children.empty()) {
                render_nodes(node.children);
            }
        }
    };

    render_nodes(roots);
    return vbox(std::move(elements));
}

// Commit graph rendering

auto render_commit_graph(const std::vector<CommitGraphNode>& nodes, size_t height) -> Element {
    std::vector<Element> lines;

    for (size_t i = 0; i < std::min(nodes.size(), height); ++i) {
        const auto& node = nodes[i];

        std::string graph_line(node.branch_column * 2, ' ');
        graph_line += node.is_merge ? "◆" : "●";

        lines.push_back(text(graph_line) | color(Style::color_scheme().commit_hash));
    }

    return vbox(std::move(lines));
}

// Branch tree rendering

auto render_branch_tree(const std::vector<std::string>& branches, const std::string& current_branch,
                        size_t selected_index) -> Element {
    std::vector<Element> elements;

    for (size_t i = 0; i < branches.size(); ++i) {
        bool is_current = branches[i] == current_branch;
        bool is_selected = i == selected_index;

        auto elem = hbox({is_current ? text("* ") : text("  "), text(branches[i])});

        if (is_current) {
            elem = elem | color(Style::color_scheme().branch_current);
        }

        if (is_selected) {
            elem = elem | bgcolor(Style::color_scheme().selected_bg);
        }

        elements.push_back(elem);
    }

    return vbox(std::move(elements));
}

} // namespace repo::tui::render
