#pragma once

#include <string>
#include <vector>

#include "../error.hpp"

namespace repo::backend {

/// Helper for creating user-friendly authentication error messages
/// Provides context-aware suggestions based on the error scenario
class AuthErrorFormatter {
  public:
    /// Authentication context for generating helpful error messages
    struct Context {
        std::string url;       // Remote URL (e.g., "https://github.com/user/repo.git")
        bool is_ssh = false;   // Whether URL is SSH
        bool is_https = false; // Whether URL is HTTPS
        bool credential_helper_available = false; // Whether git credential helper is configured
        bool ssh_keys_found = false;              // Whether SSH keys exist in ~/.ssh/
        bool is_github = false;                   // Whether provider is GitHub
        bool is_gitlab = false;                   // Whether provider is GitLab
        int attempt_count = 0;                    // Number of authentication attempts
    };

    /// Create error for failed authentication with context-aware help
    /// @param context Authentication context
    /// @param reason Specific reason for failure (optional)
    /// @return Error with helpful message and suggestions
    [[nodiscard]] static auto authentication_failed(const Context& context,
                                                    const std::string& reason = "") -> Error;

    /// Create error for credential required (no credentials available)
    /// @param context Authentication context
    /// @return Error with setup instructions
    [[nodiscard]] static auto credential_required(const Context& context) -> Error;

    /// Create error for SSH key issues
    /// @param reason Specific issue (e.g., "No keys found", "All keys encrypted")
    /// @param context Authentication context
    /// @return Error with SSH-specific help
    [[nodiscard]] static auto ssh_key_error(const std::string& reason, const Context& context)
        -> Error;

    /// Create error for credential helper issues
    /// @param reason Specific issue
    /// @param context Authentication context
    /// @return Error with credential helper setup instructions
    [[nodiscard]] static auto credential_helper_error(const std::string& reason,
                                                      const Context& context) -> Error;

    /// Create error for OAuth issues
    /// @param reason Specific issue
    /// @param context Authentication context
    /// @return Error with OAuth-specific help
    [[nodiscard]] static auto oauth_error(const std::string& reason, const Context& context)
        -> Error;

    /// Create error for max authentication attempts reached
    /// @param context Authentication context
    /// @return Error explaining retry limit
    [[nodiscard]] static auto max_attempts_reached(const Context& context) -> Error;

  private:
    /// Generate suggestions based on context
    static auto generate_suggestions(const Context& context) -> std::vector<std::string>;

    /// Format suggestions as numbered list
    static auto format_suggestions(const std::vector<std::string>& suggestions) -> std::string;

    /// Get platform-specific instructions for credential helper
    static auto get_credential_helper_instructions() -> std::string;

    /// Get SSH setup instructions
    static auto get_ssh_setup_instructions(const Context& context) -> std::string;

    /// Get provider-specific OAuth instructions
    static auto get_oauth_instructions(const Context& context) -> std::string;
};

} // namespace repo::backend
