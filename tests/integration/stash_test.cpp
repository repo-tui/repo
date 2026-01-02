#include <repo/ops/stash.hpp>

#include <catch2/catch_all.hpp>

#include <fstream>

#include "../test_utils.hpp"

using namespace repo;
using namespace repo::test;

TEST_CASE("Stash - list stashes in new repo (empty)", "[integration][stash]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("README.md", "# Test")
        .with_message("Initial commit")
        .create();

    // List stashes (should be empty)
    auto result = ops::list_stashes(temp_repo.repo());

    REQUIRE(result.has_value());
    REQUIRE(result->stashes.empty());
}

TEST_CASE("Stash - create stash with modified file", "[integration][stash]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "original content")
        .with_message("Initial commit")
        .create();

    // Modify file
    auto file_path = temp_repo.path() / "file.txt";
    std::ofstream(file_path) << "modified content";

    // Create stash
    domain::Signature stasher{.name = "Test Stasher",
                              .email = "stasher@example.com",
                              .when = std::chrono::system_clock::now(),
                          .tz_offset = std::chrono::minutes{0}};

    auto create_result = ops::create_stash(temp_repo.repo(), {.message = "WIP: testing",
                                                              .stasher = stasher,
                                                              .include_untracked = false,
                                                              .keep_index = false});

    REQUIRE(create_result.has_value());

    // Verify stash was created
    auto list_result = ops::list_stashes(temp_repo.repo());
    REQUIRE(list_result.has_value());
    REQUIRE(list_result->stashes.size() == 1);
    REQUIRE(list_result->stashes[0].index == 0);
    REQUIRE(list_result->stashes[0].message.find("WIP: testing") != std::string::npos);
}

TEST_CASE("Stash - working directory is restored after stash", "[integration][stash]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "original content")
        .with_message("Initial commit")
        .create();

    // Modify file
    auto file_path = temp_repo.path() / "file.txt";
    std::ofstream(file_path) << "modified content";

    // Create stash
    domain::Signature stasher{.name = "Test Stasher",
                              .email = "stasher@example.com",
                              .when = std::chrono::system_clock::now(),
                          .tz_offset = std::chrono::minutes{0}};

    ops::create_stash(temp_repo.repo(), {.message = "WIP: testing",
                                         .stasher = stasher,
                                         .include_untracked = false,
                                         .keep_index = false});

    // Verify file is restored to original content
    std::ifstream file(file_path);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    REQUIRE(content == "original content");
}

TEST_CASE("Stash - apply stash restores changes", "[integration][stash]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "original content")
        .with_message("Initial commit")
        .create();

    // Modify file
    auto file_path = temp_repo.path() / "file.txt";
    std::ofstream(file_path) << "modified content";

    // Create stash
    domain::Signature stasher{.name = "Test Stasher",
                              .email = "stasher@example.com",
                              .when = std::chrono::system_clock::now(),
                          .tz_offset = std::chrono::minutes{0}};

    ops::create_stash(temp_repo.repo(), {.message = "WIP: testing",
                                         .stasher = stasher,
                                         .include_untracked = false,
                                         .keep_index = false});

    // Apply stash
    auto apply_result = ops::apply_stash(temp_repo.repo(), {.index = 0, .reinstate_index = false});

    REQUIRE(apply_result.has_value());

    // Verify changes are restored
    std::ifstream file(file_path);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    REQUIRE(content == "modified content");

    // Verify stash still exists
    auto list_result = ops::list_stashes(temp_repo.repo());
    REQUIRE(list_result.has_value());
    REQUIRE(list_result->stashes.size() == 1);
}

