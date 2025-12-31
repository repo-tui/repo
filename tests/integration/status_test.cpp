#include <repo/ops/commit.hpp>
#include <repo/ops/stage.hpp>
#include <repo/ops/status.hpp>

#include <catch2/catch_all.hpp>

#include "../test_utils.hpp"

using namespace repo;
using namespace repo::test;

TEST_CASE("Status - clean repository", "[integration][status]") {
    TempRepo temp_repo;

    // Create initial commit to avoid empty repository
    CommitBuilder(temp_repo)
        .with_file("README.md", "# Test Project")
        .with_message("Initial commit")
        .create();

    auto result = ops::status(temp_repo.repo());
    REQUIRE(result.has_value());
    REQUIRE(result->is_clean());
    REQUIRE(result->files.empty());
}

TEST_CASE("Status - untracked file", "[integration][status]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("README.md", "# Test Project")
        .with_message("Initial commit")
        .create();

    // Add an untracked file
    temp_repo.write_file("new_file.txt", "This is a new file");

    auto result = ops::status(temp_repo.repo());
    REQUIRE(result.has_value());
    REQUIRE_FALSE(result->is_clean());

    auto untracked = result->untracked();
    REQUIRE(untracked.size() == 1);
    REQUIRE(untracked[0].path == "new_file.txt");
    REQUIRE(untracked[0].is_untracked());
}

TEST_CASE("Status - staged file", "[integration][status]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("README.md", "# Test Project")
        .with_message("Initial commit")
        .create();

    // Add and stage a new file
    temp_repo.write_file("staged.txt", "This is a staged file");
    auto stage_result = ops::stage(temp_repo.repo(), {.paths = {"staged.txt"}});
    REQUIRE(stage_result.has_value());

    auto result = ops::status(temp_repo.repo());
    REQUIRE(result.has_value());
    REQUIRE_FALSE(result->is_clean());

    auto staged = result->staged();
    REQUIRE(staged.size() == 1);
    REQUIRE(staged[0].path == "staged.txt");
    REQUIRE(staged[0].is_staged());
    REQUIRE(staged[0].index_status == domain::FileStatus::State::Added);
}

TEST_CASE("Status - modified file (unstaged)", "[integration][status]") {
    TempRepo temp_repo;

    // Create initial commit with a file
    CommitBuilder(temp_repo)
        .with_file("file.txt", "Original content")
        .with_message("Initial commit")
        .create();

    // Modify the file
    temp_repo.write_file("file.txt", "Modified content");

    auto result = ops::status(temp_repo.repo());
    REQUIRE(result.has_value());
    REQUIRE_FALSE(result->is_clean());

    auto unstaged = result->unstaged();
    REQUIRE(unstaged.size() == 1);
    REQUIRE(unstaged[0].path == "file.txt");
    REQUIRE(unstaged[0].is_unstaged());
    REQUIRE(unstaged[0].worktree_status == domain::FileStatus::State::Modified);
}

TEST_CASE("Status - modified file (staged)", "[integration][status]") {
    TempRepo temp_repo;

    // Create initial commit with a file
    CommitBuilder(temp_repo)
        .with_file("file.txt", "Original content")
        .with_message("Initial commit")
        .create();

    // Modify and stage the file
    temp_repo.write_file("file.txt", "Modified content");
    auto stage_result = ops::stage(temp_repo.repo(), {.paths = {"file.txt"}});
    REQUIRE(stage_result.has_value());

    auto result = ops::status(temp_repo.repo());
    REQUIRE(result.has_value());
    REQUIRE_FALSE(result->is_clean());

    auto staged = result->staged();
    REQUIRE(staged.size() == 1);
    REQUIRE(staged[0].path == "file.txt");
    REQUIRE(staged[0].is_staged());
    REQUIRE(staged[0].index_status == domain::FileStatus::State::Modified);
}

TEST_CASE("Status - deleted file (unstaged)", "[integration][status]") {
    TempRepo temp_repo;

    // Create initial commit with a file
    CommitBuilder(temp_repo)
        .with_file("file.txt", "Content")
        .with_message("Initial commit")
        .create();

    // Delete the file
    temp_repo.delete_file("file.txt");

    auto result = ops::status(temp_repo.repo());
    REQUIRE(result.has_value());
    REQUIRE_FALSE(result->is_clean());

    auto unstaged = result->unstaged();
    REQUIRE(unstaged.size() == 1);
    REQUIRE(unstaged[0].path == "file.txt");
    REQUIRE(unstaged[0].is_unstaged());
    REQUIRE(unstaged[0].worktree_status == domain::FileStatus::State::Deleted);
}

TEST_CASE("Status - mixed states", "[integration][status]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("existing.txt", "Existing file")
        .with_message("Initial commit")
        .create();

    // Create various file states
    temp_repo.write_file("untracked.txt", "Untracked"); // Untracked
    temp_repo.write_file("existing.txt", "Modified");   // Modified unstaged
    temp_repo.write_file("staged.txt", "Staged");       // To be staged

    auto stage_result = ops::stage(temp_repo.repo(), {.paths = {"staged.txt"}});
    REQUIRE(stage_result.has_value());

    auto result = ops::status(temp_repo.repo());
    REQUIRE(result.has_value());
    REQUIRE_FALSE(result->is_clean());
    REQUIRE(result->files.size() >= 3);

    // Check untracked
    auto untracked = result->untracked();
    REQUIRE(untracked.size() == 1);
    REQUIRE(untracked[0].path == "untracked.txt");

    // Check staged
    auto staged = result->staged();
    REQUIRE(staged.size() == 1);
    REQUIRE(staged[0].path == "staged.txt");

    // Check unstaged
    auto unstaged = result->unstaged();
    REQUIRE(unstaged.size() == 1);
    REQUIRE(unstaged[0].path == "existing.txt");
}
