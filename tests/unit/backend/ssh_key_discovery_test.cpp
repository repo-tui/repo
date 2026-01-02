#include <repo/backend/ssh_key_discovery.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

using namespace repo::backend;

// Helper to create a temporary directory for testing
class TempSSHDir {
  public:
    TempSSHDir() {
        path = std::filesystem::temp_directory_path() / ("test_ssh_" + random_string());
        std::filesystem::create_directories(path);
    }

    ~TempSSHDir() {
        if (std::filesystem::exists(path)) {
            std::filesystem::remove_all(path);
        }
    }

    auto get_path() const -> std::filesystem::path { return path; }

    void create_key_pair(const std::string& name, const std::string& type,
                         bool encrypt_private = false) {
        // Create public key
        auto pub_path = path / (name + ".pub");
        std::ofstream pub(pub_path);
        pub << "ssh-" << type << " AAAAB3NzaC1... comment@example.com\n";
        pub.close();

        // Create private key
        auto priv_path = path / name;
        std::ofstream priv(priv_path);
        if (encrypt_private) {
            priv << "-----BEGIN OPENSSH PRIVATE KEY-----\n";
            priv << "b3BlbnNzaC1rZXktdjEAAAAABGFlczI1Ni1jdHIAAAAGYmNyeXB0AAAAGAAAABD...\n";
            priv << "aes256-ctr encrypted key data here\n";
            priv << "-----END OPENSSH PRIVATE KEY-----\n";
        } else {
            // Unencrypted key - must contain "none" cipher indication
            priv << "-----BEGIN OPENSSH PRIVATE KEY-----\n";
            priv << "openssh-key-v1 none cipher used for unencrypted keys\n";
            priv << "b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAAAMwAAAAtzc2gtZW\n";
            priv << "-----END OPENSSH PRIVATE KEY-----\n";
        }
        priv.close();

        // Set proper permissions (600 for private key)
        std::filesystem::permissions(priv_path, std::filesystem::perms::owner_read |
                                                    std::filesystem::perms::owner_write);
    }

  private:
    std::filesystem::path path;

    static auto random_string() -> std::string {
        return std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    }
};

TEST_CASE("SSHKeyDiscovery - key type detection", "[ssh_key_discovery]") {
    TempSSHDir temp_dir;

    SECTION("Detect Ed25519 key") {
        temp_dir.create_key_pair("id_ed25519", "ed25519");

        auto type = SSHKeyDiscovery::get_key_type(temp_dir.get_path() / "id_ed25519.pub");
        REQUIRE(type);
        REQUIRE(*type == "ed25519");
    }

    SECTION("Detect RSA key") {
        temp_dir.create_key_pair("id_rsa", "rsa");

        auto type = SSHKeyDiscovery::get_key_type(temp_dir.get_path() / "id_rsa.pub");
        REQUIRE(type);
        REQUIRE(*type == "rsa");
    }

    SECTION("Detect ECDSA key") {
        temp_dir.create_key_pair("id_ecdsa", "ecdsa");

        auto type = SSHKeyDiscovery::get_key_type(temp_dir.get_path() / "id_ecdsa.pub");
        REQUIRE(type);
        REQUIRE(*type == "ecdsa");
    }

    SECTION("Return nullopt for non-existent file") {
        auto type = SSHKeyDiscovery::get_key_type(temp_dir.get_path() / "nonexistent.pub");
        REQUIRE_FALSE(type);
    }
}

TEST_CASE("SSHKeyDiscovery - encryption detection", "[ssh_key_discovery]") {
    TempSSHDir temp_dir;

    SECTION("Detect encrypted key") {
        temp_dir.create_key_pair("id_encrypted", "ed25519", true);

        bool encrypted = SSHKeyDiscovery::is_key_encrypted(temp_dir.get_path() / "id_encrypted");
        REQUIRE(encrypted);
    }

    SECTION("Detect unencrypted key") {
        temp_dir.create_key_pair("id_plain", "ed25519", false);

        bool encrypted = SSHKeyDiscovery::is_key_encrypted(temp_dir.get_path() / "id_plain");
        REQUIRE_FALSE(encrypted);
    }

    SECTION("Return false for non-existent file") {
        bool encrypted = SSHKeyDiscovery::is_key_encrypted(temp_dir.get_path() / "nonexistent");
        REQUIRE_FALSE(encrypted);
    }
}

