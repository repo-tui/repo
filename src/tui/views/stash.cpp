#include <repo/tui/render/primitives.hpp>
#include <repo/tui/render/style.hpp>
#include <repo/tui/views/stash.hpp>

#include <ftxui/dom/elements.hpp>

#include <iomanip>
#include <sstream>

namespace repo::tui::views {

using namespace ftxui;
using namespace repo::tui::render;

// Helper: Format time ago
static auto format_time_ago(const std::chrono::system_clock::time_point& when) -> std::string {
    auto now = std::chrono::system_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - when).count();

    if (diff < 60) {
        return std::to_string(diff) + " seconds ago";
    } else if (diff < 3600) {
        return std::to_string(diff / 60) + " minutes ago";
    } else if (diff < 86400) {
        return std::to_string(diff / 3600) + " hours ago";
    } else {
        return std::to_string(diff / 86400) + " days ago";
    }
}

// Render the stash view
auto render_stash(const models::StashModel& model) -> Element {
    // Show help screen if requested
    if (model.mode == models::StashMode::Help) {
        return render_stash_help_screen();
    }

    // Show details view if requested
    if (model.mode == models::StashMode::Details) {
        return render_stash_details(model);
    }

    // Show dialogs if in special mode
    if (model.mode == models::StashMode::CreateInput) {
        return vbox(
            {render_stash_list(model) | flex, separator(), render_create_stash_dialog(model)});
    }

    if (model.mode == models::StashMode::ApplyConfirm) {
        return vbox(
            {render_stash_list(model) | flex, separator(), render_apply_confirmation(model)});
    }

    if (model.mode == models::StashMode::PopConfirm) {
        return vbox({render_stash_list(model) | flex, separator(), render_pop_confirmation(model)});
    }

    if (model.mode == models::StashMode::DropConfirm) {
        return vbox(
            {render_stash_list(model) | flex, separator(), render_drop_confirmation(model)});
    }

    // Normal mode - show stash list
    std::vector<Element> elements;

    // Title
    elements.push_back(text("Stashes") | bold | color(Style::color_scheme().highlight) | center);
    elements.push_back(separator());

    // Stash count
    auto count_info = hbox({text("Stashes: ") | bold, text(std::to_string(model.stashes.size()))});
    elements.push_back(count_info);
    elements.push_back(separator());

    // Stash list
    if (model.is_loading) {
        elements.push_back(loading("Loading stashes...") | flex | center);
    } else if (model.error) {
        elements.push_back(error_display("Error", model.error->message) | flex | center);
    } else if (model.stashes.empty()) {
        elements.push_back(empty_state("No stashes", "Press 'c' to create a stash") | flex |
                           center);
    } else {
        elements.push_back(render_stash_list(model) | flex);
    }

    // Status bar at bottom
    elements.push_back(separator());
    elements.push_back(render_stash_status_bar(model));

    // Notification banner (if any)
    if (model.notification) {
        return vbox({notification(*model.notification, Style::color_scheme().info, true),
                     vbox(std::move(elements))});
    }

    return vbox(std::move(elements));
}

// Helper: Render stash list
auto render_stash_list(const models::StashModel& model) -> Element {
    std::vector<Element> rows;

    for (size_t i = 0; i < model.stashes.size(); ++i) {
        bool is_selected = (i == model.selected_index);
        rows.push_back(render_stash_item(model.stashes[i], is_selected));
    }

    return vbox(std::move(rows));
}

// Helper: Render a single stash item
auto render_stash_item(const domain::Stash& stash, bool is_selected) -> Element {
    // Stash identifier and message
    auto elem =
        vbox({hbox({text("stash@{" + std::to_string(stash.index) + "}") | bold |
                        color(Style::color_scheme().highlight),
                    text("  "), text(stash.message)}),
              hbox({text("  "), text(stash.author.name) | color(Style::color_scheme().dimmed),
                    text(" · "),
                    text(format_time_ago(stash.when)) | color(Style::color_scheme().dimmed)})});

    // Highlight if selected
    if (is_selected) {
        elem = elem | bgcolor(Style::color_scheme().selected_bg) |
               color(Style::color_scheme().selected);
    }

    return elem;
}

