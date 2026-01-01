#include <repo/backend/ssh_key_discovery.hpp>
#include <repo/backend/interactive_prompt.hpp>
#include <repo/error.hpp>

#include <fstream>
#include <algorithm>
#include <unistd.h>
#include <pwd.h>

namespace repo::backend {

auto SSHKeyDiscovery::get_ssh_directory() -> std::filesystem::path {
    // Get home directory
    const char* home = getenv("HOME");
    if (!home) {
        // Fallback to getpwuid
        struct passwd* pw = getpwuid(getuid());
        if (pw) {
            home = pw->pw_dir;
        }
    }

    if (!home) {
        return {}; // No home directory found
    }

    return std::filesystem::path(home) / ".ssh";
}

auto SSHKeyDiscovery::discover_keys() -> std::vector<KeyPair> {
    return discover_keys_in(get_ssh_directory());
}

auto SSHKeyDiscovery::discover_keys_in(const std::filesystem::path& ssh_dir)
    -> std::vector<KeyPair> {

    std::vector<KeyPair> keys;

    // Check if SSH directory exists
    if (!std::filesystem::exists(ssh_dir) || !std::filesystem::is_directory(ssh_dir)) {
        return keys; // Empty vector
    }

    // Iterate through all .pub files (public keys)
    for (const auto& entry : std::filesystem::directory_iterator(ssh_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const auto& path = entry.path();

        // Only process .pub files
        if (path.extension() != ".pub") {
            continue;
        }

        // Find corresponding private key
        auto private_key = find_private_key(path);
        if (!private_key) {
            continue; // No matching private key
        }

        // Get key type
        auto key_type = get_key_type(path);
        if (!key_type) {
            continue; // Cannot determine key type
        }

        // Check if private key is encrypted
        bool encrypted = is_key_encrypted(*private_key);

        // Calculate priority
        int priority = calculate_priority(*private_key, *key_type);

        keys.push_back(KeyPair{
            .public_key = path,
            .private_key = *private_key,
            .key_type = *key_type,
            .is_encrypted = encrypted,
            .priority = priority,
        });
    }

    // Sort by priority (lower = higher priority)
    std::sort(keys.begin(), keys.end(),
              [](const KeyPair& a, const KeyPair& b) { return a.priority < b.priority; });

    return keys;
}

auto SSHKeyDiscovery::find_private_key(const std::filesystem::path& public_key)
    -> std::optional<std::filesystem::path> {

    // Private key is same name without .pub extension
    auto private_key = public_key;
    private_key.replace_extension(""); // Remove .pub

    if (is_readable_file(private_key)) {
        return private_key;
    }

    return std::nullopt;
}

auto SSHKeyDiscovery::get_key_type(const std::filesystem::path& public_key)
    -> std::optional<std::string> {

    std::ifstream file(public_key);
    if (!file.is_open()) {
        return std::nullopt;
    }

    // Read first line
    std::string line;
    if (!std::getline(file, line)) {
        return std::nullopt;
    }

    // Public key format: "ssh-{type} {base64-data} {comment}"
    // Extract the type from first token
    size_t space_pos = line.find(' ');
    if (space_pos == std::string::npos) {
        return std::nullopt;
    }

    std::string type = line.substr(0, space_pos);

    // Convert "ssh-ed25519" -> "ed25519", "ssh-rsa" -> "rsa", etc.
    if (type.starts_with("ssh-")) {
        type = type.substr(4); // Remove "ssh-" prefix
    }

    return type;
}

auto SSHKeyDiscovery::is_key_encrypted(const std::filesystem::path& private_key) -> bool {
    std::ifstream file(private_key);
    if (!file.is_open()) {
        return false; // Assume not encrypted if we can't read it
    }

    // Read file content to check for encryption markers
    std::string content;
    std::string line;
    while (std::getline(file, line)) {
        content += line + "\n";
        // Only need to read first few lines
        if (content.size() > 500) {
            break;
        }
    }

    // Check for encryption markers in OpenSSH format
    // Old format: "Proc-Type: 4,ENCRYPTED"
    // New format: "-----BEGIN ENCRYPTED PRIVATE KEY-----"
    // New OpenSSH format: "-----BEGIN OPENSSH PRIVATE KEY-----" with "aes256-ctr" or similar

    if (content.find("ENCRYPTED") != std::string::npos) {
        return true;
    }

    // For new OpenSSH format, check for cipher markers
    if (content.find("aes") != std::string::npos || content.find("AES") != std::string::npos) {
        return true;
    }

    // Check for "Proc-Type: 4,ENCRYPTED"
    if (content.find("Proc-Type: 4,ENCRYPTED") != std::string::npos) {
        return true;
    }

    // If it says "BEGIN OPENSSH PRIVATE KEY" but doesn't have "none" cipher, it's encrypted
    if (content.find("BEGIN OPENSSH PRIVATE KEY") != std::string::npos) {
        // Unencrypted keys have "none" as cipher
        if (content.find("none") == std::string::npos) {
            return true; // Likely encrypted
        }
    }

    return false;
}

auto SSHKeyDiscovery::calculate_priority(const std::filesystem::path& path,
                                         const std::string& key_type) -> int {
    int priority = 0;

    // Base priority by key type (lower = better)
    if (key_type == "ed25519") {
        priority = 100; // Highest priority - most secure, modern
    } else if (key_type == "ecdsa") {
        priority = 200;
    } else if (key_type == "rsa") {
        priority = 300;
    } else if (key_type == "dsa") {
        priority = 400; // Lowest priority - legacy, insecure
    } else {
        priority = 500; // Unknown type
    }

    // Adjust priority based on filename (common names get higher priority)
    std::string filename = path.filename().string();

    // Default key names get bonus (lower priority number = higher priority)
    if (filename.starts_with("id_")) {
        priority -= 50; // Boost priority
    }

    // Service-specific keys
    if (filename.find("github") != std::string::npos) {
        priority -= 30;
    }
    if (filename.find("gitlab") != std::string::npos) {
        priority -= 30;
    }

    // Legacy or test keys get penalty
    if (filename.find("old") != std::string::npos || filename.find("backup") != std::string::npos ||
        filename.find("test") != std::string::npos) {
        priority += 100;
    }

    return priority;
}

auto SSHKeyDiscovery::is_readable_file(const std::filesystem::path& path) -> bool {
    if (!std::filesystem::exists(path)) {
        return false;
    }

    if (!std::filesystem::is_regular_file(path)) {
        return false;
    }

    // Check if readable
    std::ifstream file(path);
    return file.is_open();
}

auto SSHKeyDiscovery::try_discovered_keys(const std::string& username, const std::string& url)
    -> Result<Credential> {

    auto keys = discover_keys();

    if (keys.empty()) {
        return std::unexpected(make_error(
            Error::Code::CredentialRequired,
            "No SSH keys found in ~/.ssh/ for " + url,
            "No SSH key pairs found.\n\n"
            "To generate a new SSH key:\n"
            "  ssh-keygen -t ed25519 -C \"your_email@example.com\"\n\n"
            "Then add the public key to your Git hosting service:\n"
            "  cat ~/.ssh/id_ed25519.pub\n\n"
            "Or use HTTPS authentication instead."));
    }

    // Try each key in priority order
    for (const auto& key : keys) {
        std::optional<std::string> passphrase;

        // If key is encrypted, prompt for passphrase
        if (key.is_encrypted) {
            // Only prompt in CLI mode
            if (InteractivePrompt::detect_mode() == InteractivePrompt::Mode::CLI) {
                auto key_name = key.private_key.filename().string();
                auto prompt = "Passphrase for SSH key '" + key_name + "': ";

                auto passphrase_result = InteractivePrompt::prompt_password(prompt);
                if (passphrase_result) {
                    passphrase = std::move(*passphrase_result);
                } else {
                    // User cancelled or error - skip this key
                    continue;
                }
            } else {
                // In TUI/non-interactive mode, skip encrypted keys
                continue;
            }
        }

        // Create credential with this key pair
        return Credential::ssh_key(username, key.public_key.string(), key.private_key.string(),
                                   std::move(passphrase));
    }

    // All keys failed or were skipped
    return std::unexpected(make_error(
        Error::Code::CredentialRequired,
        "Could not authenticate with any SSH key for " + url,
        "Tried " + std::to_string(keys.size()) + " SSH key(s) but none worked.\n\n"
                                                  "Possible issues:\n"
                                                  "  - Keys are encrypted but no passphrase provided (non-interactive mode)\n"
                                                  "  - Public key not added to Git hosting service\n"
                                                  "  - Wrong key permissions (should be 600 for private key)\n\n"
                                                  "Try using ssh-agent:\n"
                                                  "  ssh-add ~/.ssh/id_ed25519\n\n"
                                                  "Or use HTTPS authentication instead."));
}

} // namespace repo::backend
