#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "../result.hpp"
#include "credential.hpp"

namespace repo::backend {

/// SSH key discovery for automatic authentication
/// Discovers and prioritizes SSH keys from ~/.ssh/ directory
class SSHKeyDiscovery {
  public:
    /// Discovered SSH key pair
    struct KeyPair {
        std::filesystem::path public_key;  // Path to public key (.pub)
        std::filesystem::path private_key; // Path to private key
        std::string key_type;              // Key type (rsa, ed25519, ecdsa, etc.)
        bool is_encrypted;                 // Whether private key is encrypted

        /// Priority for trying this key (lower = higher priority)
        /// Based on key type and common naming conventions
        int priority;
    };

    /// Discover all SSH key pairs in ~/.ssh/
    /// Returns keys sorted by priority (most likely to work first)
    ///
    /// Priority order:
    ///   1. Ed25519 keys (most secure, recommended)
    ///   2. ECDSA keys
    ///   3. RSA keys (still common)
    ///   4. DSA keys (legacy, least secure)
    ///
    /// Within each type, prioritize common names:
    ///   - id_{type} (default name)
    ///   - github_{type}, gitlab_{type} (service-specific)
    ///   - {type}_key (alternative naming)
    [[nodiscard]] static auto discover_keys() -> std::vector<KeyPair>;

    /// Discover keys in a specific directory (for testing)
    [[nodiscard]] static auto discover_keys_in(const std::filesystem::path& ssh_dir)
        -> std::vector<KeyPair>;

    /// Check if a private key file is encrypted (requires passphrase)
    /// Reads the key file and checks for encryption headers
    [[nodiscard]] static auto is_key_encrypted(const std::filesystem::path& private_key) -> bool;

    /// Get key type from public key file
    /// Reads first line and extracts type (ssh-rsa, ssh-ed25519, etc.)
    [[nodiscard]] static auto get_key_type(const std::filesystem::path& public_key)
        -> std::optional<std::string>;

    /// Get default SSH directory (~/.ssh)
    [[nodiscard]] static auto get_ssh_directory() -> std::filesystem::path;

    /// Try to authenticate using discovered keys
    /// Attempts each discovered key in priority order
    /// Prompts for passphrase if key is encrypted
    ///
    /// @param username SSH username (usually "git" for GitHub/GitLab)
    /// @param url URL being accessed (for passphrase prompt context)
    /// @return Credential with SSH key, or error if all keys failed
    [[nodiscard]] static auto try_discovered_keys(const std::string& username,
                                                  const std::string& url) -> Result<Credential>;

  private:
    /// Calculate priority score for a key pair
    /// Lower score = higher priority
    static auto calculate_priority(const std::filesystem::path& path, const std::string& key_type)
        -> int;

    /// Check if a file exists and is readable
    static auto is_readable_file(const std::filesystem::path& path) -> bool;

    /// Find matching private key for a public key
    /// Public key is .pub file, private key is same name without .pub
    static auto find_private_key(const std::filesystem::path& public_key)
        -> std::optional<std::filesystem::path>;
};

} // namespace repo::backend
