#pragma once

#include <ftxui/dom/elements.hpp>

#include <string>
#include <vector>

#include "style.hpp"

namespace repo::tui::render {

using namespace ftxui;

// Text rendering helpers
namespace text_helpers {

// Truncate text to fit width
auto truncate(const std::string& text, size_t max_width, const std::string& ellipsis = "...")
    -> std::string;

// Pad text to width
auto pad_right(const std::string& text, size_t width, char fill = ' ') -> std::string;
auto pad_left(const std::string& text, size_t width, char fill = ' ') -> std::string;

// Wrap text to multiple lines
auto wrap(const std::string& text, size_t max_width) -> std::vector<std::string>;

// Format file size (bytes -> KB, MB, etc.)
auto format_size(size_t bytes) -> std::string;

// Format time ago (e.g., "2 hours ago")
auto format_time_ago(std::chrono::system_clock::time_point time) -> std::string;

} // namespace text_helpers

// Table rendering
class Table {
  public:
    struct Column {
        std::string header;
        size_t width;
        bool align_right = false;
        Color color = Color::White;
    };

    explicit Table(std::vector<Column> columns);

    // Add a row
    auto add_row(std::vector<std::string> cells) -> void;

    // Render table
    auto render() -> Element;

  private:
    std::vector<Column> columns_;
    std::vector<std::vector<std::string>> rows_;
};

// Status bar at bottom of screen
auto status_bar(const std::string& left_text, const std::string& right_text = "",
                Color bg_color = Color::Blue) -> Element;

// Key binding hint (e.g., "q: quit  h: help")
struct KeyHint {
    std::string key;
    std::string description;
};

auto key_hints(const std::vector<KeyHint>& hints) -> Element;

// Progress bar
auto progress_bar(float fraction, size_t width = 40, Color color = Color::Green) -> Element;

// Spinner animation
auto spinner(size_t frame) -> Element;

// Empty state placeholder
auto empty_state(const std::string& message, const std::string& hint = "") -> Element;

// Error display
auto error_display(const std::string& title, const std::string& message) -> Element;

// Notification banner
auto notification(const std::string& message, Color color = Color::Blue, bool dismissible = true)
    -> Element;

// Loading indicator
auto loading(const std::string& message = "Loading...") -> Element;

// Separator line
auto separator(size_t width = 0) -> Element;

// Git diff hunk rendering
struct DiffLine {
    enum class Type { Context, Added, Removed, Header };
    Type type;
    std::string content;
};

auto render_diff_line(const DiffLine& line) -> Element;
auto render_diff_lines(const std::vector<DiffLine>& lines) -> Element;

// File tree rendering
struct FileTree {
    struct Node {
        std::string name;
        bool is_directory;
        bool is_expanded;
        size_t depth;
        std::vector<Node> children;
    };

    static auto render_node(const Node& node, bool is_selected = false) -> Element;
    static auto render_tree(const std::vector<Node>& roots, size_t selected_index = 0) -> Element;
};

// Commit graph rendering (for log view)
struct CommitGraphNode {
    std::string commit_hash;
    std::vector<size_t> parents; // Indices of parent commits
    size_t branch_column;
    bool is_merge;
};

auto render_commit_graph(const std::vector<CommitGraphNode>& nodes, size_t height) -> Element;

// Branch visualization
auto render_branch_tree(const std::vector<std::string>& branches, const std::string& current_branch,
                        size_t selected_index = 0) -> Element;

} // namespace repo::tui::render
