#include <repo/tui/render/primitives.hpp>
#include <repo/tui/render/style.hpp>
#include <repo/tui/views/command.hpp>

#include <ftxui/dom/elements.hpp>

namespace repo::tui::views {

using namespace ftxui;
using namespace repo::tui::render;

// Render command input line
auto render_command_input(const models::CommandModel& model) -> Element {
    if (model.mode == models::CommandMode::Inactive) {
        // Not in command mode - return empty
        return text("");
    }

    // Show cursor in input
    std::string display_text = model.input_text;
    if (model.input_cursor_pos <= display_text.size()) {
        display_text.insert(model.input_cursor_pos, "|");
    }

    // Command prompt
    Element prompt_elem =
        hbox({text(":") | bold | color(Style::color_scheme().highlight), text(" "),
              text(display_text.empty() ? "(enter command)" : display_text)});

    // Add executing indicator if running
    if (model.is_executing) {
        prompt_elem =
            hbox({std::move(prompt_elem), text("  "), text("[executing...]") | dim | blink});
    }

    // Show error if present
    if (model.error) {
        return vbox({prompt_elem,
                     text("Error: " + model.error->message) | color(Style::color_scheme().error)});
    }

    // Show result message if present
    if (model.result_message && !model.result_message->empty()) {
        return vbox({prompt_elem, text(*model.result_message) | color(Style::color_scheme().info)});
    }

    return prompt_elem;
}

// Render command suggestions
auto render_command_suggestions(const models::CommandModel& model) -> Element {
    if (model.mode != models::CommandMode::Input || model.suggestions.empty()) {
        return text("");
    }

    std::vector<Element> suggestion_elements;

    for (size_t i = 0; i < std::min(model.suggestions.size(), size_t(5)); ++i) {
        auto elem = text(model.suggestions[i]);

        if (i == model.selected_suggestion) {
            elem = elem | bgcolor(Style::color_scheme().selected_bg) |
                   color(Style::color_scheme().selected);
        }

        suggestion_elements.push_back(elem);
    }

    if (suggestion_elements.empty()) {
        return text("");
    }

    return vbox({text("Suggestions:") | dim,
                 vbox(std::move(suggestion_elements)) | border | borderStyled(LIGHT)});
}

// Render command result
auto render_command_result(const models::CommandModel& model) -> Element {
    if (!model.result_message || model.result_message->empty()) {
        return text("");
    }

    // Split result into lines
    std::vector<Element> lines;
    std::istringstream stream(*model.result_message);
    std::string line;

    while (std::getline(stream, line)) {
        lines.push_back(text(line));
    }

    return vbox({text("Command Result:") | bold, separator(), vbox(std::move(lines)), separator(),
                 text("Press Esc to dismiss") | dim | center}) |
           border;
}

} // namespace repo::tui::views
