#include <repo/error.hpp>
#include <repo/tui/tea/program.hpp>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <chrono>

namespace repo::tui::tea {

using namespace ftxui;

template <typename Model>
Program<Model>::Program(ProgramConfig<Model> config)
    : config_(std::move(config)), screen_(ScreenInteractive::Fullscreen()) {
    // Initialize model and execute init commands
    auto [initial_model, init_cmds] = config_.init();
    model_ = std::move(initial_model);

    // Execute init commands
    if (!init_cmds.empty()) {
        execute_commands(std::move(init_cmds));
    }

    // Setup FTXUI component
    component_ = Renderer([this] { return render(); });

    // Add event handling
    component_ = CatchEvent(component_, [this](Event event) { return handle_event(event); });
}

template <typename Model> Program<Model>::~Program() {
    stop_subscriptions();

    // Wait for command threads to finish
    for (auto& thread : command_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

template <typename Model> auto Program<Model>::run() -> void {
    running_ = true;

    // Start subscription management
    update_subscriptions();

    // Start message processing thread
    std::thread message_thread([this] {
        while (running_) {
            std::unique_lock<std::mutex> lock(queue_mutex_);

            // Wait for messages or shutdown
            queue_cv_.wait(lock, [this] { return !message_queue_.empty() || !running_; });

            if (!running_)
                break;

            // Process all pending messages
            while (!message_queue_.empty()) {
                auto msg = std::move(message_queue_.front());
                message_queue_.pop();

                lock.unlock();
                process_message(std::move(msg));
                lock.lock();
            }
        }
    });

    // Run FTXUI event loop (blocks until quit)
    screen_.Loop(component_);

    // Cleanup
    running_ = false;
    queue_cv_.notify_all();

    if (message_thread.joinable()) {
        message_thread.join();
    }

    stop_subscriptions();
}

template <typename Model> auto Program<Model>::send(Msg msg) -> void {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        message_queue_.push(std::move(msg));
    }
    queue_cv_.notify_one();
}

template <typename Model> auto Program<Model>::quit() -> void {
    running_ = false;
    screen_.Exit();
}

template <typename Model> auto Program<Model>::process_message(Msg msg) -> void {
    // Call update function
    auto [new_model, cmds] = config_.update(std::move(model_), std::move(msg));

    // Update model
    model_ = std::move(new_model);

    // Check if model wants to quit (using C++20 concepts for optional field detection)
    if constexpr (requires { model_.should_quit; }) {
        if (model_.should_quit) {
            quit();
            return;
        }
    }

    // Execute commands
    if (!cmds.empty()) {
        execute_commands(std::move(cmds));
    }

    // Update subscriptions if they changed
    update_subscriptions();

    // Trigger re-render
    needs_render_ = true;
    screen_.PostEvent(Event::Custom);
}

template <typename Model> auto Program<Model>::execute_commands(CmdBatch cmds) -> void {
    for (auto& cmd : cmds) {
        execute_command(std::move(cmd));
    }
}

template <typename Model> auto Program<Model>::execute_command(Cmd cmd) -> void {
    // Execute command on a background thread
    command_threads_.emplace_back([this, cmd = std::move(cmd)]() mutable {
        try {
            auto result = cmd();
            if (result) {
                send(std::move(*result));
            }
        } catch (const std::exception& e) {
            // Send error message
            send(ErrorMsg{make_error(Error::Code::Unknown, "Command execution failed", e.what()),
                          "async command"});
        }

        // Clean up finished threads periodically
        // (This is a simplification - real implementation would use a thread pool)
    });

    // Detach thread (or manage with a thread pool in production)
    command_threads_.back().detach();
}

template <typename Model> auto Program<Model>::update_subscriptions() -> void {
    auto new_subs = config_.subscriptions(model_);

    // Stop subscriptions that are no longer active
    for (auto& sub : active_subscriptions_) {
        auto found = std::find_if(new_subs.begin(), new_subs.end(),
                                  [&](const auto& s) { return s.id == sub.id; });
        if (found == new_subs.end()) {
            // Subscription removed - cleanup would happen here
            // (Simplified - real implementation would track thread handles)
        }
    }

    // Start new subscriptions
    for (auto& sub : new_subs) {
        auto found = std::find_if(active_subscriptions_.begin(), active_subscriptions_.end(),
                                  [&](const auto& s) { return s.id == sub.id; });
        if (found == active_subscriptions_.end()) {
            // New subscription - start it
            subscription_threads_.emplace_back(
                [this, sub]() { sub.run([this](Msg msg) { send(std::move(msg)); }); });
        }
    }

    active_subscriptions_ = std::move(new_subs);
}

template <typename Model> auto Program<Model>::stop_subscriptions() -> void {
    active_subscriptions_.clear();

    // Join subscription threads
    for (auto& thread : subscription_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    subscription_threads_.clear();
}

template <typename Model> auto Program<Model>::handle_event(Event event) -> bool {
    // Convert FTXUI event to KeyMsg
    auto key_msg = event_to_key_msg(event);
    if (key_msg) {
        send(std::move(*key_msg));
        return true;
    }

    // Handle mouse events
    if (event.is_mouse()) {
        MouseMsg mouse{event.mouse().button == ftxui::Mouse::Left     ? MouseMsg::Button::Left
                       : event.mouse().button == ftxui::Mouse::Right  ? MouseMsg::Button::Right
                       : event.mouse().button == ftxui::Mouse::Middle ? MouseMsg::Button::Middle
                       : event.mouse().button == ftxui::Mouse::WheelUp
                           ? MouseMsg::Button::WheelUp
                           : MouseMsg::Button::WheelDown,

                       event.mouse().motion == ftxui::Mouse::Pressed    ? MouseMsg::Action::Press
                       : event.mouse().motion == ftxui::Mouse::Released ? MouseMsg::Action::Release
                                                                        : MouseMsg::Action::Move,

                       event.mouse().x, event.mouse().y};
        send(std::move(mouse));
        return true;
    }

    // Handle window resize
    if (event == Event::Special({0})) {
        // Window resize event
        // (FTXUI handles this automatically, but we could track it)
        return false;
    }

    return false;
}

template <typename Model> auto Program<Model>::render() -> Element {
    if (!needs_render_) {
        return text("");
    }

    needs_render_ = false;
    return config_.view(model_);
}

// Helper: Convert FTXUI event to KeyMsg
auto event_to_key_msg(const Event& event) -> std::optional<KeyMsg> {
    if (event.is_character()) {
        return KeyMsg::from_char(event.character()[0]);
    }

    if (event == Event::ArrowUp)
        return KeyMsg::arrow_up();
    if (event == Event::ArrowDown)
        return KeyMsg::arrow_down();
    if (event == Event::ArrowLeft)
        return KeyMsg::arrow_left();
    if (event == Event::ArrowRight)
        return KeyMsg::arrow_right();
    if (event == Event::Return)
        return KeyMsg::enter();
    if (event == Event::Escape)
        return KeyMsg::escape();
    if (event == Event::Tab)
        return KeyMsg::tab();
    if (event == Event::Backspace)
        return KeyMsg::backspace();
    if (event == Event::Delete)
        return KeyMsg::del();
    if (event == Event::Home)
        return KeyMsg{KeyMsg::Type::Home};
    if (event == Event::End)
        return KeyMsg{KeyMsg::Type::End};
    if (event == Event::PageUp)
        return KeyMsg{KeyMsg::Type::PageUp};
    if (event == Event::PageDown)
        return KeyMsg{KeyMsg::Type::PageDown};

    // F-keys
    if (event == Event::F1)
        return KeyMsg{KeyMsg::Type::F1};
    if (event == Event::F2)
        return KeyMsg{KeyMsg::Type::F2};
    if (event == Event::F3)
        return KeyMsg{KeyMsg::Type::F3};
    if (event == Event::F4)
        return KeyMsg{KeyMsg::Type::F4};
    if (event == Event::F5)
        return KeyMsg{KeyMsg::Type::F5};
    if (event == Event::F6)
        return KeyMsg{KeyMsg::Type::F6};
    if (event == Event::F7)
        return KeyMsg{KeyMsg::Type::F7};
    if (event == Event::F8)
        return KeyMsg{KeyMsg::Type::F8};
    if (event == Event::F9)
        return KeyMsg{KeyMsg::Type::F9};
    if (event == Event::F10)
        return KeyMsg{KeyMsg::Type::F10};
    if (event == Event::F11)
        return KeyMsg{KeyMsg::Type::F11};
    if (event == Event::F12)
        return KeyMsg{KeyMsg::Type::F12};

    // Ctrl+key combinations
    if (event == Event::CtrlC) {
        auto msg = KeyMsg::from_char('c');
        msg.ctrl = true;
        return msg;
    }

    return std::nullopt;
}

// Helper: Create a timer subscription
auto timer_subscription(std::string id, std::chrono::milliseconds interval,
                        std::function<Msg()> tick) -> Subscription {
    return Subscription{std::move(id),
                        [interval, tick = std::move(tick)](std::function<void(Msg)> send) {
                            while (true) {
                                std::this_thread::sleep_for(interval);
                                send(tick());
                            }
                        }};
}

// Helper: Create a file watcher subscription
auto file_watcher_subscription(std::string id, std::filesystem::path path,
                               std::function<Msg(std::filesystem::path)> on_change)
    -> Subscription {
    return Subscription{std::move(id),
                        [path, on_change = std::move(on_change)](std::function<void(Msg)> send) {
                            auto last_write_time = std::filesystem::last_write_time(path);

                            while (true) {
                                std::this_thread::sleep_for(std::chrono::milliseconds(500));

                                try {
                                    auto current_time = std::filesystem::last_write_time(path);
                                    if (current_time != last_write_time) {
                                        last_write_time = current_time;
                                        send(on_change(path));
                                    }
                                } catch (...) {
                                    // File might not exist or be inaccessible
                                }
                            }
                        }};
}

} // namespace repo::tui::tea

// Explicit template instantiations for concrete Model types
#include <repo/tui/models/app.hpp>

template class repo::tui::tea::Program<repo::tui::models::AppModel>;
