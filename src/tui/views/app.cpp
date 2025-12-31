#include <repo/tui/render/primitives.hpp>
#include <repo/tui/render/style.hpp>
#include <repo/tui/views/app.hpp>
#include <repo/tui/views/branch.hpp>
#include <repo/tui/views/command.hpp>
#include <repo/tui/views/diff.hpp>
#include <repo/tui/views/log.hpp>
#include <repo/tui/views/remote.hpp>
#include <repo/tui/views/stash.hpp>
#include <repo/tui/views/status.hpp>

#include <ftxui/dom/elements.hpp>

namespace repo::tui::views {

using namespace ftxui;
using namespace repo::tui::render;

// Render tab bar
auto render_tab_bar(const models::AppModel& model) -> Element {
    using models::ActiveView;

    std::vector<Element> tabs;

    // Define tabs with their names and keys
    struct Tab {
        ActiveView view;
        std::string name;
        std::string key;
    };

    std::vector<Tab> tab_list = {
        {ActiveView::Status, "Status", "1"},  {ActiveView::Log, "Log", "2"},
        {ActiveView::Diff, "Diff", "3"},      {ActiveView::Branch, "Branches", "4"},
        {ActiveView::Remote, "Remotes", "5"}, {ActiveView::Stash, "Stashes", "6"}};

    for (size_t i = 0; i < tab_list.size(); ++i) {
        const auto& tab = tab_list[i];

        // Create tab element
        auto tab_elem =
            hbox({text(tab.name), text(" [") | dim, text(tab.key) | dim, text("]") | dim});

        // Highlight if active
        if (model.active_view == tab.view) {
            tab_elem = tab_elem | bold | bgcolor(Style::color_scheme().selected_bg) |
                       color(Style::color_scheme().selected);
        }

        tabs.push_back(tab_elem);

        // Add separator between tabs
        if (i < tab_list.size() - 1) {
            tabs.push_back(text(" | ") | dim);
        }
    }

    return hbox(std::move(tabs));
}

// Render current view content
auto render_current_view(const models::AppModel& model) -> Element {
    using models::ActiveView;

    switch (model.active_view) {
        case ActiveView::Status:
            return render_status(model.status);
        case ActiveView::Log:
            return render_log(model.log);
        case ActiveView::Diff:
            return render_diff(model.diff);
        case ActiveView::Branch:
            return render_branch(model.branch);
        case ActiveView::Remote:
            return render_remote(model.remote);
        case ActiveView::Stash:
            return render_stash(model.stash);
    }

    return text("Unknown view");
}

// Render global status bar
auto render_global_status_bar(const models::AppModel& model) -> Element {
    // Command mode takes priority in status bar
    if (model.command.mode != models::CommandMode::Inactive) {
        return render_command_input(model.command);
    }

    // Global keyboard hints
    std::vector<KeyHint> hints = {
        {"1-6", "switch view"}, {":", "command"}, {"?", "help"}, {"q", "quit"}};

    auto hints_elem = key_hints(hints);

    // Show global notification if present
    if (model.global_notification) {
        return hbox({text(*model.global_notification) | color(Style::color_scheme().info),
                     text("  "), hints_elem});
    }

    return hints_elem;
}

// Render the entire application
auto render_app(const models::AppModel& model) -> Element {
    std::vector<Element> elements;

    // Title bar
    elements.push_back(text("Repo - Modern Git Interface") | bold | center |
                       color(Style::color_scheme().highlight));
    elements.push_back(separator());

    // Tab bar
    elements.push_back(render_tab_bar(model));
    elements.push_back(separator());

    // Main content (current view)
    elements.push_back(render_current_view(model) | flex);

    // Status bar at bottom
    elements.push_back(separator());
    elements.push_back(render_global_status_bar(model));

    auto main_layout = vbox(std::move(elements));

    // Overlay global notification at top if present
    if (model.global_notification) {
        main_layout =
            vbox({notification(*model.global_notification, Style::color_scheme().info, true),
                  std::move(main_layout)});
    }

    // Overlay command suggestions if in command mode
    if (model.command.mode == models::CommandMode::Input && !model.command.suggestions.empty()) {
        main_layout = dbox({std::move(main_layout),
                            vbox({filler(), separator(), render_command_suggestions(model.command),
                                  filler() | size(HEIGHT, EQUAL, 3)})});
    }

    return main_layout;
}

} // namespace repo::tui::views