TEST_CASE("Stash - pop stash restores and removes", "[integration][stash]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "original content")
        .with_message("Initial commit")
        .create();

    // Modify file
    auto file_path = temp_repo.path() / "file.txt";
    std::ofstream(file_path) << "modified content";

    // Create stash
    domain::Signature stasher{.name = "Test Stasher",
                              .email = "stasher@example.com",
                              .when = std::chrono::system_clock::now(),
                          .tz_offset = std::chrono::minutes{0}};

    ops::create_stash(temp_repo.repo(), {.message = "WIP: testing",
                                         .stasher = stasher,
                                         .include_untracked = false,
                                         .keep_index = false});

    // Pop stash
    auto pop_result = ops::pop_stash(temp_repo.repo(), {.index = 0, .reinstate_index = false});

    REQUIRE(pop_result.has_value());

    // Verify changes are restored
    std::ifstream file(file_path);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    REQUIRE(content == "modified content");

    // Verify stash was removed
    auto list_result = ops::list_stashes(temp_repo.repo());
    REQUIRE(list_result.has_value());
    REQUIRE(list_result->stashes.empty());
}

TEST_CASE("Stash - drop stash removes without applying", "[integration][stash]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "original content")
        .with_message("Initial commit")
        .create();

    // Modify file
    auto file_path = temp_repo.path() / "file.txt";
    std::ofstream(file_path) << "modified content";

    // Create stash
    domain::Signature stasher{.name = "Test Stasher",
                              .email = "stasher@example.com",
                              .when = std::chrono::system_clock::now(),
                          .tz_offset = std::chrono::minutes{0}};

    ops::create_stash(temp_repo.repo(), {.message = "WIP: testing",
                                         .stasher = stasher,
                                         .include_untracked = false,
                                         .keep_index = false});

    // Verify file is back to original
    std::ifstream before_drop(file_path);
    std::string before_content((std::istreambuf_iterator<char>(before_drop)),
                               std::istreambuf_iterator<char>());
    REQUIRE(before_content == "original content");

    // Drop stash
    auto drop_result = ops::drop_stash(temp_repo.repo(), {.index = 0});
    REQUIRE(drop_result.has_value());

    // Verify stash was removed
    auto list_result = ops::list_stashes(temp_repo.repo());
    REQUIRE(list_result.has_value());
    REQUIRE(list_result->stashes.empty());

    // Verify file is still original (not modified)
    std::ifstream after_drop(file_path);
    std::string after_content((std::istreambuf_iterator<char>(after_drop)),
                              std::istreambuf_iterator<char>());
    REQUIRE(after_content == "original content");
}

TEST_CASE("Stash - multiple stashes", "[integration][stash]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "original")
        .with_message("Initial commit")
        .create();

    domain::Signature stasher{.name = "Test Stasher",
                              .email = "stasher@example.com",
                              .when = std::chrono::system_clock::now(),
                          .tz_offset = std::chrono::minutes{0}};

    auto file_path = temp_repo.path() / "file.txt";

    // Create first stash
    std::ofstream(file_path) << "change 1";
    ops::create_stash(temp_repo.repo(), {.message = "Stash 1",
                                         .stasher = stasher,
                                         .include_untracked = false,
                                         .keep_index = false});

    // Create second stash
    std::ofstream(file_path) << "change 2";
    ops::create_stash(temp_repo.repo(), {.message = "Stash 2",
                                         .stasher = stasher,
                                         .include_untracked = false,
                                         .keep_index = false});

    // Create third stash
    std::ofstream(file_path) << "change 3";
    ops::create_stash(temp_repo.repo(), {.message = "Stash 3",
                                         .stasher = stasher,
                                         .include_untracked = false,
                                         .keep_index = false});

    // Verify all stashes exist
    auto result = ops::list_stashes(temp_repo.repo());
    REQUIRE(result.has_value());
    REQUIRE(result->stashes.size() == 3);

    // Verify order (0 = most recent)
    REQUIRE(result->stashes[0].message.find("Stash 3") != std::string::npos);
    REQUIRE(result->stashes[1].message.find("Stash 2") != std::string::npos);
    REQUIRE(result->stashes[2].message.find("Stash 1") != std::string::npos);
}

