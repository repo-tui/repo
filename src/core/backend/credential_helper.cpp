#include <repo/backend/credential_helper.hpp>
#include <repo/error.hpp>

#include <sstream>

#include "subprocess_utils.hpp"

namespace repo::backend {

auto CredentialHelper::fill(const std::string& url, const std::string& username_hint)
    -> Result<Credential> {

    // Parse URL into components
    auto components_result = parse_url(url);
    if (!components_result) {
        return std::unexpected(components_result.error());
    }
    auto& components = *components_result;

    // Use username hint if provided
    if (!username_hint.empty()) {
        components.username = username_hint;
    }

    // Invoke git credential fill
    auto output_result = invoke_credential_helper("fill", components);
    if (!output_result) {
        return std::unexpected(output_result.error());
    }

    // Parse output
    auto creds = parse_credential_output(*output_result);

    // Extract username and password
    auto username_it = creds.find("username");
    auto password_it = creds.find("password");

    if (username_it == creds.end() || password_it == creds.end()) {
        return std::unexpected(
            make_error(Error::Code::CredentialRequired,
                       "Git credential helper did not return username/password",
                       "The credential helper returned an incomplete response.\n\n"
                       "Ensure your credential helper is properly configured:\n"
                       "  git config --global credential.helper <helper>\n\n"
                       "Available helpers:\n"
                       "  macOS:  osxkeychain\n"
                       "  Linux:  libsecret, cache, store"));
    }

    return Credential::user_password(username_it->second, password_it->second);
}

auto CredentialHelper::approve(const std::string& url, const Credential& cred) -> Status {
    // Parse URL
    auto components_result = parse_url(url);
    if (!components_result) {
        return std::unexpected(components_result.error());
    }

    // Invoke git credential approve
    auto result = invoke_credential_helper("approve", *components_result, true, &cred);
    if (!result) {
        return std::unexpected(result.error());
    }

    return {}; // Success
}

auto CredentialHelper::reject(const std::string& url) -> Status {
    // Parse URL
    auto components_result = parse_url(url);
    if (!components_result) {
        return std::unexpected(components_result.error());
    }

    // Invoke git credential reject
    auto result = invoke_credential_helper("reject", *components_result);
    if (!result) {
        return std::unexpected(result.error());
    }

    return {}; // Success
}

auto CredentialHelper::is_available() const -> bool {
    // Check if git is installed
    auto git_binary = find_git_binary();
    if (!git_binary) {
        return false;
    }

    // Check if credential.helper is configured (try to get config value)
    auto result = run_subprocess("git config --get credential.helper");
    if (!result || result->exit_code != 0 || result->stdout_output.empty()) {
        return false;
    }

    return true;
}

auto CredentialHelper::parse_url(const std::string& url) -> Result<URLComponents> {
    URLComponents components;

    // Simple URL parsing
    // Format: [protocol://][username@]host[:port][/path]

    std::string remaining = url;

    // Extract protocol
    size_t protocol_end = remaining.find("://");
    if (protocol_end != std::string::npos) {
        components.protocol = remaining.substr(0, protocol_end);
        remaining = remaining.substr(protocol_end + 3); // Skip "://"
    } else {
        // Default to https if no protocol
        components.protocol = "https";
    }

    // Extract username if present (before @)
    size_t at_pos = remaining.find('@');
    if (at_pos != std::string::npos) {
        components.username = remaining.substr(0, at_pos);
        remaining = remaining.substr(at_pos + 1); // Skip @
    }

    // Extract path (after first /)
    size_t path_start = remaining.find('/');
    std::string host_port;
    if (path_start != std::string::npos) {
        components.path = remaining.substr(path_start);
        host_port = remaining.substr(0, path_start);
    } else {
        host_port = remaining;
    }

    // Extract port (after :)
    size_t colon_pos = host_port.find(':');
    if (colon_pos != std::string::npos) {
        components.host = host_port.substr(0, colon_pos);
        components.port = host_port.substr(colon_pos + 1);
    } else {
        components.host = host_port;
    }

    if (components.host.empty()) {
        return std::unexpected(
            make_error(Error::Code::InvalidArgument, "Invalid URL: missing host", "URL: " + url));
    }

    return components;
}

auto CredentialHelper::invoke_credential_helper(const std::string& operation,
                                                const URLComponents& components,
                                                bool include_credentials, const Credential* cred)
    -> Result<std::string> {

    // Build input for credential helper
    std::string input = build_credential_input(components, include_credentials, cred);

    // Find git binary
    auto git_binary = find_git_binary();
    if (!git_binary) {
        return std::unexpected(git_binary.error());
    }

    // Build command
    std::string command = *git_binary + " credential " + operation;

    // Execute command
    auto result = run_subprocess(command, input);
    if (!result) {
        return std::unexpected(result.error());
    }

    // Check exit code for errors
    if (result->exit_code != 0) {
        return std::unexpected(make_error(Error::Code::CredentialHelperError,
                                          "Git credential " + operation + " failed",
                                          "Exit code: " + std::to_string(result->exit_code) +
                                              "\nStderr: " + result->stderr_output));
    }

    return result->stdout_output;
}

auto CredentialHelper::parse_credential_output(const std::string& output)
    -> std::unordered_map<std::string, std::string> {

    std::unordered_map<std::string, std::string> result;
    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line)) {
        // Skip empty lines
        if (line.empty()) {
            continue;
        }

        // Parse key=value
        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);
            result[key] = value;
        }
    }

    return result;
}

auto CredentialHelper::build_credential_input(const URLComponents& components,
                                              bool include_credentials, const Credential* cred)
    -> std::string {
    std::ostringstream input;

    // Build input in git credential protocol format
    input << "protocol=" << components.protocol << "\n";
    input << "host=" << components.host << "\n";

    if (!components.port.empty()) {
        input << "port=" << components.port << "\n";
    }

    if (!components.path.empty()) {
        // Only include path if credential.useHttpPath is set
        // For now, we'll include it - users can configure git if needed
        input << "path=" << components.path << "\n";
    }

    if (!components.username.empty()) {
        input << "username=" << components.username << "\n";
    }

    if (include_credentials && cred) {
        // Include credentials for approve/reject operations
        if (cred->type == CredentialType::UserPassword || cred->type == CredentialType::OAuth) {
            if (!cred->username.empty()) {
                input << "username=" << cred->username << "\n";
            }
            if (!cred->password.empty()) {
                input << "password=" << cred->password << "\n";
            }
        }
    }

    input << "\n"; // Blank line terminates input

    return input.str();
}

} // namespace repo::backend
