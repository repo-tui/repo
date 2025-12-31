#include <repo/domain/signature.hpp>

#include <catch2/catch_all.hpp>

#include <chrono>

using namespace repo::domain;
using namespace std::chrono_literals;

TEST_CASE("Signature - basic construction", "[domain][signature]") {
    auto now = std::chrono::system_clock::now();

    Signature sig{.name = "John Doe", .email = "john@example.com", .when = now, .tz_offset = 0min};

    REQUIRE(sig.name == "John Doe");
    REQUIRE(sig.email == "john@example.com");
    REQUIRE(sig.when == now);
    REQUIRE(sig.tz_offset == 0min);
}

TEST_CASE("Signature - format_name_email", "[domain][signature]") {
    auto now = std::chrono::system_clock::now();

    SECTION("Standard format") {
        Signature sig{
            .name = "John Doe", .email = "john@example.com", .when = now, .tz_offset = 0min};

        REQUIRE(sig.format_name_email() == "John Doe <john@example.com>");
    }

    SECTION("Name with special characters") {
        Signature sig{
            .name = "José García", .email = "jose@example.com", .when = now, .tz_offset = 0min};

        REQUIRE(sig.format_name_email() == "José García <jose@example.com>");
    }
}

TEST_CASE("Signature - format_time", "[domain][signature]") {
    // Use a specific timestamp for reproducible tests
    // Unix timestamp: 1609459200 = 2021-01-01 00:00:00 UTC
    auto time_point = std::chrono::system_clock::from_time_t(1609459200);

    SECTION("UTC timezone") {
        Signature sig{
            .name = "John Doe", .email = "john@example.com", .when = time_point, .tz_offset = 0min};

        auto formatted = sig.format_time();
        REQUIRE(formatted.find("2021") != std::string::npos);
        REQUIRE(formatted.find("+0000") != std::string::npos);
    }

    SECTION("Positive timezone offset") {
        Signature sig{
            .name = "John Doe",
            .email = "john@example.com",
            .when = time_point,
            .tz_offset = 60min // +01:00
        };

        auto formatted = sig.format_time();
        REQUIRE(formatted.find("+0100") != std::string::npos);
    }

    SECTION("Negative timezone offset") {
        Signature sig{
            .name = "John Doe",
            .email = "john@example.com",
            .when = time_point,
            .tz_offset = -300min // -05:00
        };

        auto formatted = sig.format_time();
        REQUIRE(formatted.find("-0500") != std::string::npos);
    }
}

TEST_CASE("Signature - full format", "[domain][signature]") {
    auto time_point = std::chrono::system_clock::from_time_t(1609459200);

    Signature sig{
        .name = "John Doe", .email = "john@example.com", .when = time_point, .tz_offset = 0min};

    auto formatted = sig.format();

    SECTION("Contains name and email") {
        REQUIRE(formatted.find("John Doe") != std::string::npos);
        REQUIRE(formatted.find("john@example.com") != std::string::npos);
    }

    SECTION("Contains timestamp") {
        REQUIRE(formatted.find("1609459200") != std::string::npos);
    }

    SECTION("Contains timezone") {
        REQUIRE(formatted.find("+0000") != std::string::npos);
    }
}

TEST_CASE("Signature - equality", "[domain][signature]") {
    auto time_point = std::chrono::system_clock::from_time_t(1609459200);

    Signature sig1{
        .name = "John Doe", .email = "john@example.com", .when = time_point, .tz_offset = 0min};

    Signature sig2{
        .name = "John Doe", .email = "john@example.com", .when = time_point, .tz_offset = 0min};

    Signature sig3{
        .name = "Jane Doe", .email = "jane@example.com", .when = time_point, .tz_offset = 0min};

    REQUIRE(sig1 == sig2);
    REQUIRE_FALSE(sig1 == sig3);
}
