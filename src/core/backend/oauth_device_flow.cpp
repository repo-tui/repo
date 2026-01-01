#include <repo/backend/oauth_device_flow.hpp>
#include <repo/backend/interactive_prompt.hpp>
#include <repo/error.hpp>

#include "subprocess_utils.hpp"

#include <fmt/format.h>
#include <iostream>
#include <thread>
#include <regex>

namespace repo::backend {

// Public client IDs for the repo CLI tool
// These are safe to embed in source code (OAuth spec allows this for native apps)
// Using GitHub CLI's OAuth app (publicly available for CLI tools)
constexpr const char* GITHUB_CLIENT_ID = "178c6fc778ccc68e1d6a"; // GitHub CLI OAuth app
constexpr const char* GITLAB_CLIENT_ID = "your-gitlab-client-id"; // Placeholder - register your own

auto OAuthDeviceFlow::detect_provider(const std::string& url) -> std::optional<Provider> {
    // GitHub patterns
    if (url.find("github.com") != std::string::npos) {
        return Provider::GitHub;
    }

    // GitLab patterns
    if (url.find("gitlab.com") != std::string::npos || url.find("gitlab.") != std::string::npos) {
        return Provider::GitLab;
    }

    return std::nullopt;
}

auto OAuthDeviceFlow::get_default_scopes(Provider provider) -> std::string {
    switch (provider) {
        case Provider::GitHub:
            // "repo" scope gives full control of private repositories (both personal and org)
            // "workflow" allows push to repositories with GitHub Actions
            // Note: "repo" includes access to organization repositories if user is a member
            return "repo workflow";

        case Provider::GitLab:
            // "write_repository" gives read/write access to repositories
            // "read_user" allows reading user info
            return "write_repository read_user";
    }

    return "";
}

auto OAuthDeviceFlow::get_client_id(Provider provider) -> std::string {
    switch (provider) {
        case Provider::GitHub:
            return GITHUB_CLIENT_ID;
        case Provider::GitLab:
            return GITLAB_CLIENT_ID;
    }
    return "";
}

auto OAuthDeviceFlow::get_device_code_endpoint(Provider provider,
                                                const std::string& gitlab_host) -> std::string {
    switch (provider) {
        case Provider::GitHub:
            return "https://github.com/login/device/code";
        case Provider::GitLab:
            return fmt::format("https://{}/oauth/authorize_device", gitlab_host);
    }
    return "";
}

auto OAuthDeviceFlow::get_token_endpoint(Provider provider, const std::string& gitlab_host)
    -> std::string {
    switch (provider) {
        case Provider::GitHub:
            return "https://github.com/login/oauth/access_token";
        case Provider::GitLab:
            return fmt::format("https://{}/oauth/token", gitlab_host);
    }
    return "";
}

auto OAuthDeviceFlow::http_post(const std::string& url, const std::string& body,
                                const std::string& content_type) -> Result<std::string> {

    // Use curl for HTTP requests
    // curl -X POST -H "Content-Type: ..." -H "Accept: application/json" -d "..." url

    auto curl_binary = find_binary("curl");
    if (!curl_binary) {
        return std::unexpected(make_error(Error::Code::ExternalCommandFailed,
                                         "curl not found",
                                         "OAuth device flow requires curl to be installed.\n\n"
                                         "Install curl:\n"
                                         "  macOS:  brew install curl (or use system curl)\n"
                                         "  Linux:  apt-get install curl / yum install curl"));
    }

    // Build curl command
    std::string command = fmt::format(
        "{} -X POST "
        "-H \"Content-Type: {}\" "
        "-H \"Accept: application/json\" "
        "-d '{}' "
        "\"{}\" 2>&1",
        *curl_binary, content_type, body, url);

    auto result = run_subprocess(command);
    if (!result) {
        return std::unexpected(result.error());
    }

    if (result->exit_code != 0) {
        return std::unexpected(make_error(
            Error::Code::NetworkUnreachable,
            "HTTP request failed",
            fmt::format("curl exited with code {}\nOutput: {}", result->exit_code,
                       result->stderr_output)));
    }

    return result->stdout_output;
}

auto OAuthDeviceFlow::parse_json_field(const std::string& json, const std::string& field)
    -> std::optional<std::string> {

    // Simple JSON parsing - look for "field":"value" pattern
    // This is sufficient for OAuth responses which have simple structure

    std::string pattern = fmt::format("\"{}\"\\s*:\\s*\"([^\"]+)\"", field);
    std::regex regex(pattern);
    std::smatch match;

    if (std::regex_search(json, match, regex)) {
        return match[1].str();
    }

    return std::nullopt;
}

auto OAuthDeviceFlow::parse_json_int_field(const std::string& json, const std::string& field)
    -> std::optional<int> {

    // Simple JSON parsing - look for "field":123 pattern
    std::string pattern = fmt::format("\"{}\"\\s*:\\s*(\\d+)", field);
    std::regex regex(pattern);
    std::smatch match;

    if (std::regex_search(json, match, regex)) {
        try {
            return std::stoi(match[1].str());
        } catch (...) {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

auto OAuthDeviceFlow::request_device_code(Provider provider, const std::string& scopes,
                                          const std::string& gitlab_host) -> Result<DeviceCode> {

    std::string client_id = get_client_id(provider);
    std::string endpoint = get_device_code_endpoint(provider, gitlab_host);
    std::string scope = scopes.empty() ? get_default_scopes(provider) : scopes;

    // Build request body
    std::string body;
    if (provider == Provider::GitHub) {
        body = fmt::format("client_id={}&scope={}", client_id, scope);
    } else {
        // GitLab uses JSON
        body = fmt::format("{{\"client_id\":\"{}\",\"scope\":\"{}\"}}", client_id, scope);
    }

    // Make request
    auto content_type =
        provider == Provider::GitHub ? "application/x-www-form-urlencoded" : "application/json";

    auto response = http_post(endpoint, body, content_type);
    if (!response) {
        return std::unexpected(response.error());
    }

    // Parse response
    auto device_code = parse_json_field(*response, "device_code");
    auto user_code = parse_json_field(*response, "user_code");
    auto verification_uri = parse_json_field(*response, "verification_uri");
    auto expires_in = parse_json_int_field(*response, "expires_in");
    auto interval = parse_json_int_field(*response, "interval");

    if (!device_code || !user_code || !verification_uri || !expires_in || !interval) {
        return std::unexpected(make_error(
            Error::Code::AuthenticationFailed, "Failed to parse device code response",
            "Response: " + *response));
    }

    return DeviceCode{
        .device_code = *device_code,
        .user_code = *user_code,
        .verification_uri = *verification_uri,
        .expires_in = *expires_in,
        .interval = *interval,
    };
}

auto OAuthDeviceFlow::poll_for_token(Provider provider, const DeviceCode& device_code,
                                     int timeout_seconds, const std::string& gitlab_host)
    -> Result<AccessToken> {

    std::string client_id = get_client_id(provider);
    std::string endpoint = get_token_endpoint(provider, gitlab_host);

    // Calculate timeout
    int timeout = timeout_seconds > 0 ? timeout_seconds : device_code.expires_in;
    auto start_time = std::chrono::steady_clock::now();
    auto timeout_time = start_time + std::chrono::seconds(timeout);

    // Poll interval
    int interval = device_code.interval;
    if (interval < 5) {
        interval = 5; // Minimum 5 seconds to avoid rate limiting
    }

    while (std::chrono::steady_clock::now() < timeout_time) {
        // Build request body
        std::string body;
        if (provider == Provider::GitHub) {
            body = fmt::format("client_id={}&device_code={}&grant_type=urn:ietf:params:oauth:grant-type:device_code",
                              client_id, device_code.device_code);
        } else {
            body = fmt::format(
                "{{\"client_id\":\"{}\",\"device_code\":\"{}\",\"grant_type\":\"urn:ietf:params:oauth:grant-type:device_code\"}}",
                client_id, device_code.device_code);
        }

        // Make request
        auto content_type = provider == Provider::GitHub ? "application/x-www-form-urlencoded"
                                                          : "application/json";

        auto response = http_post(endpoint, body, content_type);
        if (!response) {
            // Network error - retry
            std::this_thread::sleep_for(std::chrono::seconds(interval));
            continue;
        }

        // Check for errors
        auto error = parse_json_field(*response, "error");
        if (error) {
            if (*error == "authorization_pending") {
                // User hasn't completed authentication yet - keep polling
                std::this_thread::sleep_for(std::chrono::seconds(interval));
                continue;
            } else if (*error == "slow_down") {
                // We're polling too fast - increase interval
                interval += 5;
                std::this_thread::sleep_for(std::chrono::seconds(interval));
                continue;
            } else if (*error == "expired_token") {
                return std::unexpected(
                    make_error(Error::Code::AuthenticationFailed,
                              "Device code expired",
                              "The device code expired before authentication was completed.\n"
                              "Please try again."));
            } else if (*error == "access_denied") {
                return std::unexpected(
                    make_error(Error::Code::AuthenticationFailed, "Access denied",
                              "User denied authorization."));
            } else {
                return std::unexpected(make_error(Error::Code::AuthenticationFailed,
                                                  "OAuth error: " + *error,
                                                  "Response: " + *response));
            }
        }

        // Check for access token
        auto access_token = parse_json_field(*response, "access_token");
        if (access_token) {
            auto token_type = parse_json_field(*response, "token_type").value_or("bearer");
            auto scope = parse_json_field(*response, "scope").value_or("");

            return AccessToken{
                .token = *access_token,
                .token_type = token_type,
                .scope = scope,
                .expires_at = std::nullopt, // Most OAuth tokens don't expire
            };
        }

        // Unknown response - retry
        std::this_thread::sleep_for(std::chrono::seconds(interval));
    }

    // Timeout
    return std::unexpected(make_error(
        Error::Code::AuthenticationFailed, "Authentication timeout",
        "User did not complete authentication within the time limit."));
}

auto OAuthDeviceFlow::authenticate(Provider provider, const std::string& url,
                                  const std::string& scopes, const std::string& gitlab_host)
    -> Result<Credential> {

    // Check if we're in interactive mode
    auto mode = InteractivePrompt::detect_mode();
    if (mode != InteractivePrompt::Mode::CLI) {
        return std::unexpected(make_error(
            Error::Code::CredentialRequired,
            "OAuth device flow requires interactive mode",
            "OAuth device flow cannot be used in TUI or non-interactive mode.\n\n"
            "Options:\n"
            "  1. Use credential helper:\n"
            "     git config --global credential.helper <helper>\n\n"
            "  2. Create a Personal Access Token manually:\n"
            "     GitHub: https://github.com/settings/tokens\n"
            "     GitLab: https://gitlab.com/-/profile/personal_access_tokens\n\n"
            "  3. Use SSH authentication instead."));
    }

    // Request device code
    auto device_code_result = request_device_code(provider, scopes, gitlab_host);
    if (!device_code_result) {
        return std::unexpected(device_code_result.error());
    }

    auto& device_code = *device_code_result;

    // Display instructions to user
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "  OAuth Device Flow Authentication\n";
    std::cout << "========================================\n";
    std::cout << "\n";
    std::cout << "Authenticating for: " << url << "\n";
    std::cout << "\n";
    std::cout << "To authenticate, visit:\n";
    std::cout << "  " << device_code.verification_uri << "\n";
    std::cout << "\n";
    std::cout << "And enter code:\n";
    std::cout << "  " << device_code.user_code << "\n";
    std::cout << "\n";
    std::cout << "Waiting for authentication";
    std::cout.flush();

    // Poll for token (with progress dots)
    std::string provider_name = provider == Provider::GitHub ? "GitHub" : "GitLab";

    auto token_result = poll_for_token(provider, device_code, 0, gitlab_host);

    std::cout << "\n";

    if (!token_result) {
        return std::unexpected(token_result.error());
    }

    std::cout << "\n";
    std::cout << "✓ Authentication successful!\n";
    std::cout << "\n";

    // Create credential with OAuth token
    // For GitHub, fetch the actual username using the token
    std::string username = "git";  // Default fallback

    if (provider == Provider::GitHub) {
        // Try to get GitHub username from API
        auto curl_binary = find_binary("curl");
        if (curl_binary) {
            std::string api_cmd = fmt::format(
                "{} -s -H \"Authorization: Bearer {}\" -H \"Accept: application/vnd.github.v3+json\" "
                "https://api.github.com/user 2>&1",
                *curl_binary, token_result->token);

            auto api_result = run_subprocess(api_cmd);
            if (api_result && api_result->success()) {
                auto login = parse_json_field(api_result->stdout_output, "login");
                if (login) {
                    username = *login;
                }
            }
        }
    } else if (provider == Provider::GitLab) {
        username = "oauth2";
    }

    return Credential::user_password(username, token_result->token);
}

} // namespace repo::backend
