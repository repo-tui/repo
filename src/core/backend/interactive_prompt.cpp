#include <repo/backend/interactive_prompt.hpp>

#include <repo/error.hpp>

#include <iostream>
#include <termios.h>
#include <unistd.h>

namespace repo::backend {

auto InteractivePrompt::detect_mode() -> Mode {
    // Check if both stdin and stdout are terminals
    if (is_terminal_input() && is_terminal_output()) {
        // TODO: Add TUI detection by checking environment variables
        // For now, assume CLI if we're in a terminal
        return Mode::CLI;
    }

    return Mode::NonInteractive;
}

auto InteractivePrompt::prompt_for_credentials(const std::string& url,
                                               const std::string& username_hint)
    -> Result<Credential> {

    Mode mode = detect_mode();

    if (mode == Mode::TUI) {
        return std::unexpected(make_error(
            Error::Code::CredentialRequired,
            "Interactive authentication not available in TUI mode",
            "Cannot prompt for credentials in TUI mode.\n\n"
            "Options:\n"
            "  1. Configure credential helper:\n"
            "     git config --global credential.helper <helper>\n\n"
            "  2. Use git command directly:\n"
            "     git push --set-upstream origin <branch>\n\n"
            "  3. Switch to SSH:\n"
            "     git remote set-url origin git@github.com:user/repo.git"));
    }

    if (mode == Mode::NonInteractive) {
        return std::unexpected(make_error(
            Error::Code::CredentialRequired,
            "Interactive authentication not available (non-interactive mode)",
            "Cannot prompt for credentials when not running in a terminal.\n\n"
            "Options:\n"
            "  1. Configure credential helper:\n"
            "     git config --global credential.helper <helper>\n\n"
            "  2. Use SSH authentication:\n"
            "     git remote set-url origin git@github.com:user/repo.git"));
    }

    // CLI mode - we can prompt
    std::cout << "Authentication required for: " << url << "\n";
    std::cout << "\n";

    // Prompt for username
    std::string username;
    if (!username_hint.empty()) {
        username = username_hint;
        std::cout << "Username: " << username << " (from URL)\n";
    } else {
        auto username_result = prompt_username();
        if (!username_result) {
            return std::unexpected(username_result.error());
        }
        username = *username_result;
    }

    // Prompt for password/token
    std::cout << "Password/Token (input hidden): ";
    std::cout.flush();

    auto password_result = prompt_password("");
    if (!password_result) {
        return std::unexpected(password_result.error());
    }

    std::cout << "\n"; // New line after password input

    return Credential::user_password(std::move(username), std::move(*password_result));
}

auto InteractivePrompt::prompt_username(const std::string& prompt) -> Result<std::string> {
    if (!is_terminal_input()) {
        return std::unexpected(
            make_error(Error::Code::CredentialRequired, "Cannot prompt: not a terminal"));
    }

    std::cout << prompt;
    std::cout.flush();

    std::string username;
    if (!std::getline(std::cin, username)) {
        return std::unexpected(
            make_error(Error::Code::CredentialRequired, "Failed to read username"));
    }

    // Trim whitespace
    size_t start = username.find_first_not_of(" \t\n\r");
    size_t end = username.find_last_not_of(" \t\n\r");

    if (start == std::string::npos) {
        return std::unexpected(
            make_error(Error::Code::CredentialRequired, "Username cannot be empty"));
    }

    username = username.substr(start, end - start + 1);

    return username;
}

auto InteractivePrompt::prompt_password(const std::string& prompt) -> Result<std::string> {
    if (!is_terminal_input()) {
        return std::unexpected(
            make_error(Error::Code::CredentialRequired, "Cannot prompt: not a terminal"));
    }

    if (!prompt.empty()) {
        std::cout << prompt;
        std::cout.flush();
    }

    // Disable terminal echo
    set_terminal_echo(false);

    std::string password;
    bool read_success = static_cast<bool>(std::getline(std::cin, password));

    // Re-enable terminal echo
    set_terminal_echo(true);

    if (!read_success) {
        return std::unexpected(
            make_error(Error::Code::CredentialRequired, "Failed to read password"));
    }

    return password;
}

auto InteractivePrompt::is_terminal_input() -> bool {
    return isatty(STDIN_FILENO) != 0;
}

auto InteractivePrompt::is_terminal_output() -> bool {
    return isatty(STDOUT_FILENO) != 0;
}

auto InteractivePrompt::set_terminal_echo(bool enable) -> void {
    if (!is_terminal_input()) {
        return;
    }

    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);

    if (enable) {
        tty.c_lflag |= ECHO;
    } else {
        tty.c_lflag &= ~ECHO;
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}

} // namespace repo::backend
