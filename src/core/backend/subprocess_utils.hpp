#pragma once

#include <repo/result.hpp>

#include <string>
#include <vector>

namespace repo::backend {

/// Result of subprocess execution
struct SubprocessResult {
    std::string stdout_output;
    std::string stderr_output;
    int exit_code;

    [[nodiscard]] auto success() const -> bool { return exit_code == 0; }
};

/// Execute a command with input and capture output
/// Cross-platform subprocess execution using popen()
///
/// @param command Full command to execute (will be passed to shell)
/// @param input Data to write to stdin
/// @param timeout_ms Timeout in milliseconds (0 = no timeout)
/// @return Result with stdout/stderr and exit code
[[nodiscard]] auto run_subprocess(const std::string& command, const std::string& input = "",
                                  int timeout_ms = 5000) -> Result<SubprocessResult>;

/// Find a binary on the system
/// Checks common locations and PATH
/// @param name Binary name (e.g., "git", "curl")
/// @return Path to executable, or error if not found
[[nodiscard]] auto find_binary(const std::string& name) -> Result<std::string>;

/// Find the git binary on the system
/// Checks common locations and PATH
/// @return Path to git executable, or error if not found
[[nodiscard]] auto find_git_binary() -> Result<std::string>;

/// Check if a command is available in PATH
/// @param command Command name (e.g., "git")
/// @return true if command exists and is executable
[[nodiscard]] auto is_command_available(const std::string& command) -> bool;

} // namespace repo::backend
