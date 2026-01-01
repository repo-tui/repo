#include "subprocess_utils.hpp"

#include <repo/error.hpp>

#include <array>
#include <sys/wait.h>
#include <unistd.h>

namespace repo::backend {

// Unix/macOS implementation using fork/exec with pipes
auto run_subprocess(const std::string& command, const std::string& input, int timeout_ms)
    -> Result<SubprocessResult> {
    (void)timeout_ms; // TODO: Implement timeout with alarm() or timerfd

    // Create pipes for stdin, stdout, stderr
    std::array<int, 2> stdin_pipe;
    std::array<int, 2> stdout_pipe;
    std::array<int, 2> stderr_pipe;

    if (pipe(stdin_pipe.data()) == -1 || pipe(stdout_pipe.data()) == -1 ||
        pipe(stderr_pipe.data()) == -1) {
        return std::unexpected(
            make_error(Error::Code::CredentialHelperError, "Failed to create pipes"));
    }

    pid_t pid = fork();

    if (pid == -1) {
        // Fork failed - close all pipes
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        return std::unexpected(
            make_error(Error::Code::CredentialHelperError, "Failed to fork process"));
    }

    if (pid == 0) {
        // Child process

        // Redirect stdin
        dup2(stdin_pipe[0], STDIN_FILENO);
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);

        // Redirect stdout
        dup2(stdout_pipe[1], STDOUT_FILENO);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);

        // Redirect stderr
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);

        // Execute command through shell
        execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);

        // If execl returns, it failed
        _exit(127);
    }

    // Parent process

    // Close unused pipe ends
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    // Write input to stdin
    if (!input.empty()) {
        ssize_t written = write(stdin_pipe[1], input.c_str(), input.size());
        if (written == -1) {
            close(stdin_pipe[1]);
            close(stdout_pipe[0]);
            close(stderr_pipe[0]);
            return std::unexpected(
                make_error(Error::Code::CredentialHelperError, "Failed to write to subprocess"));
        }
    }
    close(stdin_pipe[1]); // Signal EOF to child

    // Read stdout and stderr
    std::string stdout_output;
    std::string stderr_output;

    std::array<char, 4096> buffer;
    ssize_t bytes_read;

    // Read all stdout
    while ((bytes_read = read(stdout_pipe[0], buffer.data(), buffer.size())) > 0) {
        stdout_output.append(buffer.data(), static_cast<size_t>(bytes_read));
    }

    // Read all stderr
    while ((bytes_read = read(stderr_pipe[0], buffer.data(), buffer.size())) > 0) {
        stderr_output.append(buffer.data(), static_cast<size_t>(bytes_read));
    }

    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    // Wait for child process to complete
    int status;
    waitpid(pid, &status, 0);

    int exit_code = 0;
    if (WIFEXITED(status)) {
        exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        exit_code = -1; // Terminated by signal
    }

    SubprocessResult result;
    result.stdout_output = std::move(stdout_output);
    result.stderr_output = std::move(stderr_output);
    result.exit_code = exit_code;

    return result;
}

auto find_git_binary() -> Result<std::string> {
    // Try common Unix/macOS locations
    std::vector<std::string> paths = {"/usr/bin/git", "/usr/local/bin/git", "/opt/homebrew/bin/git",
                                      "git" // In PATH
    };

    for (const auto& path : paths) {
        if (is_command_available(path)) {
            return path;
        }
    }

    return std::unexpected(make_error(
        Error::Code::CredentialHelperError,
        "Git binary not found. Please ensure git is installed and in PATH.",
        "Git is required for credential helper integration.\n\n"
        "Install git:\n"
        "  macOS:  brew install git\n"
        "  Ubuntu: sudo apt-get install git\n"
        "  Fedora: sudo dnf install git"));
}

auto find_binary(const std::string& name) -> Result<std::string> {
    // Try using 'which' to find the binary in PATH
    std::string which_cmd = "which " + name + " 2>/dev/null";
    auto result = run_subprocess(which_cmd);

    if (result && result->exit_code == 0 && !result->stdout_output.empty()) {
        // Remove trailing newline
        std::string path = result->stdout_output;
        if (!path.empty() && path.back() == '\n') {
            path.pop_back();
        }
        return path;
    }

    // Not found
    return std::unexpected(make_error(
        Error::Code::ExternalCommandFailed, name + " not found",
        name + " is required but could not be found in PATH.\n\n"
               "Please install " +
            name + " and ensure it's in your PATH."));
}

auto is_command_available(const std::string& command) -> bool {
    std::string test_cmd = "which " + command + " >/dev/null 2>&1";
    int result = system(test_cmd.c_str());
    return result == 0;
}

} // namespace repo::backend
