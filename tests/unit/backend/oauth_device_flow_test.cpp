#include <catch2/catch_test_macros.hpp>

#include <repo/backend/oauth_device_flow.hpp>

using namespace repo::backend;

TEST_CASE("OAuthDeviceFlow - provider detection", "[oauth_device_flow]") {
    SECTION("Detect GitHub from HTTPS URL") {
        auto provider = OAuthDeviceFlow::detect_provider("https://github.com/user/repo.git");
        REQUIRE(provider);
        REQUIRE(*provider == OAuthDeviceFlow::Provider::GitHub);
    }

    SECTION("Detect GitHub from SSH URL") {
        auto provider = OAuthDeviceFlow::detect_provider("git@github.com:user/repo.git");
        REQUIRE(provider);
        REQUIRE(*provider == OAuthDeviceFlow::Provider::GitHub);
    }

    SECTION("Detect GitLab from HTTPS URL") {
        auto provider = OAuthDeviceFlow::detect_provider("https://gitlab.com/user/repo.git");
        REQUIRE(provider);
        REQUIRE(*provider == OAuthDeviceFlow::Provider::GitLab);
    }

    SECTION("Detect GitLab from custom instance") {
        auto provider =
            OAuthDeviceFlow::detect_provider("https://gitlab.example.com/user/repo.git");
        REQUIRE(provider);
        REQUIRE(*provider == OAuthDeviceFlow::Provider::GitLab);
    }

    SECTION("Return nullopt for unknown provider") {
        auto provider = OAuthDeviceFlow::detect_provider("https://bitbucket.org/user/repo.git");
        REQUIRE_FALSE(provider);
    }

    SECTION("Return nullopt for non-Git URL") {
        auto provider = OAuthDeviceFlow::detect_provider("https://example.com");
        REQUIRE_FALSE(provider);
    }
}

TEST_CASE("OAuthDeviceFlow - default scopes", "[oauth_device_flow]") {
    SECTION("GitHub default scope is 'repo'") {
        auto scope = OAuthDeviceFlow::get_default_scopes(OAuthDeviceFlow::Provider::GitHub);
        REQUIRE(scope == "repo");
    }

    SECTION("GitLab default scope is 'write_repository'") {
        auto scope = OAuthDeviceFlow::get_default_scopes(OAuthDeviceFlow::Provider::GitLab);
        REQUIRE(scope == "write_repository");
    }
}

TEST_CASE("OAuthDeviceFlow - JSON parsing", "[oauth_device_flow]") {
    SECTION("Parse string field") {
        std::string json = R"({"device_code":"ABC123","user_code":"WXYZ-1234"})";

        auto device_code = OAuthDeviceFlow::parse_json_field(json, "device_code");
        auto user_code = OAuthDeviceFlow::parse_json_field(json, "user_code");

        REQUIRE(device_code);
        REQUIRE(*device_code == "ABC123");
        REQUIRE(user_code);
        REQUIRE(*user_code == "WXYZ-1234");
    }

    SECTION("Parse integer field") {
        std::string json = R"({"expires_in":900,"interval":5})";

        auto expires_in = OAuthDeviceFlow::parse_json_int_field(json, "expires_in");
        auto interval = OAuthDeviceFlow::parse_json_int_field(json, "interval");

        REQUIRE(expires_in);
        REQUIRE(*expires_in == 900);
        REQUIRE(interval);
        REQUIRE(*interval == 5);
    }

    SECTION("Return nullopt for missing field") {
        std::string json = R"({"device_code":"ABC123"})";

        auto missing = OAuthDeviceFlow::parse_json_field(json, "user_code");
        REQUIRE_FALSE(missing);
    }

    SECTION("Handle JSON with whitespace") {
        std::string json = R"({
            "device_code" : "ABC123",
            "user_code"   : "WXYZ-1234"
        })";

        auto device_code = OAuthDeviceFlow::parse_json_field(json, "device_code");
        REQUIRE(device_code);
        REQUIRE(*device_code == "ABC123");
    }

    SECTION("Parse nested values") {
        std::string json =
            R"({"data":{"token":"secret"},"access_token":"ghp_abc123"})";

        auto token = OAuthDeviceFlow::parse_json_field(json, "access_token");
        REQUIRE(token);
        REQUIRE(*token == "ghp_abc123");
    }

    SECTION("Handle empty JSON") {
        std::string json = "{}";

        auto field = OAuthDeviceFlow::parse_json_field(json, "any_field");
        REQUIRE_FALSE(field);
    }

    SECTION("Handle malformed JSON gracefully") {
        std::string json = "not valid json";

        auto field = OAuthDeviceFlow::parse_json_field(json, "field");
        REQUIRE_FALSE(field);
    }
}

TEST_CASE("OAuthDeviceFlow - endpoint generation", "[oauth_device_flow]") {
    SECTION("GitHub device code endpoint") {
        auto endpoint = OAuthDeviceFlow::get_device_code_endpoint(
            OAuthDeviceFlow::Provider::GitHub, "");
        REQUIRE(endpoint == "https://github.com/login/device/code");
    }

    SECTION("GitHub token endpoint") {
        auto endpoint =
            OAuthDeviceFlow::get_token_endpoint(OAuthDeviceFlow::Provider::GitHub, "");
        REQUIRE(endpoint == "https://github.com/login/oauth/access_token");
    }

    SECTION("GitLab device code endpoint with custom host") {
        auto endpoint = OAuthDeviceFlow::get_device_code_endpoint(
            OAuthDeviceFlow::Provider::GitLab, "gitlab.example.com");
        REQUIRE(endpoint == "https://gitlab.example.com/oauth/authorize_device");
    }

    SECTION("GitLab token endpoint with default host") {
        auto endpoint = OAuthDeviceFlow::get_token_endpoint(
            OAuthDeviceFlow::Provider::GitLab, "gitlab.com");
        REQUIRE(endpoint == "https://gitlab.com/oauth/token");
    }
}
