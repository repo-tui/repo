#pragma once

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <string>

namespace repo::tui::render {

using namespace ftxui;

// Color scheme for the TUI
struct ColorScheme {
    // Basic colors
    Color foreground;
    Color background;
    Color border;

    // Status colors
    Color success;
    Color warning;
    Color error;
    Color info;

    // Git status colors
    Color added;
    Color modified;
    Color deleted;
    Color renamed;
    Color untracked;
    Color conflicted;

    // UI element colors
    Color selected;
    Color selected_bg;
    Color dimmed;
    Color highlight;
    Color cursor;

    // Diff colors
    Color diff_added;
    Color diff_removed;
    Color diff_hunk;
    Color diff_context;

    // Branch colors
    Color branch_local;
    Color branch_remote;
    Color branch_current;

    // Commit colors
    Color commit_hash;
    Color commit_author;
    Color commit_date;
    Color commit_message;
};

// Predefined themes
namespace themes {

// Default theme (based on terminal colors)
inline auto default_theme() -> ColorScheme {
    return ColorScheme{
        .foreground = Color::White,
        .background = Color::Black,
        .border = Color::GrayDark,

        .success = Color::Green,
        .warning = Color::Yellow,
        .error = Color::Red,
        .info = Color::Blue,

        .added = Color::Green,
        .modified = Color::Yellow,
        .deleted = Color::Red,
        .renamed = Color::Blue,
        .untracked = Color::Cyan,
        .conflicted = Color::RedLight,

        .selected = Color::White,
        .selected_bg = Color::Blue,
        .dimmed = Color::GrayDark,
        .highlight = Color::Cyan,
        .cursor = Color::Yellow,

        .diff_added = Color::Green,
        .diff_removed = Color::Red,
        .diff_hunk = Color::Cyan,
        .diff_context = Color::White,

        .branch_local = Color::Green,
        .branch_remote = Color::Red,
        .branch_current = Color::Cyan,

        .commit_hash = Color::Yellow,
        .commit_author = Color::Green,
        .commit_date = Color::Blue,
        .commit_message = Color::White,
    };
}

// Dracula theme
inline auto dracula_theme() -> ColorScheme {
    return ColorScheme{
        .foreground = Color::RGB(248, 248, 242),
        .background = Color::RGB(40, 42, 54),
        .border = Color::RGB(68, 71, 90),

        .success = Color::RGB(80, 250, 123),
        .warning = Color::RGB(241, 250, 140),
        .error = Color::RGB(255, 85, 85),
        .info = Color::RGB(139, 233, 253),

        .added = Color::RGB(80, 250, 123),
        .modified = Color::RGB(241, 250, 140),
        .deleted = Color::RGB(255, 85, 85),
        .renamed = Color::RGB(189, 147, 249),
        .untracked = Color::RGB(139, 233, 253),
        .conflicted = Color::RGB(255, 121, 198),

        .selected = Color::RGB(248, 248, 242),
        .selected_bg = Color::RGB(68, 71, 90),
        .dimmed = Color::RGB(98, 114, 164),
        .highlight = Color::RGB(139, 233, 253),
        .cursor = Color::RGB(255, 121, 198),

        .diff_added = Color::RGB(80, 250, 123),
        .diff_removed = Color::RGB(255, 85, 85),
        .diff_hunk = Color::RGB(139, 233, 253),
        .diff_context = Color::RGB(248, 248, 242),

        .branch_local = Color::RGB(80, 250, 123),
        .branch_remote = Color::RGB(255, 121, 198),
        .branch_current = Color::RGB(139, 233, 253),

        .commit_hash = Color::RGB(241, 250, 140),
        .commit_author = Color::RGB(80, 250, 123),
        .commit_date = Color::RGB(189, 147, 249),
        .commit_message = Color::RGB(248, 248, 242),
    };
}

// Gruvbox theme
inline auto gruvbox_theme() -> ColorScheme {
    return ColorScheme{
        .foreground = Color::RGB(235, 219, 178),
        .background = Color::RGB(40, 40, 40),
        .border = Color::RGB(80, 73, 69),

        .success = Color::RGB(184, 187, 38),
        .warning = Color::RGB(250, 189, 47),
        .error = Color::RGB(251, 73, 52),
        .info = Color::RGB(131, 165, 152),

        .added = Color::RGB(184, 187, 38),
        .modified = Color::RGB(250, 189, 47),
        .deleted = Color::RGB(251, 73, 52),
        .renamed = Color::RGB(211, 134, 155),
        .untracked = Color::RGB(142, 192, 124),
        .conflicted = Color::RGB(254, 128, 25),

        .selected = Color::RGB(235, 219, 178),
        .selected_bg = Color::RGB(60, 56, 54),
        .dimmed = Color::RGB(146, 131, 116),
        .highlight = Color::RGB(131, 165, 152),
        .cursor = Color::RGB(250, 189, 47),

        .diff_added = Color::RGB(184, 187, 38),
        .diff_removed = Color::RGB(251, 73, 52),
        .diff_hunk = Color::RGB(131, 165, 152),
        .diff_context = Color::RGB(235, 219, 178),

        .branch_local = Color::RGB(184, 187, 38),
        .branch_remote = Color::RGB(251, 73, 52),
        .branch_current = Color::RGB(131, 165, 152),

        .commit_hash = Color::RGB(250, 189, 47),
        .commit_author = Color::RGB(184, 187, 38),
        .commit_date = Color::RGB(211, 134, 155),
        .commit_message = Color::RGB(235, 219, 178),
    };
}

} // namespace themes

// Style helpers
struct Style {
    // Get current color scheme (from config)
    static auto color_scheme() -> const ColorScheme&;

    // Apply colored style to text
    static auto colored(const std::string& text, Color color) -> Element;

    // Apply dimmed style
    static auto dimmed(const std::string& text) -> Element;

    // Apply bold style
    static auto bold(const std::string& text) -> Element;

    // Apply italic style
    static auto italic(const std::string& text) -> Element;

    // Apply underline style
    static auto underline(const std::string& text) -> Element;

    // Success/warning/error text
    static auto success(const std::string& text) -> Element;
    static auto warning(const std::string& text) -> Element;
    static auto error(const std::string& text) -> Element;
    static auto info(const std::string& text) -> Element;

    // Git status indicators
    static auto file_status_icon(const std::string& status) -> Element;
    static auto file_status_color(const std::string& status) -> Color;

    // Branch indicators
    static auto branch_icon(bool is_current, bool is_remote) -> Element;

    // Commit hash rendering
    static auto commit_hash(const std::string& hash, bool short_form = true) -> Element;
};

// Box styles
namespace boxes {

// Create a titled box
auto titled_box(const std::string& title, Element content) -> Element;

// Create a bordered box
auto bordered_box(Element content) -> Element;

// Create a selected/highlighted box
auto selected_box(Element content) -> Element;

// Create a dimmed box
auto dimmed_box(Element content) -> Element;

} // namespace boxes

// Layout helpers
namespace layout {

// Create a horizontal layout with separator
auto hsplit(Element left, Element right, int separator_width = 1) -> Element;

// Create a vertical layout with separator
auto vsplit(Element top, Element bottom, int separator_height = 1) -> Element;

// Create a padded element
auto padded(Element content, int padding = 1) -> Element;

// Create centered element
auto centered(Element content) -> Element;

} // namespace layout

} // namespace repo::tui::render