// Helper: Render create stash dialog
auto render_create_stash_dialog(const models::StashModel& model) -> Element {
    // Show cursor in input
    std::string display_text = model.input_message;
    if (model.input_cursor_pos <= display_text.size()) {
        display_text.insert(model.input_cursor_pos, "|");
    }

    return vbox({text("Create Stash") | bold | color(Style::color_scheme().highlight), text(""),
                 hbox({text("Message: ") | bold,
                       text(display_text.empty() ? "(enter stash message)" : display_text)}),
                 text(""),
                 hbox({text("[") | dim, text(model.include_untracked ? "x" : " "), text("] ") | dim,
                       text("u: Include untracked files") | dim}),
                 hbox({text("[") | dim, text(model.keep_index ? "x" : " "), text("] ") | dim,
                       text("i: Keep index (staged changes)") | dim}),
                 text(""),
                 hbox({text("Enter: ") | bold, text("create  "), text("Esc: ") | bold,
                       text("cancel")}) |
                     dim}) |
           border | borderStyled(ROUNDED);
}

// Helper: Render apply confirmation dialog
auto render_apply_confirmation(const models::StashModel& model) -> Element {
    std::string stash_name =
        model.pending_operation_stash
            ? "stash@{" + std::to_string(model.pending_operation_stash->index) + "}"
            : "";

    std::string message =
        model.pending_operation_stash ? model.pending_operation_stash->message : "";

    return vbox({text("Apply Stash") | bold | color(Style::color_scheme().info), text(""),
                 hbox({text("Apply: "),
                       text(stash_name) | bold | color(Style::color_scheme().highlight)}),
                 hbox({text("Message: "), text(message) | dim}), text(""),
                 hbox({text("[") | dim, text(model.reinstate_index ? "x" : " "), text("] ") | dim,
                       text("i: Reinstate index (restore staged changes)") | dim}),
                 text(""), text("The stash will remain in the stash list.") | dim, text(""),
                 hbox({text("Y: ") | bold, text("confirm  "), text("N/Esc: ") | bold,
                       text("cancel")}) |
                     dim}) |
           border | borderStyled(ROUNDED);
}

// Helper: Render pop confirmation dialog
auto render_pop_confirmation(const models::StashModel& model) -> Element {
    std::string stash_name =
        model.pending_operation_stash
            ? "stash@{" + std::to_string(model.pending_operation_stash->index) + "}"
            : "";

    std::string message =
        model.pending_operation_stash ? model.pending_operation_stash->message : "";

    return vbox({text("Pop Stash") | bold | color(Style::color_scheme().info), text(""),
                 hbox({text("Pop: "),
                       text(stash_name) | bold | color(Style::color_scheme().highlight)}),
                 hbox({text("Message: "), text(message) | dim}), text(""),
                 hbox({text("[") | dim, text(model.reinstate_index ? "x" : " "), text("] ") | dim,
                       text("i: Reinstate index (restore staged changes)") | dim}),
                 text(""), text("The stash will be applied and removed from the stash list.") | dim,
                 text(""),
                 hbox({text("Y: ") | bold, text("confirm  "), text("N/Esc: ") | bold,
                       text("cancel")}) |
                     dim}) |
           border | borderStyled(ROUNDED);
}