TEST_CASE("SSHKeyDiscovery - key discovery", "[ssh_key_discovery]") {
    TempSSHDir temp_dir;

    SECTION("Discover single key pair") {
        temp_dir.create_key_pair("id_ed25519", "ed25519");

        auto keys = SSHKeyDiscovery::discover_keys_in(temp_dir.get_path());

        REQUIRE(keys.size() == 1);
        REQUIRE(keys[0].key_type == "ed25519");
        REQUIRE(keys[0].public_key.filename() == "id_ed25519.pub");
        REQUIRE(keys[0].private_key.filename() == "id_ed25519");
        REQUIRE_FALSE(keys[0].is_encrypted);
    }

    SECTION("Discover multiple key pairs") {
        temp_dir.create_key_pair("id_ed25519", "ed25519");
        temp_dir.create_key_pair("id_rsa", "rsa");
        temp_dir.create_key_pair("id_ecdsa", "ecdsa");

        auto keys = SSHKeyDiscovery::discover_keys_in(temp_dir.get_path());

        REQUIRE(keys.size() == 3);
        // Should be sorted by priority (ed25519 first)
        REQUIRE(keys[0].key_type == "ed25519");
    }

    SECTION("Skip public key without matching private key") {
        // Create only public key
        auto pub_path = temp_dir.get_path() / "orphan.pub";
        std::ofstream pub(pub_path);
        pub << "ssh-rsa AAAAB3... comment\n";
        pub.close();

        auto keys = SSHKeyDiscovery::discover_keys_in(temp_dir.get_path());

        REQUIRE(keys.empty());
    }

    SECTION("Return empty for non-existent directory") {
        auto keys = SSHKeyDiscovery::discover_keys_in(temp_dir.get_path() / "nonexistent");

        REQUIRE(keys.empty());
    }

    SECTION("Return empty for empty directory") {
        auto keys = SSHKeyDiscovery::discover_keys_in(temp_dir.get_path());

        REQUIRE(keys.empty());
    }
}

TEST_CASE("SSHKeyDiscovery - key prioritization", "[ssh_key_discovery]") {
    TempSSHDir temp_dir;

    SECTION("Ed25519 has higher priority than RSA") {
        temp_dir.create_key_pair("id_rsa", "rsa");
        temp_dir.create_key_pair("id_ed25519", "ed25519");

        auto keys = SSHKeyDiscovery::discover_keys_in(temp_dir.get_path());

        REQUIRE(keys.size() == 2);
        // Ed25519 should come first
        REQUIRE(keys[0].key_type == "ed25519");
        REQUIRE(keys[1].key_type == "rsa");
    }

    SECTION("Default key names have higher priority") {
        temp_dir.create_key_pair("custom_ed25519", "ed25519");
        temp_dir.create_key_pair("id_ed25519", "ed25519");

        auto keys = SSHKeyDiscovery::discover_keys_in(temp_dir.get_path());

        REQUIRE(keys.size() == 2);
        // id_ed25519 should come first (default name)
        REQUIRE(keys[0].private_key.filename() == "id_ed25519");
        REQUIRE(keys[1].private_key.filename() == "custom_ed25519");
    }

    SECTION("Service-specific keys get priority boost") {
        temp_dir.create_key_pair("custom", "ed25519");
        temp_dir.create_key_pair("github_key", "ed25519");

        auto keys = SSHKeyDiscovery::discover_keys_in(temp_dir.get_path());

        REQUIRE(keys.size() == 2);
        // github_key should come first
        REQUIRE(keys[0].private_key.filename().string().find("github") != std::string::npos);
    }
}

TEST_CASE("SSHKeyDiscovery - encrypted key handling", "[ssh_key_discovery]") {
    TempSSHDir temp_dir;

    SECTION("Mark encrypted keys correctly") {
        temp_dir.create_key_pair("id_encrypted", "ed25519", true);
        temp_dir.create_key_pair("id_plain", "ed25519", false);

        auto keys = SSHKeyDiscovery::discover_keys_in(temp_dir.get_path());

        REQUIRE(keys.size() == 2);

        // Find the encrypted and plain keys
        auto encrypted_it = std::find_if(keys.begin(), keys.end(), [](const auto& k) {
            return k.private_key.filename() == "id_encrypted";
        });

        auto plain_it = std::find_if(keys.begin(), keys.end(), [](const auto& k) {
            return k.private_key.filename() == "id_plain";
        });

        REQUIRE(encrypted_it != keys.end());
        REQUIRE(plain_it != keys.end());

        REQUIRE(encrypted_it->is_encrypted);
        REQUIRE_FALSE(plain_it->is_encrypted);
    }
}
