#include <repo/domain/object_id.hpp>

#include <catch2/catch_all.hpp>

using namespace repo;
using namespace repo::domain;

TEST_CASE("ObjectId - zero OID", "[domain][object_id]") {
    auto zero = ObjectId::zero();

    REQUIRE(zero.is_zero());
    REQUIRE(zero.to_string() == "0000000000000000000000000000000000000000");
}

TEST_CASE("ObjectId - parsing valid hex string", "[domain][object_id]") {
    SECTION("Full 40-character hash") {
        auto result = ObjectId::from_string("abc123def456789012345678901234567890abcd");

        REQUIRE(result.has_value());
        REQUIRE(result->to_string() == "abc123def456789012345678901234567890abcd");
    }

    SECTION("Uppercase hex") {
        auto result = ObjectId::from_string("ABC123DEF456789012345678901234567890ABCD");

        REQUIRE(result.has_value());
        REQUIRE(result->to_string() == "abc123def456789012345678901234567890abcd");
    }

    SECTION("Mixed case") {
        auto result = ObjectId::from_string("AbC123DeF456789012345678901234567890aBcD");

        REQUIRE(result.has_value());
        REQUIRE(result->to_string() == "abc123def456789012345678901234567890abcd");
    }
}

TEST_CASE("ObjectId - parsing invalid hex string", "[domain][object_id]") {
    SECTION("Too short") {
        auto result = ObjectId::from_string("abc123");
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == Error::Code::InvalidArgument);
    }

    SECTION("Too long") {
        auto result = ObjectId::from_string("abc123def456789012345678901234567890abcd00");
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == Error::Code::InvalidArgument);
    }

    SECTION("Invalid characters") {
        auto result = ObjectId::from_string("zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz");
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == Error::Code::InvalidArgument);
    }

    SECTION("Empty string") {
        auto result = ObjectId::from_string("");
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().code == Error::Code::InvalidArgument);
    }
}

TEST_CASE("ObjectId - abbreviated parsing", "[domain][object_id]") {
    // Note: Abbreviated OIDs are zero-padded for now
    // In real implementation with libgit2, these would need resolution
    SECTION("7-character abbreviation") {
        auto result = ObjectId::from_string("abc1234");

        REQUIRE(result.has_value());
        // For now, we pad with zeros
        REQUIRE(result->to_string() == "abc1234000000000000000000000000000000000");
    }

    SECTION("10-character abbreviation") {
        auto result = ObjectId::from_string("abc1234567");

        REQUIRE(result.has_value());
        REQUIRE(result->to_string() == "abc1234567000000000000000000000000000000");
    }
}

TEST_CASE("ObjectId - to_short", "[domain][object_id]") {
    auto oid = ObjectId::from_string("abc123def456789012345678901234567890abcd").value();

    SECTION("Default 7 characters") {
        REQUIRE(oid.to_short() == "abc123d");
    }

    SECTION("Custom length") {
        REQUIRE(oid.to_short(10) == "abc123def4");
        REQUIRE(oid.to_short(5) == "abc12");
        REQUIRE(oid.to_short(40) == "abc123def456789012345678901234567890abcd");
    }
}

TEST_CASE("ObjectId - equality and comparison", "[domain][object_id]") {
    auto oid1 = ObjectId::from_string("abc123def456789012345678901234567890abcd").value();
    auto oid2 = ObjectId::from_string("abc123def456789012345678901234567890abcd").value();
    auto oid3 = ObjectId::from_string("123456789abcdef0123456789abcdef012345678").value();

    SECTION("Equality") {
        REQUIRE(oid1 == oid2);
        REQUIRE_FALSE(oid1 == oid3);
    }

    SECTION("Ordering") {
        REQUIRE(oid3 < oid1); // 123... < abc...
        REQUIRE(oid1 > oid3);
    }
}

TEST_CASE("ObjectId - hash support", "[domain][object_id]") {
    auto oid1 = ObjectId::from_string("abc123def456789012345678901234567890abcd").value();
    auto oid2 = ObjectId::from_string("abc123def456789012345678901234567890abcd").value();
    auto oid3 = ObjectId::from_string("123456789abcdef0123456789abcdef012345678").value();

    std::hash<ObjectId> hasher;

    SECTION("Same OIDs have same hash") {
        REQUIRE(hasher(oid1) == hasher(oid2));
    }

    SECTION("Different OIDs (usually) have different hashes") {
        // This isn't guaranteed but highly likely
        REQUIRE(hasher(oid1) != hasher(oid3));
    }

    SECTION("Can be used in unordered containers") {
        std::unordered_map<ObjectId, std::string> map;
        map[oid1] = "commit1";
        map[oid3] = "commit2";

        REQUIRE(map.size() == 2);
        REQUIRE(map[oid1] == "commit1");
        REQUIRE(map[oid2] == "commit1"); // Same as oid1
        REQUIRE(map[oid3] == "commit2");
    }
}
