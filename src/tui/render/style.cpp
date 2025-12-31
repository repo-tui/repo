#include <repo/tui/render/style.hpp>

#include <ftxui/dom/elements.hpp>

namespace repo::tui::render {

using namespace ftxui;

// Global color scheme (default theme for now)
static ColorScheme g_color_scheme = themes::default_theme();

auto Style::color_scheme() -> const ColorScheme& {
    return g_color_scheme;
}

auto Style::colored(const std::string& text, Color c) -> Element {
    return ftxui::text(text) | color(c);
}

auto Style::dimmed(const std::string& text) -> Element {
    return ftxui::text(text) | color(g_color_scheme.dimmed);
}

auto Style::bold(const std::string& text) -> Element {
    return ftxui::text(text) | ftxui::bold;
}

auto Style::italic(const std::string& text) -> Element {
    return ftxui::text(text) | ftxui::dim; // FTXUI doesn't have italic, use dim
}

auto Style::underline(const std::string& text) -> Element {
    return ftxui::text(text) | ftxui::underlined;
}

auto Style::success(const std::string& text) -> Element {
    return ftxui::text(text) | color(g_color_scheme.success);
}

auto Style::warning(const std::string& text) -> Element {
    return ftxui::text(text) | color(g_color_scheme.warning);
}

auto Style::error(const std::string& text) -> Element {
    return ftxui::text(text) | color(g_color_scheme.error);
}

auto Style::info(const std::string& text) -> Element {
    return ftxui::text(text) | color(g_color_scheme.info);
}

auto Style::file_status_icon(const std::string& status) -> Element {
    if (status == "modified" || status == "M") {
        return ftxui::text("M") | color(g_color_scheme.modified);
    } else if (status == "added" || status == "A") {
        return ftxui::text("A") | color(g_color_scheme.added);
    } else if (status == "deleted" || status == "D") {
        return ftxui::text("D") | color(g_color_scheme.deleted);
    } else if (status == "renamed" || status == "R") {
        return ftxui::text("R") | color(g_color_scheme.renamed);
    } else if (status == "untracked" || status == "?") {
        return ftxui::text("?") | color(g_color_scheme.untracked);
    } else if (status == "conflicted" || status == "U") {
        return ftxui::text("U") | color(g_color_scheme.conflicted);
    }
    return ftxui::text(" ");
}

auto Style::file_status_color(const std::string& status) -> Color {
    if (status == "modified" || status == "M") {
        return g_color_scheme.modified;
    } else if (status == "added" || status == "A") {
        return g_color_scheme.added;
    } else if (status == "deleted" || status == "D") {
        return g_color_scheme.deleted;
    } else if (status == "renamed" || status == "R") {
        return g_color_scheme.renamed;
    } else if (status == "untracked" || status == "?") {
        return g_color_scheme.untracked;
    } else if (status == "conflicted" || status == "U") {
        return g_color_scheme.conflicted;
    }
    return g_color_scheme.foreground;
}

auto Style::branch_icon(bool is_current, bool is_remote) -> Element {
    if (is_current) {
        return ftxui::text("* ") | color(g_color_scheme.branch_current);
    } else if (is_remote) {
        return ftxui::text("⎇ ") | color(g_color_scheme.branch_remote);
    } else {
        return ftxui::text("  ") | color(g_color_scheme.branch_local);
    }
}

auto Style::commit_hash(const std::string& hash, bool short_form) -> Element {
    std::string display_hash = short_form && hash.length() > 7 ? hash.substr(0, 7) : hash;
    return ftxui::text(display_hash) | color(g_color_scheme.commit_hash);
}

namespace boxes {

auto titled_box(const std::string& title, Element content) -> Element {
    return window(ftxui::text(title), content);
}

auto bordered_box(Element content) -> Element {
    return content | border;
}

auto selected_box(Element content) -> Element {
    return content | border | bgcolor(g_color_scheme.selected_bg);
}

auto dimmed_box(Element content) -> Element {
    return content | color(g_color_scheme.dimmed);
}

} // namespace boxes

namespace layout {

auto hsplit(Element left, Element right, int separator_width) -> Element {
    if (separator_width > 0) {
        return hbox({left | flex, separator(), right | flex});
    }
    return hbox({left | flex, right | flex});
}

auto vsplit(Element top, Element bottom, int separator_height) -> Element {
    if (separator_height > 0) {
        return vbox({top | flex, separator(), bottom | flex});
    }
    return vbox({top | flex, bottom | flex});
}

auto padded(Element content, int padding) -> Element {
    if (padding <= 0)
        return content;

    std::vector<Element> vertical;
    for (int i = 0; i < padding; ++i) {
        vertical.push_back(ftxui::text(""));
    }
    vertical.push_back(content);
    for (int i = 0; i < padding; ++i) {
        vertical.push_back(ftxui::text(""));
    }

    return vbox(std::move(vertical));
}

auto centered(Element content) -> Element {
    return content | center;
}

} // namespace layout

} // namespace repo::tui::render
