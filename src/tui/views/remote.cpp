#include <repo/tui/render/primitives.hpp>
#include <repo/tui/render/style.hpp>
#include <repo/tui/views/remote.hpp>

#include <ftxui/dom/elements.hpp>

namespace repo::tui::views {

using namespace ftxui;
using namespace repo::tui::render;

// Render the remote view
auto render_remote(const models::RemoteModel& model) -> Element {
    // Show help screen if requested
    if (model.mode == models::RemoteMode::Help) {
        return render_remote_help_screen();
    }

    // Show dialogs/progress if in special mode
    if (model.mode == models::RemoteMode::AddInput) {
        return vbox(
            {render_remote_list(model) | flex, separator(), render_add_remote_dialog(model)});
    }

    if (model.mode == models::RemoteMode::RemoveConfirm) {
        return vbox(
            {render_remote_list(model) | flex, separator(), render_remove_confirmation(model)});
    }

    if (model.mode == models::RemoteMode::FetchProgress) {
        return vbox({render_remote_list(model) | flex, separator(),
                     render_operation_progress(model, "Fetch")});
    }

    if (model.mode == models::RemoteMode::PushProgress) {
        return vbox({render_remote_list(model) | flex, separator(),
                     render_operation_progress(model, "Push")});
    }

    if (model.mode == models::RemoteMode::PullProgress) {
        return vbox({render_remote_list(model) | flex, separator(),
                     render_operation_progress(model, "Pull")});
    }

    // Normal mode - show remote list
    std::vector<Element> elements;

    // Title
    elements.push_back(text("Remotes") | bold | color(Style::color_scheme().highlight) | center);
    elements.push_back(separator());

    // Remote count
    auto count_info = hbox({text("Remotes: ") | bold, text(std::to_string(model.remotes.size()))});
    elements.push_back(count_info);
    elements.push_back(separator());

    // Remote list
    if (model.is_loading) {
        elements.push_back(loading("Loading remotes...") | flex | center);
    } else if (model.error) {
        elements.push_back(error_display("Error", model.error->message) | flex | center);
    } else if (model.remotes.empty()) {
        elements.push_back(empty_state("No remotes configured", "Press 'a' to add a remote") |
                           flex | center);
    } else {
        elements.push_back(render_remote_list(model) | flex);
    }

    // Status bar at bottom
    elements.push_back(separator());
    elements.push_back(render_remote_status_bar(model));

    // Notification banner (if any)
    if (model.notification) {
        return vbox({notification(*model.notification, Style::color_scheme().info, true),
                     vbox(std::move(elements))});
    }

    return vbox(std::move(elements));
}

// Helper: Render remote list
auto render_remote_list(const models::RemoteModel& model) -> Element {
    std::vector<Element> rows;

    for (size_t i = 0; i < model.remotes.size(); ++i) {
        bool is_selected = (i == model.selected_index);
        rows.push_back(render_remote_item(model.remotes[i], is_selected));
    }

    return vbox(std::move(rows));
}

// Helper: Render a single remote item
auto render_remote_item(const domain::Remote& remote, bool is_selected) -> Element {
    // Remote name and URL
    auto elem =
        vbox({hbox({text("● ") | color(Style::color_scheme().highlight), text(remote.name) | bold}),
              hbox({text("  URL: "), text(remote.url) | color(Style::color_scheme().dimmed)})});

    // Add push URL if different
    if (remote.push_url && *remote.push_url != remote.url) {
        elem = vbox({std::move(elem),
                     hbox({text("  Push URL: "),
                           text(*remote.push_url) | color(Style::color_scheme().dimmed)})});
    }

    // Highlight if selected
    if (is_selected) {
        elem = elem | bgcolor(Style::color_scheme().selected_bg) |
               color(Style::color_scheme().selected);
    }

    return elem;
}

// Helper: Render add remote dialog
auto render_add_remote_dialog(const models::RemoteModel& model) -> Element {
    std::string current_text;
    std::string prompt;
    bool is_name_step = (model.add_input_step == models::AddInputStep::Name);

    if (is_name_step) {
        current_text = model.input_name;
        prompt = "Remote name:";
    } else {
        current_text = model.input_url;
        prompt = "Remote URL:";
    }

    // Show cursor in input
    std::string display_text = current_text;
    if (model.input_cursor_pos <= display_text.size()) {
        display_text.insert(model.input_cursor_pos, "|");
    }

    std::vector<Element> dialog_elements;

    dialog_elements.push_back(text("Add Remote") | bold | color(Style::color_scheme().highlight));
    dialog_elements.push_back(text(""));

    // Name field (show both when on URL step)
    if (!is_name_step) {
        dialog_elements.push_back(
            hbox({text("Remote name: ") | bold,
                  text(model.input_name) | color(Style::color_scheme().dimmed)}));
    }

    // Current input field
    dialog_elements.push_back(hbox(
        {text(prompt) | bold, text("  "),
         text(display_text.empty() ? "(enter " + std::string(is_name_step ? "name" : "URL") + ")"
                                   : display_text)}));

    dialog_elements.push_back(text(""));

    // Instructions
    std::vector<Element> hints;
    if (is_name_step) {
        hints.push_back(hbox({text("Enter: ") | bold, text("next  ")}));
    } else {
        hints.push_back(hbox({text("Tab: ") | bold, text("back to name  ")}));
        hints.push_back(hbox({text("Enter: ") | bold, text("add remote  ")}));
    }
    hints.push_back(hbox({text("Esc: ") | bold, text("cancel")}));

    dialog_elements.push_back(hbox(std::move(hints)) | dim);

    return vbox(std::move(dialog_elements)) | border | borderStyled(ROUNDED);
}

// Helper: Render remove confirmation dialog
auto render_remove_confirmation(const models::RemoteModel& model) -> Element {
    std::string remote_name = model.pending_remove_remote ? model.pending_remove_remote->name : "";

    return vbox({text("Remove Remote") | bold | color(Style::color_scheme().error), text(""),
                 hbox({text("Remove remote: "),
                       text(remote_name) | bold | color(Style::color_scheme().warning)}),
                 text(""), text("This action cannot be undone.") | dim, text(""),
                 hbox({text("Y: ") | bold, text("confirm  "), text("N/Esc: ") | bold,
                       text("cancel")}) |
                     dim}) |
           border | borderStyled(HEAVY) | color(Style::color_scheme().error);
}

// Helper: Render operation progress
auto render_operation_progress(const models::RemoteModel& model, const std::string& operation)
    -> Element {
    std::string remote_name = model.operation_remote_name.value_or("unknown");

    std::vector<Element> progress_elements;

    progress_elements.push_back(text(operation) | bold | color(Style::color_scheme().info));
    progress_elements.push_back(text(""));

    progress_elements.push_back(hbox({text("Remote: "), text(remote_name) | bold}));

    progress_elements.push_back(text(""));

    // Progress info
    if (model.progress_total > 0) {
        float percent = (float)model.progress_received / (float)model.progress_total * 100.0f;
        progress_elements.push_back(
            hbox({text("Progress: "), text(std::to_string((int)percent) + "%")}));
        progress_elements.push_back(
            hbox({text("Objects: "), text(std::to_string(model.progress_received) + " / " +
                                          std::to_string(model.progress_total))}));
    } else {
        progress_elements.push_back(text(model.progress_phase));
    }

    progress_elements.push_back(text(""));
    progress_elements.push_back(text("Please wait...") | dim | center);

    return vbox(std::move(progress_elements)) | border | borderStyled(ROUNDED);
}

// Helper: Render help screen
auto render_remote_help_screen() -> Element {
    return vbox({text("Remote View - Keyboard Shortcuts") | bold | center |
                     color(Style::color_scheme().highlight),
                 text(""),

                 text("Navigation:") | bold,
                 hbox({text("  j/↓"), filler(), text("Move down")}),
                 hbox({text("  k/↑"), filler(), text("Move up")}),
                 hbox({text("  g/Home"), filler(), text("Jump to top")}),
                 hbox({text("  G/End"), filler(), text("Jump to bottom")}),
                 text(""),

                 text("Remote Operations:") | bold,
                 hbox({text("  a"), filler(), text("Add remote")}),
                 hbox({text("  r"), filler(), text("Remove remote")}),
                 hbox({text("  f"), filler(), text("Fetch from remote")}),
                 hbox({text("  p"), filler(), text("Push to remote")}),
                 hbox({text("  P"), filler(), text("Force push to remote")}),
                 hbox({text("  l"), filler(), text("Pull from remote")}),
                 text(""),

                 text("View:") | bold,
                 hbox({text("  R"), filler(), text("Refresh remote list")}),
                 hbox({text("  ?"), filler(), text("Toggle help")}),
                 text(""),

                 text("Press any key to close help") | center | dim}) |
           border | borderStyled(HEAVY) | center;
}

// Helper: Render status bar
auto render_remote_status_bar(const models::RemoteModel& /* model */) -> Element {
    std::vector<KeyHint> hints = {{"↑↓", "navigate"}, {"a", "add"},  {"r", "remove"},
                                  {"f", "fetch"},     {"p", "push"}, {"l", "pull"},
                                  {"?", "help"}};

    return key_hints(hints);
}

} // namespace repo::tui::views
