#include <repo/ops/commit.hpp>
#include <repo/ops/tag.hpp>

#include <catch2/catch_all.hpp>

#include "../test_utils.hpp"

using namespace repo;
using namespace repo::test;

TEST_CASE("Tag - list tags in new repo (empty)", "[integration][tag]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("README.md", "# Test")
        .with_message("Initial commit")
        .create();

    // List tags (should be empty)
    auto result = ops::list_tags(temp_repo.repo());

    REQUIRE(result.has_value());
    REQUIRE(result->tags.empty());
}

TEST_CASE("Tag - create lightweight tag", "[integration][tag]") {
    TempRepo temp_repo;

    // Create initial commit
    auto commit_oid = CommitBuilder(temp_repo)
                          .with_file("file.txt", "content")
                          .with_message("Initial commit")
                          .create();

    // Create lightweight tag (no message)
    auto create_result =
        ops::create_tag(temp_repo.repo(), {.name = "v1.0.0",
                                           .target = commit_oid,
                                           .message = "", // Empty message = lightweight tag
                                           .tagger = {},  // Not needed for lightweight tags
                                           .force = false});

    REQUIRE(create_result.has_value());

    // Verify tag was created
    auto list_result = ops::list_tags(temp_repo.repo());
    REQUIRE(list_result.has_value());
    REQUIRE(list_result->tags.size() == 1);
    REQUIRE(list_result->tags[0].name == "v1.0.0");
    REQUIRE_FALSE(list_result->tags[0].is_annotated);
    REQUIRE(list_result->tags[0].target == commit_oid);
}

TEST_CASE("Tag - create annotated tag", "[integration][tag]") {
    TempRepo temp_repo;

    // Create initial commit
    auto commit_oid = CommitBuilder(temp_repo)
                          .with_file("file.txt", "content")
                          .with_message("Initial commit")
                          .create();

    // Create annotated tag
    domain::Signature tagger{.name = "Test Tagger",
                             .email = "tagger@example.com",
                             .when = std::chrono::system_clock::now(),
                             .tz_offset = std::chrono::minutes{0}};

    auto create_result = ops::create_tag(temp_repo.repo(), {.name = "v1.0.0",
                                                            .target = commit_oid,
                                                            .message = "Release version 1.0.0",
                                                            .tagger = tagger,
                                                            .force = false});

    REQUIRE(create_result.has_value());

    // Verify tag was created
    auto list_result = ops::list_tags(temp_repo.repo());
    REQUIRE(list_result.has_value());
    REQUIRE(list_result->tags.size() == 1);
    REQUIRE(list_result->tags[0].name == "v1.0.0");
    REQUIRE(list_result->tags[0].is_annotated);
    REQUIRE(list_result->tags[0].message == "Release version 1.0.0");
    REQUIRE(list_result->tags[0].target == commit_oid);
}

TEST_CASE("Tag - create multiple tags", "[integration][tag]") {
    TempRepo temp_repo;

    // Create commits
    auto commit1 = CommitBuilder(temp_repo)
                       .with_file("v1.txt", "version 1")
                       .with_message("Version 1")
                       .create();

    auto commit2 = CommitBuilder(temp_repo)
                       .with_file("v2.txt", "version 2")
                       .with_message("Version 2")
                       .create();

    auto commit3 = CommitBuilder(temp_repo)
                       .with_file("v3.txt", "version 3")
                       .with_message("Version 3")
                       .create();

    domain::Signature tagger{.name = "Test Tagger",
                             .email = "tagger@example.com",
                             .when = std::chrono::system_clock::now(),
                             .tz_offset = std::chrono::minutes{0}};

    // Create multiple tags
    ops::create_tag(temp_repo.repo(), {.name = "v1.0.0",
                                       .target = commit1,
                                       .message = "", // Lightweight
                                       .tagger = {},
                                       .force = false});

    ops::create_tag(temp_repo.repo(), {.name = "v2.0.0",
                                       .target = commit2,
                                       .message = "Release 2.0.0", // Annotated
                                       .tagger = tagger,
                                       .force = false});

    ops::create_tag(temp_repo.repo(), {.name = "v3.0.0-beta",
                                       .target = commit3,
                                       .message = "", // Lightweight
                                       .tagger = {},
                                       .force = false});

    // Verify all tags
    auto result = ops::list_tags(temp_repo.repo());
    REQUIRE(result.has_value());
    REQUIRE(result->tags.size() == 3);

    // Check tag names exist (order may vary)
    std::vector<std::string> names;
    for (const auto& tag : result->tags) {
        names.push_back(tag.name);
    }

    REQUIRE(std::find(names.begin(), names.end(), "v1.0.0") != names.end());
    REQUIRE(std::find(names.begin(), names.end(), "v2.0.0") != names.end());
    REQUIRE(std::find(names.begin(), names.end(), "v3.0.0-beta") != names.end());
}

TEST_CASE("Tag - delete tag", "[integration][tag]") {
    TempRepo temp_repo;

    // Create commit and tag
    auto commit_oid = CommitBuilder(temp_repo)
                          .with_file("file.txt", "content")
                          .with_message("Initial commit")
                          .create();

    ops::create_tag(
        temp_repo.repo(),
        {.name = "v1.0.0", .target = commit_oid, .message = "", .tagger = {}, .force = false});

    // Verify tag exists
    auto list_before = ops::list_tags(temp_repo.repo());
    REQUIRE(list_before.has_value());
    REQUIRE(list_before->tags.size() == 1);

    // Delete the tag
    auto delete_result = ops::delete_tag(temp_repo.repo(), {.name = "v1.0.0"});
    REQUIRE(delete_result.has_value());

    // Verify tag is gone
    auto list_after = ops::list_tags(temp_repo.repo());
    REQUIRE(list_after.has_value());
    REQUIRE(list_after->tags.empty());
}

