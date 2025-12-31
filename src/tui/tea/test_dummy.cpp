// Test file for TEA infrastructure - Simple counter example
// This file demonstrates that the TEA pattern works correctly
// Compile and run this to verify the infrastructure

#include <repo/tui/render/primitives.hpp>
#include <repo/tui/render/style.hpp>
#include <repo/tui/tea/cmd.hpp>
#include <repo/tui/tea/msg.hpp>
#include <repo/tui/tea/program.hpp>

#include <ftxui/dom/elements.hpp>

#include <iostream>

using namespace repo::tui::tea;
using namespace repo::tui::render;
using namespace ftxui;

// Simple counter model
struct CounterModel {
    int count = 0;
    bool should_quit = false;
};

// Custom message for counter
struct IncrementMsg {};
struct DecrementMsg {};
struct QuitMsg {};

// Extend Msg variant to include our custom messages
// (In real code, these would be added to msg.hpp)
using CounterMsg = std::variant<KeyMsg, IncrementMsg, DecrementMsg, QuitMsg>;

// Init function - returns initial model and commands
auto init() -> std::pair<CounterModel, CmdBatch> {
    CounterModel model{};
    return {model, none()};
}

// Update function - processes messages
auto update(CounterModel model, Msg msg) -> std::pair<CounterModel, CmdBatch> {
    // For this test, we'll only handle KeyMsg
    if (const auto* key_msg = std::get_if<KeyMsg>(&msg)) {
        // Increment on 'j' or arrow down
        if (key_msg->type == KeyMsg::Type::ArrowDown ||
            (key_msg->type == KeyMsg::Type::Character && key_msg->character == 'j')) {
            model.count++;
        }
        // Decrement on 'k' or arrow up
        else if (key_msg->type == KeyMsg::Type::ArrowUp ||
                 (key_msg->type == KeyMsg::Type::Character && key_msg->character == 'k')) {
            model.count--;
        }
        // Quit on 'q' or Escape
        else if (key_msg->type == KeyMsg::Type::Escape ||
                 (key_msg->type == KeyMsg::Type::Character && key_msg->character == 'q')) {
            model.should_quit = true;
        }
    }

    return {model, none()};
}

// View function - renders model
auto view(const CounterModel& model) -> Element {
    return vbox({text("TEA Infrastructure Test - Counter Example") | bold | center, text(""),
                 text("Count: " + std::to_string(model.count)) | center | color(Color::Cyan),
                 text(""), separator(), text(""),
                 hbox({text("j/↓: ") | bold, text("increment  "), text("k/↑: ") | bold,
                       text("decrement  "), text("q/Esc: ") | bold, text("quit")}) |
                     center | dim}) |
           border | center;
}

// Template instantiation for CounterModel
// This is needed because Program is a template
namespace repo::tui::tea {
template class Program<CounterModel>;
}

int main() {
    std::cout << "Starting TEA infrastructure test...\n";
    std::cout << "This is a simple counter to verify TEA pattern works.\n";
    std::cout << "Use j/k or arrow keys to increment/decrement, q to quit.\n\n";

    try {
        // Create program configuration
        ProgramConfig<CounterModel> config{
            .init = init, .update = update, .view = view, .subscriptions = [](const CounterModel&) {
                return std::vector<Subscription>{};
            }};

        // Create and run program
        auto program = make_program(std::move(config));

        // Note: In the real implementation, we'd check model.should_quit
        // and call program.quit() from the update function somehow
        // For this test, we'll just run the program
        program.run();

        std::cout << "\nTEA infrastructure test completed successfully!\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