// Helper: Render drop confirmation dialog
auto render_drop_confirmation(const models::StashModel& model) -> Element {
    std::string stash_name =
        model.pending_operation_stash
            ? "stash@{" + std::to_string(model.pending_operation_stash->index) + "}"
            : "";

    std::string message =
        model.pending_operation_stash ? model.pending_operation_stash->message : "";

    return vbox({text("Drop Stash") | bold | color(Style::color_scheme().error), text(""),
                 hbox({text("Drop: "),
                       text(stash_name) | bold | color(Style::color_scheme().warning)}),
                 hbox({text("Message: "), text(message) | dim}), text(""),
                 text("This action cannot be undone.") | dim, text(""),
                 hbox({text("Y: ") | bold, text("confirm  "), text("N/Esc: ") | bold,
                       text("cancel")}) |
                     dim}) |
           border | borderStyled(HEAVY) | color(Style::color_scheme().error);
}

// Helper: Render stash details view
auto render_stash_details(const models::StashModel& model) -> Element {
    if (!model.detail_stash) {
        return text("No stash details") | center;
    }

    const auto& stash = *model.detail_stash;

    std::vector<Element> elements;

    // Header
    elements.push_back(text("Stash Details") | bold | color(Style::color_scheme().highlight) |
                       center);
    elements.push_back(separator());

    // Stash info
    elements.push_back(
        hbox({text("Stash: ") | bold, text("stash@{" + std::to_string(stash.index) + "}") |
                                          color(Style::color_scheme().highlight)}));

    elements.push_back(hbox({text("Message: ") | bold, text(stash.message)}));

    elements.push_back(
        hbox({text("Author: ") | bold, text(stash.author.name + " <" + stash.author.email + ">")}));

    elements.push_back(hbox({text("When: ") | bold, text(format_time_ago(stash.when))}));

    elements.push_back(
        hbox({text("Commit ID: ") | bold, text(stash.commit_id.to_string().substr(0, 7)) |
                                              color(Style::color_scheme().dimmed)}));

    elements.push_back(separator());

    // Diff (if loaded)
    if (model.detail_diff && !model.detail_diff->empty()) {
        elements.push_back(text("Changes:") | bold);
        elements.push_back(text(*model.detail_diff) | dim);
    } else {
        elements.push_back(text("(Diff view not yet implemented)") | dim | center);
    }

    elements.push_back(separator());
    elements.push_back(text("Press any key to close") | center | dim);

    return vbox(std::move(elements));
}

// Helper: Render help screen
auto render_stash_help_screen() -> Element {
    return vbox({text("Stash View - Keyboard Shortcuts") | bold | center |
                     color(Style::color_scheme().highlight),
                 text(""),

                 text("Navigation:") | bold,
                 hbox({text("  j/↓"), filler(), text("Move down")}),
                 hbox({text("  k/↑"), filler(), text("Move up")}),
                 hbox({text("  g/Home"), filler(), text("Jump to top")}),
                 hbox({text("  G/End"), filler(), text("Jump to bottom")}),
                 text(""),

                 text("Stash Operations:") | bold,
                 hbox({text("  c"), filler(), text("Create new stash")}),
                 hbox({text("  a"), filler(), text("Apply stash (keep in list)")}),
                 hbox({text("  p"), filler(), text("Pop stash (apply and remove)")}),
                 hbox({text("  d"), filler(), text("Drop stash (remove without applying)")}),
                 hbox({text("  Enter/v"), filler(), text("View stash details")}),
                 text(""),

                 text("View:") | bold,
                 hbox({text("  R"), filler(), text("Refresh stash list")}),
                 hbox({text("  ?"), filler(), text("Toggle help")}),
                 text(""),

                 text("Press any key to close help") | center | dim}) |
           border | borderStyled(HEAVY) | center;
}

// Helper: Render status bar
auto render_stash_status_bar(const models::StashModel& /* model */) -> Element {
    std::vector<KeyHint> hints = {{"↑↓", "navigate"}, {"c", "create"}, {"a", "apply"},
                                  {"p", "pop"},       {"d", "drop"},   {"Enter", "details"},
                                  {"?", "help"}};

    return key_hints(hints);
}

} // namespace repo::tui::views