TEST_CASE("Tag - delete one of multiple tags", "[integration][tag]") {
    TempRepo temp_repo;

    // Create commit
    auto commit_oid = CommitBuilder(temp_repo)
                          .with_file("file.txt", "content")
                          .with_message("Initial commit")
                          .create();

    // Create multiple tags
    ops::create_tag(
        temp_repo.repo(),
        {.name = "v1.0.0", .target = commit_oid, .message = "", .tagger = {}, .force = false});

    ops::create_tag(
        temp_repo.repo(),
        {.name = "v2.0.0", .target = commit_oid, .message = "", .tagger = {}, .force = false});

    // Delete one tag
    ops::delete_tag(temp_repo.repo(), {.name = "v1.0.0"});

    // Verify only v2.0.0 remains
    auto result = ops::list_tags(temp_repo.repo());
    REQUIRE(result.has_value());
    REQUIRE(result->tags.size() == 1);
    REQUIRE(result->tags[0].name == "v2.0.0");
}

TEST_CASE("Tag - create with empty name fails", "[integration][tag]") {
    TempRepo temp_repo;

    // Create commit
    auto commit_oid = CommitBuilder(temp_repo)
                          .with_file("file.txt", "content")
                          .with_message("Initial commit")
                          .create();

    // Try to create tag with empty name
    auto result = ops::create_tag(
        temp_repo.repo(),
        {.name = "", .target = commit_oid, .message = "", .tagger = {}, .force = false});

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == Error::Code::InvalidArgument);
}

TEST_CASE("Tag - create annotated tag with empty tagger name fails", "[integration][tag]") {
    TempRepo temp_repo;

    // Create commit
    auto commit_oid = CommitBuilder(temp_repo)
                          .with_file("file.txt", "content")
                          .with_message("Initial commit")
                          .create();

    // Try to create annotated tag with empty tagger name
    domain::Signature tagger{.name = "", // Empty name
                             .email = "tagger@example.com",
                             .when = std::chrono::system_clock::now(),
                             .tz_offset = std::chrono::minutes{0}};

    auto result = ops::create_tag(temp_repo.repo(), {.name = "v1.0.0",
                                                     .target = commit_oid,
                                                     .message = "Release 1.0.0",
                                                     .tagger = tagger,
                                                     .force = false});

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == Error::Code::InvalidArgument);
}

TEST_CASE("Tag - create annotated tag with empty tagger email fails", "[integration][tag]") {
    TempRepo temp_repo;

    // Create commit
    auto commit_oid = CommitBuilder(temp_repo)
                          .with_file("file.txt", "content")
                          .with_message("Initial commit")
                          .create();

    // Try to create annotated tag with empty tagger email
    domain::Signature tagger{.name = "Test Tagger",
                             .email = "", // Empty email
                             .when = std::chrono::system_clock::now(),
                             .tz_offset = std::chrono::minutes{0}};

    auto result = ops::create_tag(temp_repo.repo(), {.name = "v1.0.0",
                                                     .target = commit_oid,
                                                     .message = "Release 1.0.0",
                                                     .tagger = tagger,
                                                     .force = false});

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == Error::Code::InvalidArgument);
}

TEST_CASE("Tag - delete with empty name fails", "[integration][tag]") {
    TempRepo temp_repo;

    // Create commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "content")
        .with_message("Initial commit")
        .create();

    // Try to delete tag with empty name
    auto result = ops::delete_tag(temp_repo.repo(), {.name = ""});

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == Error::Code::InvalidArgument);
}

TEST_CASE("Tag - delete non-existent tag fails", "[integration][tag]") {
    TempRepo temp_repo;

    // Create commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "content")
        .with_message("Initial commit")
        .create();

    // Try to delete non-existent tag
    auto result = ops::delete_tag(temp_repo.repo(), {.name = "nonexistent"});

    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("Tag - force flag overwrites existing tag", "[integration][tag]") {
    TempRepo temp_repo;

    // Create two commits
    auto commit1 = CommitBuilder(temp_repo)
                       .with_file("v1.txt", "version 1")
                       .with_message("Version 1")
                       .create();

    auto commit2 = CommitBuilder(temp_repo)
                       .with_file("v2.txt", "version 2")
                       .with_message("Version 2")
                       .create();

    // Create tag pointing to commit1
    ops::create_tag(
        temp_repo.repo(),
        {.name = "v1.0.0", .target = commit1, .message = "", .tagger = {}, .force = false});

    // Verify tag points to commit1
    auto list1 = ops::list_tags(temp_repo.repo());
    REQUIRE(list1.has_value());
    REQUIRE(list1->tags[0].target == commit1);

    // Try to create same tag without force (should fail)
    auto create_without_force = ops::create_tag(
        temp_repo.repo(),
        {.name = "v1.0.0", .target = commit2, .message = "", .tagger = {}, .force = false});

    REQUIRE_FALSE(create_without_force.has_value());

    // Create same tag with force (should succeed)
    auto create_with_force = ops::create_tag(
        temp_repo.repo(),
        {.name = "v1.0.0", .target = commit2, .message = "", .tagger = {}, .force = true});

    REQUIRE(create_with_force.has_value());

    // Verify tag now points to commit2
    auto list2 = ops::list_tags(temp_repo.repo());
    REQUIRE(list2.has_value());
    REQUIRE(list2->tags.size() == 1);
    REQUIRE(list2->tags[0].target == commit2);
}
