#pragma once

#include <chrono>
#include <optional>
#include <string>

#include "../result.hpp"
#include "credential.hpp"

namespace repo::backend {

/// OAuth Device Flow implementation for GitHub and GitLab
///
/// Device flow is ideal for CLI tools because:
/// - No embedded browser needed
/// - User authenticates in their own browser
/// - Works in SSH sessions and remote environments
///
/// Flow:
/// 1. Request device code from provider
/// 2. Show user the verification URL and code
/// 3. Poll provider until user completes authentication
/// 4. Receive access token
///
/// References:
/// - GitHub:
/// https://docs.github.com/en/apps/oauth-apps/building-oauth-apps/authorizing-oauth-apps#device-flow
/// - GitLab: https://docs.gitlab.com/ee/api/oauth2.html#device-authorization-grant-flow
class OAuthDeviceFlow {
  public:
    /// OAuth provider type
    enum class Provider {
        GitHub,
        GitLab,
    };

    /// Device authorization response
    struct DeviceCode {
        std::string device_code;      // Device verification code (opaque)
        std::string user_code;        // User-friendly code to display
        std::string verification_uri; // URL where user enters code
        int expires_in;               // Seconds until device code expires
        int interval;                 // Seconds between polling requests
    };

    /// Token response
    struct AccessToken {
        std::string token;      // OAuth access token
        std::string token_type; // Usually "Bearer"
        std::string scope;      // Granted scopes
        std::optional<std::chrono::system_clock::time_point> expires_at;
    };

    /// Detect provider from Git remote URL
    /// Returns Provider if URL matches known pattern, nullopt otherwise
    ///
    /// Examples:
    ///   - https://github.com/user/repo.git -> GitHub
    ///   - git@github.com:user/repo.git -> GitHub
    ///   - https://gitlab.com/user/repo.git -> GitLab
    ///   - https://gitlab.example.com/user/repo.git -> GitLab
    [[nodiscard]] static auto detect_provider(const std::string& url) -> std::optional<Provider>;

    /// Request device code from provider
    /// Initiates the OAuth device flow by requesting a device code
    ///
    /// @param provider OAuth provider (GitHub or GitLab)
    /// @param scopes Requested OAuth scopes (e.g., "repo", "read_repository")
    /// @param gitlab_host Custom GitLab host (e.g., "gitlab.example.com")
    /// @return DeviceCode with verification URL and user code
    [[nodiscard]] static auto request_device_code(Provider provider, const std::string& scopes = "",
                                                  const std::string& gitlab_host = "gitlab.com")
        -> Result<DeviceCode>;

    /// Poll for access token
    /// Polls the provider until user completes authentication or timeout
    ///
    /// @param provider OAuth provider
    /// @param device_code Device code from request_device_code()
    /// @param timeout_seconds Maximum time to wait (0 = use device code expiry)
    /// @param gitlab_host Custom GitLab host
    /// @return AccessToken if user completed authentication
    [[nodiscard]] static auto poll_for_token(Provider provider, const DeviceCode& device_code,
                                             int timeout_seconds = 0,
                                             const std::string& gitlab_host = "gitlab.com")
        -> Result<AccessToken>;

    /// Complete OAuth device flow (request + poll)
    /// Combines request_device_code and poll_for_token into single operation
    /// Displays instructions to user and waits for authentication
    ///
    /// @param provider OAuth provider
    /// @param url Git remote URL (for display context)
    /// @param scopes Requested OAuth scopes
    /// @param gitlab_host Custom GitLab host
    /// @return Credential with OAuth token
    [[nodiscard]] static auto authenticate(Provider provider, const std::string& url,
                                           const std::string& scopes = "",
                                           const std::string& gitlab_host = "gitlab.com")
        -> Result<Credential>;

    /// Get default scopes for Git operations
    /// Returns appropriate scopes for read/write Git access
    [[nodiscard]] static auto get_default_scopes(Provider provider) -> std::string;

    /// Parse JSON field from response (public for testing)
    /// Simple JSON parsing - look for "field":"value" pattern
    [[nodiscard]] static auto parse_json_field(const std::string& json, const std::string& field)
        -> std::optional<std::string>;

    /// Parse JSON integer field from response (public for testing)
    [[nodiscard]] static auto parse_json_int_field(const std::string& json,
                                                   const std::string& field) -> std::optional<int>;

    /// Get OAuth API endpoints for provider (public for testing)
    [[nodiscard]] static auto get_device_code_endpoint(Provider provider,
                                                       const std::string& gitlab_host)
        -> std::string;

    [[nodiscard]] static auto get_token_endpoint(Provider provider, const std::string& gitlab_host)
        -> std::string;

  private:
    /// Make HTTP request to OAuth API
    /// Simple HTTP client for OAuth endpoints (no external dependencies)
    static auto http_post(const std::string& url, const std::string& body,
                          const std::string& content_type = "application/json")
        -> Result<std::string>;

    /// Get client ID for provider
    /// Note: These are public client IDs for the repo CLI tool
    /// They can be embedded in the source code (OAuth spec allows this for CLI tools)
    static auto get_client_id(Provider provider) -> std::string;
};

} // namespace repo::backend