TEST_CASE("Stash - create with empty message fails", "[integration][stash]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "content")
        .with_message("Initial commit")
        .create();

    // Modify file
    auto file_path = temp_repo.path() / "file.txt";
    std::ofstream(file_path) << "modified";

    domain::Signature stasher{.name = "Test Stasher",
                              .email = "stasher@example.com",
                              .when = std::chrono::system_clock::now(),
                          .tz_offset = std::chrono::minutes{0}};

    // Try to create stash with empty message
    auto result = ops::create_stash(
        temp_repo.repo(),
        {.message = "", .stasher = stasher, .include_untracked = false, .keep_index = false});

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == Error::Code::InvalidArgument);
}

TEST_CASE("Stash - create with empty stasher name fails", "[integration][stash]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "content")
        .with_message("Initial commit")
        .create();

    // Modify file
    auto file_path = temp_repo.path() / "file.txt";
    std::ofstream(file_path) << "modified";

    domain::Signature stasher{.name = "", // Empty name
                              .email = "stasher@example.com",
                              .when = std::chrono::system_clock::now(),
                          .tz_offset = std::chrono::minutes{0}};

    // Try to create stash
    auto result = ops::create_stash(
        temp_repo.repo(),
        {.message = "WIP", .stasher = stasher, .include_untracked = false, .keep_index = false});

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == Error::Code::InvalidArgument);
}

TEST_CASE("Stash - create with empty stasher email fails", "[integration][stash]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "content")
        .with_message("Initial commit")
        .create();

    // Modify file
    auto file_path = temp_repo.path() / "file.txt";
    std::ofstream(file_path) << "modified";

    domain::Signature stasher{.name = "Test Stasher",
                              .email = "", // Empty email
                              .when = std::chrono::system_clock::now(),
                          .tz_offset = std::chrono::minutes{0}};

    // Try to create stash
    auto result = ops::create_stash(
        temp_repo.repo(),
        {.message = "WIP", .stasher = stasher, .include_untracked = false, .keep_index = false});

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == Error::Code::InvalidArgument);
}

TEST_CASE("Stash - create with no changes fails", "[integration][stash]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "content")
        .with_message("Initial commit")
        .create();

    domain::Signature stasher{.name = "Test Stasher",
                              .email = "stasher@example.com",
                              .when = std::chrono::system_clock::now(),
                          .tz_offset = std::chrono::minutes{0}};

    // Try to create stash with no changes
    auto result = ops::create_stash(
        temp_repo.repo(),
        {.message = "WIP", .stasher = stasher, .include_untracked = false, .keep_index = false});

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == Error::Code::NothingToCommit);
}

TEST_CASE("Stash - apply non-existent stash fails", "[integration][stash]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "content")
        .with_message("Initial commit")
        .create();

    // Try to apply non-existent stash
    auto result = ops::apply_stash(temp_repo.repo(), {.index = 0, .reinstate_index = false});

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == Error::Code::ObjectNotFound);
}

TEST_CASE("Stash - drop non-existent stash fails", "[integration][stash]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "content")
        .with_message("Initial commit")
        .create();

    // Try to drop non-existent stash
    auto result = ops::drop_stash(temp_repo.repo(), {.index = 0});

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == Error::Code::ObjectNotFound);
}

TEST_CASE("Stash - include_untracked flag", "[integration][stash]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("tracked.txt", "tracked")
        .with_message("Initial commit")
        .create();

    // Create untracked file
    auto untracked_path = temp_repo.path() / "untracked.txt";
    std::ofstream(untracked_path) << "untracked content";

    domain::Signature stasher{.name = "Test Stasher",
                              .email = "stasher@example.com",
                              .when = std::chrono::system_clock::now(),
                          .tz_offset = std::chrono::minutes{0}};

    // Create stash with include_untracked
    auto result = ops::create_stash(temp_repo.repo(), {.message = "WIP with untracked",
                                                       .stasher = stasher,
                                                       .include_untracked = true,
                                                       .keep_index = false});

    REQUIRE(result.has_value());

    // Verify untracked file is removed
    REQUIRE_FALSE(std::filesystem::exists(untracked_path));

    // Pop stash to restore
    ops::pop_stash(temp_repo.repo(), {.index = 0, .reinstate_index = false});

    // Verify untracked file is restored
    REQUIRE(std::filesystem::exists(untracked_path));
}
