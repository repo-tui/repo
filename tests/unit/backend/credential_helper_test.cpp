#include <catch2/catch_test_macros.hpp>

#include <repo/backend/credential_helper.hpp>

using namespace repo::backend;

TEST_CASE("CredentialHelper - URL parsing", "[credential_helper]") {
    CredentialHelper helper;

    SECTION("Parse HTTPS URL with path") {
        auto result = helper.parse_url("https://github.com/user/repo.git");
        REQUIRE(result);
        REQUIRE(result->protocol == "https");
        REQUIRE(result->host == "github.com");
        REQUIRE(result->path == "/user/repo.git");
        REQUIRE(result->username.empty());
        REQUIRE(result->port.empty());
    }

    SECTION("Parse HTTPS URL with username") {
        auto result = helper.parse_url("https://alice@gitlab.com/project.git");
        REQUIRE(result);
        REQUIRE(result->protocol == "https");
        REQUIRE(result->host == "gitlab.com");
        REQUIRE(result->username == "alice");
        REQUIRE(result->path == "/project.git");
    }

    SECTION("Parse HTTPS URL with port") {
        auto result = helper.parse_url("https://gitlab.example.com:8443/repo.git");
        REQUIRE(result);
        REQUIRE(result->protocol == "https");
        REQUIRE(result->host == "gitlab.example.com");
        REQUIRE(result->port == "8443");
        REQUIRE(result->path == "/repo.git");
    }

    SECTION("Parse URL without protocol (defaults to https)") {
        auto result = helper.parse_url("github.com/user/repo.git");
        REQUIRE(result);
        REQUIRE(result->protocol == "https");
        REQUIRE(result->host == "github.com");
        REQUIRE(result->path == "/user/repo.git");
    }

    SECTION("Parse URL with username and port") {
        auto result = helper.parse_url("https://bob@server.com:3000/path");
        REQUIRE(result);
        REQUIRE(result->protocol == "https");
        REQUIRE(result->host == "server.com");
        REQUIRE(result->username == "bob");
        REQUIRE(result->port == "3000");
        REQUIRE(result->path == "/path");
    }

    SECTION("Parse URL without path") {
        auto result = helper.parse_url("https://example.com");
        REQUIRE(result);
        REQUIRE(result->protocol == "https");
        REQUIRE(result->host == "example.com");
        REQUIRE(result->path.empty());
    }

    SECTION("Reject empty URL") {
        auto result = helper.parse_url("");
        REQUIRE_FALSE(result);
    }

    SECTION("Reject URL with no host") {
        auto result = helper.parse_url("https://");
        REQUIRE_FALSE(result);
    }
}

TEST_CASE("CredentialHelper - build credential input", "[credential_helper]") {
    CredentialHelper helper;

    SECTION("Build basic input") {
        CredentialHelper::URLComponents components;
        components.protocol = "https";
        components.host = "github.com";
        components.path = "/user/repo.git";

        auto input = helper.build_credential_input(components, false, nullptr);

        REQUIRE(input.find("protocol=https") != std::string::npos);
        REQUIRE(input.find("host=github.com") != std::string::npos);
        REQUIRE(input.find("path=/user/repo.git") != std::string::npos);
    }

    SECTION("Build input with username") {
        CredentialHelper::URLComponents components;
        components.protocol = "https";
        components.host = "gitlab.com";
        components.username = "alice";

        auto input = helper.build_credential_input(components, false, nullptr);

        REQUIRE(input.find("username=alice") != std::string::npos);
    }

    SECTION("Build input with port") {
        CredentialHelper::URLComponents components;
        components.protocol = "https";
        components.host = "server.com";
        components.port = "8443";

        auto input = helper.build_credential_input(components, false, nullptr);

        REQUIRE(input.find("port=8443") != std::string::npos);
    }

    SECTION("Build input with credentials for approve") {
        CredentialHelper::URLComponents components;
        components.protocol = "https";
        components.host = "github.com";

        auto cred = Credential::user_password("testuser", "testpass");

        auto input = helper.build_credential_input(components, true, &cred);

        REQUIRE(input.find("username=testuser") != std::string::npos);
        REQUIRE(input.find("password=testpass") != std::string::npos);
    }
}

TEST_CASE("CredentialHelper - parse credential output", "[credential_helper]") {
    CredentialHelper helper;

    SECTION("Parse simple key=value pairs") {
        std::string output = "username=alice\npassword=secret123\n";

        auto result = helper.parse_credential_output(output);

        REQUIRE(result["username"] == "alice");
        REQUIRE(result["password"] == "secret123");
    }

    SECTION("Parse output with multiple fields") {
        std::string output = "protocol=https\nhost=github.com\nusername=bob\npassword=token456\n";

        auto result = helper.parse_credential_output(output);

        REQUIRE(result["protocol"] == "https");
        REQUIRE(result["host"] == "github.com");
        REQUIRE(result["username"] == "bob");
        REQUIRE(result["password"] == "token456");
    }

    SECTION("Ignore empty lines") {
        std::string output = "username=alice\n\n\npassword=secret\n";

        auto result = helper.parse_credential_output(output);

        REQUIRE(result.size() == 2);
        REQUIRE(result["username"] == "alice");
        REQUIRE(result["password"] == "secret");
    }

    SECTION("Handle missing values") {
        std::string output = "username=alice\ninvalid_line\npassword=secret\n";

        auto result = helper.parse_credential_output(output);

        // Should ignore lines without '='
        REQUIRE(result.size() == 2);
        REQUIRE(result["username"] == "alice");
        REQUIRE(result["password"] == "secret");
    }

    SECTION("Handle empty output") {
        std::string output = "";

        auto result = helper.parse_credential_output(output);

        REQUIRE(result.empty());
    }
}
