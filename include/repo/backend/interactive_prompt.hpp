#pragma once

#include <string>

#include "../result.hpp"
#include "credential.hpp"

namespace repo::backend {

/// Interactive credential prompting
/// Provides secure password input and mode detection for CLI vs TUI
class InteractivePrompt {
  public:
    /// Prompt mode detection
    enum class Mode {
        CLI,           // Running in CLI mode (can prompt user)
        TUI,           // Running in TUI mode (cannot easily prompt)
        NonInteractive // Running non-interactively (e.g., in script)
    };

    /// Detect current execution mode
    /// Checks if stdin/stdout are connected to a terminal
    [[nodiscard]] static auto detect_mode() -> Mode;

    /// Prompt user for credentials
    /// Only works in CLI mode - returns error in TUI/non-interactive mode
    ///
    /// @param url URL being accessed (for display)
    /// @param username_hint Username from URL or previous attempt (optional)
    /// @return Credential with username and password
    [[nodiscard]] static auto prompt_for_credentials(const std::string& url,
                                                     const std::string& username_hint = "")
        -> Result<Credential>;

    /// Prompt for username
    /// Echoes input to terminal
    ///
    /// @param prompt Prompt text (e.g., "Username: ")
    /// @return Username entered by user
    [[nodiscard]] static auto prompt_username(const std::string& prompt = "Username: ")
        -> Result<std::string>;

    /// Prompt for password/token
    /// Does NOT echo input to terminal (secure input)
    ///
    /// @param prompt Prompt text (e.g., "Password: ")
    /// @return Password or token entered by user
    [[nodiscard]] static auto prompt_password(const std::string& prompt = "Password: ")
        -> Result<std::string>;

    /// Check if stdin is connected to a terminal
    [[nodiscard]] static auto is_terminal_input() -> bool;

    /// Check if stdout is connected to a terminal
    [[nodiscard]] static auto is_terminal_output() -> bool;

  private:
    /// Enable/disable terminal echo (for secure password input)
    static auto set_terminal_echo(bool enable) -> void;
};

} // namespace repo::backend
