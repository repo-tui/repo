#pragma once

#include <optional>
#include <string>

namespace repo::backend {

/// Types of authentication credentials
enum class CredentialType {
    /// Username and password (or Personal Access Token)
    UserPassword,

    /// SSH key from file
    SSHKey,

    /// SSH key from ssh-agent
    SSHAgent,

    /// OAuth token
    OAuth,

    /// System default credentials (NTLM/Kerberos)
    Default
};

/// Authentication credential
struct Credential {
    CredentialType type;

    // For UserPassword and OAuth
    std::string username;
    std::string password; // Can be a password, PAT, or OAuth token

    // For SSHKey
    std::optional<std::string> ssh_public_key_path;
    std::optional<std::string> ssh_private_key_path;
    std::optional<std::string> ssh_passphrase;

    /// Create username/password credential
    [[nodiscard]] static auto user_password(std::string user, std::string pass) -> Credential {
        Credential cred;
        cred.type = CredentialType::UserPassword;
        cred.username = std::move(user);
        cred.password = std::move(pass);
        return cred;
    }

    /// Create SSH key credential
    [[nodiscard]] static auto ssh_key(std::string user, std::string pub_key, std::string priv_key,
                                      std::optional<std::string> passphrase = std::nullopt)
        -> Credential {
        Credential cred;
        cred.type = CredentialType::SSHKey;
        cred.username = std::move(user);
        cred.ssh_public_key_path = std::move(pub_key);
        cred.ssh_private_key_path = std::move(priv_key);
        cred.ssh_passphrase = std::move(passphrase);
        return cred;
    }

    /// Create SSH agent credential
    [[nodiscard]] static auto ssh_agent(std::string user) -> Credential {
        Credential cred;
        cred.type = CredentialType::SSHAgent;
        cred.username = std::move(user);
        return cred;
    }

    /// Create OAuth credential
    [[nodiscard]] static auto oauth(std::string user, std::string token) -> Credential {
        Credential cred;
        cred.type = CredentialType::OAuth;
        cred.username = std::move(user);
        cred.password = std::move(token); // OAuth token stored as password
        return cred;
    }

    /// Create default (system) credential
    [[nodiscard]] static auto default_credential() -> Credential {
        Credential cred;
        cred.type = CredentialType::Default;
        return cred;
    }
};

/// Context provided to authentication callback
struct AuthenticationContext {
    /// URL being accessed
    std::string url;

    /// Username extracted from URL (if any)
    std::optional<std::string> username_from_url;

    /// Bitmask of allowed credential types (from libgit2)
    unsigned int allowed_types;

    /// Number of previous authentication attempts for this URL
    int attempt_count;

    /// Whether this is a retry after a failed attempt
    bool is_retry;
};

} // namespace repo::backend
