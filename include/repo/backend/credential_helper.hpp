#pragma once

#include "credential.hpp"
#include "../result.hpp"

#include <string>
#include <unordered_map>

namespace repo::backend {

/// Git credential helper integration
/// Implements the git credential helper protocol for secure credential storage
/// See: https://git-scm.com/docs/gitcredentials
class CredentialHelper {
  public:
    /// Parsed URL components for credential lookup
    struct URLComponents {
        std::string protocol; // e.g., "https" or "ssh"
        std::string host;     // e.g., "github.com"
        std::string port;     // e.g., "443" (empty if default)
        std::string path;     // e.g., "/user/repo.git" (optional, controlled by credential.useHttpPath)
        std::string username; // e.g., "git" (from URL if present)
    };

    CredentialHelper() = default;

    /// Request credentials from configured git credential helpers
    /// Implements: git credential fill
    ///
    /// @param url Full URL (e.g., "https://github.com/user/repo.git")
    /// @param username_hint Username from URL or previous attempt (optional)
    /// @return Credential with username and password/token
    [[nodiscard]] auto fill(const std::string& url,
                           const std::string& username_hint = "") -> Result<Credential>;

    /// Store successful credentials
    /// Implements: git credential approve
    ///
    /// @param url URL the credentials were used for
    /// @param cred Credentials that worked
    /// @return Success or error
    [[nodiscard]] auto approve(const std::string& url, const Credential& cred) -> Status;

    /// Remove invalid credentials
    /// Implements: git credential reject
    ///
    /// @param url URL the credentials failed for
    /// @return Success or error
    [[nodiscard]] auto reject(const std::string& url) -> Status;

    /// Check if git credential helper is available
    /// @return true if git is installed and credential.helper is configured
    [[nodiscard]] auto is_available() const -> bool;

    /// Parse URL into components for credential protocol
    /// @param url Full URL string
    /// @return Parsed components
    [[nodiscard]] static auto parse_url(const std::string& url) -> Result<URLComponents>;

    /// Parse credential helper output (key=value format)
    /// @param output Raw output from git credential helper
    /// @return Map of key-value pairs
    [[nodiscard]] static auto parse_credential_output(const std::string& output)
        -> std::unordered_map<std::string, std::string>;

    /// Build credential input for git credential protocol
    /// @param components URL components
    /// @param include_credentials Whether to include username/password
    /// @param cred Credential to include (if include_credentials is true)
    /// @return Formatted input string
    [[nodiscard]] static auto build_credential_input(const URLComponents& components,
                                                     bool include_credentials = false,
                                                     const Credential* cred = nullptr)
        -> std::string;

  private:
    /// Invoke git credential with given operation
    /// @param operation "fill", "approve", or "reject"
    /// @param components URL components to send to helper
    /// @param include_credentials Whether to include username/password in input
    /// @param cred Credentials to include (if include_credentials is true)
    /// @return Output from credential helper (for "fill") or empty (for "approve"/"reject")
    [[nodiscard]] auto invoke_credential_helper(
        const std::string& operation,
        const URLComponents& components,
        bool include_credentials = false,
        const Credential* cred = nullptr) -> Result<std::string>;
};

} // namespace repo::backend
