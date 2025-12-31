#include <repo/tui/render/primitives.hpp>
#include <repo/tui/render/style.hpp>
#include <repo/tui/views/branch.hpp>

#include <ftxui/dom/elements.hpp>

namespace repo::tui::views {

using namespace ftxui;
using namespace repo::tui::render;

// Helper: Get filter name
static auto filter_name(models::BranchFilter filter) -> std::string {
    switch (filter) {
        case models::BranchFilter::All:
            return "All";
        case models::BranchFilter::Local:
            return "Local";
        case models::BranchFilter::Remote:
            return "Remote";
    }
    return "Unknown";
}

// Render the branch view
auto render_branch(const models::BranchModel& model) -> Element {
    // Show help screen if requested
    if (model.mode == models::BranchMode::Help) {
        return render_branch_help_screen();
    }

    // Show dialogs if in special mode
    if (model.mode == models::BranchMode::CreateInput) {
        return vbox(
            {render_branch_list(model) | flex, separator(), render_create_branch_dialog(model)});
    }

    if (model.mode == models::BranchMode::RenameInput) {
        return vbox(
            {render_branch_list(model) | flex, separator(), render_rename_branch_dialog(model)});
    }

    if (model.mode == models::BranchMode::DeleteConfirm) {
        return vbox(
            {render_branch_list(model) | flex, separator(), render_delete_confirmation(model)});
    }

    if (model.mode == models::BranchMode::MergeConfirm) {
        return vbox(
            {render_branch_list(model) | flex, separator(), render_merge_confirmation(model)});
    }

    // Normal mode - show branch list
    std::vector<Element> elements;

    // Title
    elements.push_back(text("Branches") | bold | color(Style::color_scheme().highlight) | center);
    elements.push_back(separator());

    // Filter info
    auto filter_info =
        hbox({text("Filter: ") | bold,
              text(filter_name(model.filter)) | color(Style::color_scheme().info), text("  "),
              text("Branches: ") | bold, text(std::to_string(model.filtered_branches.size()))});
    elements.push_back(filter_info);
    elements.push_back(separator());

    // Branch list
    if (model.is_loading) {
        elements.push_back(loading("Loading branches...") | flex | center);
    } else if (model.error) {
        elements.push_back(error_display("Error", model.error->message) | flex | center);
    } else if (model.filtered_branches.empty()) {
        std::string empty_msg;
        switch (model.filter) {
            case models::BranchFilter::All:
                empty_msg = "No branches found";
                break;
            case models::BranchFilter::Local:
                empty_msg = "No local branches";
                break;
            case models::BranchFilter::Remote:
                empty_msg = "No remote branches";
                break;
        }
        elements.push_back(empty_state(empty_msg, "Press 'f' to toggle filter") | flex | center);
    } else {
        elements.push_back(render_branch_list(model) | flex);
    }

    // Status bar at bottom
    elements.push_back(separator());
    elements.push_back(render_branch_status_bar(model));

    // Notification banner (if any)
    if (model.notification) {
        return vbox({notification(*model.notification, Style::color_scheme().info, true),
                     vbox(std::move(elements))});
    }

    return vbox(std::move(elements));
}

// Helper: Render branch list
auto render_branch_list(const models::BranchModel& model) -> Element {
    std::vector<Element> rows;

    for (size_t i = 0; i < model.filtered_branches.size(); ++i) {
        bool is_selected = (i == model.selected_index);
        bool is_current =
            model.current_branch && model.filtered_branches[i].name == *model.current_branch;
        rows.push_back(render_branch_item(model.filtered_branches[i], is_selected, is_current));
    }

    return vbox(std::move(rows));
}

// Helper: Render a single branch item
auto render_branch_item(const domain::Branch& branch, bool is_selected, bool is_current)
    -> Element {
    // Branch indicator
    std::string indicator;
    Color indicator_color;

    if (is_current) {
        indicator = "* ";
        indicator_color = Style::color_scheme().branch_current;
    } else if (branch.is_remote) {
        indicator = "⎇ ";
        indicator_color = Style::color_scheme().branch_remote;
    } else {
        indicator = "  ";
        indicator_color = Style::color_scheme().branch_local;
    }

    // Build branch row
    auto elem = hbox({text(indicator) | color(indicator_color), text(branch.name)});

    // Highlight if selected
    if (is_selected) {
        elem = elem | bgcolor(Style::color_scheme().selected_bg) |
               color(Style::color_scheme().selected);
    }

    return elem;
}

// Helper: Render create branch dialog
auto render_create_branch_dialog(const models::BranchModel& model) -> Element {
    // Show cursor in input
    std::string display_text = model.input_text;
    if (model.input_cursor_pos <= display_text.size()) {
        display_text.insert(model.input_cursor_pos, "|");
    }

    return vbox({text("Create New Branch") | bold | color(Style::color_scheme().highlight),
                 text(""), text(display_text.empty() ? "(enter branch name)" : display_text),
                 text(""),
                 hbox({text("Enter: ") | bold, text("create and switch  "), text("Esc: ") | bold,
                       text("cancel")}) |
                     dim}) |
           border | borderStyled(ROUNDED);
}

// Helper: Render rename branch dialog
auto render_rename_branch_dialog(const models::BranchModel& model) -> Element {
    // Show cursor in input
    std::string display_text = model.input_text;
    if (model.input_cursor_pos <= display_text.size()) {
        display_text.insert(model.input_cursor_pos, "|");
    }

    std::string old_name = model.pending_delete_branch ? model.pending_delete_branch->name : "";

    return vbox({text("Rename Branch") | bold | color(Style::color_scheme().highlight), text(""),
                 hbox({text("Old name: ") | bold,
                       text(old_name) | color(Style::color_scheme().dimmed)}),
                 hbox({text("New name: ") | bold, text(display_text)}), text(""),
                 hbox({text("Enter: ") | bold, text("rename  "), text("Esc: ") | bold,
                       text("cancel")}) |
                     dim}) |
           border | borderStyled(ROUNDED);
}

// Helper: Render delete confirmation dialog
auto render_delete_confirmation(const models::BranchModel& model) -> Element {
    std::string branch_name = model.pending_delete_branch ? model.pending_delete_branch->name : "";

    return vbox({text("Delete Branch") | bold | color(Style::color_scheme().error), text(""),
                 hbox({text("Delete branch: "),
                       text(branch_name) | bold | color(Style::color_scheme().warning)}),
                 text(""), text("This action cannot be undone.") | dim, text(""),
                 hbox({text("Y: ") | bold, text("confirm  "), text("N/Esc: ") | bold,
                       text("cancel")}) |
                     dim}) |
           border | borderStyled(HEAVY) | color(Style::color_scheme().error);
}

// Helper: Render merge confirmation dialog
auto render_merge_confirmation(const models::BranchModel& model) -> Element {
    std::string branch_name = model.pending_merge_branch ? model.pending_merge_branch->name : "";

    std::string current = model.current_branch.value_or("(unknown)");

    return vbox({text("Merge Branch") | bold | color(Style::color_scheme().info), text(""),
                 hbox({text("Merge: "),
                       text(branch_name) | bold | color(Style::color_scheme().highlight)}),
                 hbox({text("Into: "),
                       text(current) | bold | color(Style::color_scheme().branch_current)}),
                 text(""), text("This will merge the branch into your current branch.") | dim,
                 text(""),
                 hbox({text("Y: ") | bold, text("confirm  "), text("N/Esc: ") | bold,
                       text("cancel")}) |
                     dim}) |
           border | borderStyled(ROUNDED);
}

// Helper: Render help screen
auto render_branch_help_screen() -> Element {
    return vbox({text("Branch View - Keyboard Shortcuts") | bold | center |
                     color(Style::color_scheme().highlight),
                 text(""),

                 text("Navigation:") | bold,
                 hbox({text("  j/↓"), filler(), text("Move down")}),
                 hbox({text("  k/↑"), filler(), text("Move up")}),
                 hbox({text("  g/Home"), filler(), text("Jump to top")}),
                 hbox({text("  G/End"), filler(), text("Jump to bottom")}),
                 text(""),

                 text("Branch Operations:") | bold,
                 hbox({text("  Enter/s"), filler(), text("Switch to branch")}),
                 hbox({text("  c"), filler(), text("Create new branch")}),
                 hbox({text("  d"), filler(), text("Delete branch")}),
                 hbox({text("  r"), filler(), text("Rename branch")}),
                 hbox({text("  m"), filler(), text("Merge branch into current")}),
                 text(""),

                 text("View:") | bold,
                 hbox({text("  f"), filler(), text("Cycle filter (all/local/remote)")}),
                 hbox({text("  R"), filler(), text("Refresh branch list")}),
                 hbox({text("  ?"), filler(), text("Toggle help")}),
                 text(""),

                 text("Press any key to close help") | center | dim}) |
           border | borderStyled(HEAVY) | center;
}

// Helper: Render status bar
auto render_branch_status_bar(const models::BranchModel& model) -> Element {
    std::vector<KeyHint> hints = {{"↑↓", "navigate"}, {"Enter", "switch"}, {"c", "create"},
                                  {"d", "delete"},    {"r", "rename"},     {"m", "merge"},
                                  {"f", "filter"},    {"?", "help"}};

    auto hint_elem = key_hints(hints);

    // Add current branch info
    if (model.current_branch) {
        auto current_info =
            hbox({text("Current: ") | bold,
                  text(*model.current_branch) | color(Style::color_scheme().branch_current),
                  text("  "), hint_elem});
        return current_info;
    }

    return hint_elem;
}

} // namespace repo::tui::views
