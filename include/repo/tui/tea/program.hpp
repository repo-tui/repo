#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

#include "cmd.hpp"
#include "msg.hpp"

namespace repo::tui::tea {

// Forward declaration
template <typename Model> class Program;

// Init function - creates initial model and commands
template <typename Model> using InitFn = std::function<std::pair<Model, CmdBatch>()>;

// Update function - processes messages and returns new model + commands
template <typename Model> using UpdateFn = std::function<std::pair<Model, CmdBatch>(Model, Msg)>;

// View function - renders model to FTXUI element
template <typename Model> using ViewFn = std::function<ftxui::Element(const Model&)>;

// Subscription - represents an ongoing effect (timers, file watchers, etc.)
struct Subscription {
    std::string id;
    std::function<void(std::function<void(Msg)>)> run;

    auto operator==(const Subscription& other) const -> bool { return id == other.id; }
};

// Subscriptions function - returns active subscriptions based on model
template <typename Model>
using SubscriptionsFn = std::function<std::vector<Subscription>(const Model&)>;

// Program configuration
template <typename Model> struct ProgramConfig {
    InitFn<Model> init;
    UpdateFn<Model> update;
    ViewFn<Model> view;
    SubscriptionsFn<Model> subscriptions = [](const Model&) { return std::vector<Subscription>{}; };
};

// The TEA Program - manages the event loop and state
template <typename Model> class Program {
  public:
    explicit Program(ProgramConfig<Model> config);
    ~Program();

    // Run the program (blocks until quit)
    auto run() -> void;

    // Send a message to the program (thread-safe)
    auto send(Msg msg) -> void;

    // Quit the program
    auto quit() -> void;

  private:
    // Process a message through update function
    auto process_message(Msg msg) -> void;

    // Execute a command batch asynchronously
    auto execute_commands(CmdBatch cmds) -> void;

    // Execute a single command
    auto execute_command(Cmd cmd) -> void;

    // Update subscriptions based on new model
    auto update_subscriptions() -> void;

    // Stop all subscriptions
    auto stop_subscriptions() -> void;

    // FTXUI event handling
    auto handle_event(ftxui::Event event) -> bool;

    // Render current model
    auto render() -> ftxui::Element;

    ProgramConfig<Model> config_;
    Model model_;

    // Message queue (thread-safe)
    std::queue<Msg> message_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    // Command execution
    std::vector<std::thread> command_threads_;
    std::atomic<bool> running_{false};

    // Active subscriptions
    std::vector<Subscription> active_subscriptions_;
    std::vector<std::thread> subscription_threads_;

    // FTXUI integration
    ftxui::ScreenInteractive screen_;
    ftxui::Component component_;

    // Flag to trigger re-render
    std::atomic<bool> needs_render_{true};
};

// Factory function for creating programs
template <typename Model> auto make_program(ProgramConfig<Model> config) -> Program<Model> {
    return Program<Model>(std::move(config));
}

// Helper: Convert FTXUI event to KeyMsg
auto event_to_key_msg(const ftxui::Event& event) -> std::optional<KeyMsg>;

// Helper: Create a timer subscription
auto timer_subscription(std::string id, std::chrono::milliseconds interval,
                        std::function<Msg()> tick) -> Subscription;

// Helper: Create a file watcher subscription
auto file_watcher_subscription(std::string id, std::filesystem::path path,
                               std::function<Msg(std::filesystem::path)> on_change) -> Subscription;

} // namespace repo::tui::tea
