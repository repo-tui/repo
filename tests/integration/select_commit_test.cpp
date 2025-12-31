#include <repo/ops/rollback.hpp>
#include <repo/ops/select_commit.hpp>
#include <repo/ops/stage.hpp>
#include <repo/ops/status.hpp>

#include <catch2/catch_all.hpp>

#include <fstream>

#include "../test_utils.hpp"

using namespace repo;
using namespace repo::test;

TEST_CASE("Select commit - apply single commit to current branch", "[integration][select-commit]") {
    TempRepo temp_repo;

    // Create initial commit on main
    CommitBuilder(temp_repo)
        .with_file("base.txt", "base content")
        .with_message("Initial commit")
        .create();

    // Create a commit with a new file
    auto target_commit = CommitBuilder(temp_repo)
                             .with_file("feature.txt", "feature content")
                             .with_message("Add feature")
                             .create();

    // Create another commit
    CommitBuilder(temp_repo)
        .with_file("other.txt", "other content")
        .with_message("Add other file")
        .create();

    // Rollback to initial commit (before target_commit)
    ops::rollback(temp_repo.repo(), {.target = target_commit, .mode = ops::RollbackMode::Hard});

    // Now go back one more
    temp_repo.run_git({"reset", "--hard", "HEAD~1"});

    // Apply the feature commit to current branch
    auto result =
        ops::select_commit(temp_repo.repo(), {.commit = target_commit, .no_commit = false});

    REQUIRE(result.has_value());

    // Verify feature.txt exists
    auto feature_path = temp_repo.path() / "feature.txt";
    REQUIRE(std::filesystem::exists(feature_path));

    std::ifstream file(feature_path);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    REQUIRE(content == "feature content");
}

TEST_CASE("Select commit - apply commit modifying existing file", "[integration][select-commit]") {
    TempRepo temp_repo;

    // Create initial commit
    auto base_commit = CommitBuilder(temp_repo)
                           .with_file("file.txt", "line 1\nline 2\nline 3\n")
                           .with_message("Initial commit")
                           .create();

    // Create a commit that modifies the file
    auto modify_commit = CommitBuilder(temp_repo)
                             .with_file("file.txt", "line 1\nMODIFIED\nline 3\n")
                             .with_message("Modify line 2")
                             .create();

    // Reset back to base
    ops::rollback(temp_repo.repo(), {.target = base_commit, .mode = ops::RollbackMode::Hard});

    // Apply the modification
    auto result =
        ops::select_commit(temp_repo.repo(), {.commit = modify_commit, .no_commit = false});

    REQUIRE(result.has_value());

    // Verify file has been modified
    auto file_path = temp_repo.path() / "file.txt";
    std::ifstream file(file_path);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    REQUIRE(content == "line 1\nMODIFIED\nline 3\n");
}

TEST_CASE("Select commit - with no_commit flag", "[integration][select-commit]") {
    TempRepo temp_repo;

    // Create initial commit
    auto base_commit = CommitBuilder(temp_repo)
                           .with_file("base.txt", "base")
                           .with_message("Initial commit")
                           .create();

    // Create a commit with a new file
    auto feature_commit = CommitBuilder(temp_repo)
                              .with_file("feature.txt", "feature")
                              .with_message("Add feature")
                              .create();

    // Reset back to base
    ops::rollback(temp_repo.repo(), {.target = base_commit, .mode = ops::RollbackMode::Hard});

    // Apply commit without creating new commit
    auto result =
        ops::select_commit(temp_repo.repo(), {.commit = feature_commit, .no_commit = true});

    REQUIRE(result.has_value());

    // Verify changes are staged but not committed
    auto status = ops::status(temp_repo.repo());
    REQUIRE(status.has_value());
    auto staged = status->staged();
    REQUIRE(staged.size() == 1);
    REQUIRE(staged[0].path == "feature.txt");
}

TEST_CASE("Select commit - apply multiple commits in sequence", "[integration][select-commit]") {
    TempRepo temp_repo;

    // Create base commit
    auto base =
        CommitBuilder(temp_repo).with_file("base.txt", "base").with_message("Base").create();

    // Create first feature commit
    auto feature1 = CommitBuilder(temp_repo)
                        .with_file("feature1.txt", "feature 1")
                        .with_message("Feature 1")
                        .create();

    // Create second feature commit
    auto feature2 = CommitBuilder(temp_repo)
                        .with_file("feature2.txt", "feature 2")
                        .with_message("Feature 2")
                        .create();

    // Rollback to base
    ops::rollback(temp_repo.repo(), {.target = base, .mode = ops::RollbackMode::Hard});

    // Apply both feature commits
    auto result1 = ops::select_commit(temp_repo.repo(), {.commit = feature1});
    REQUIRE(result1.has_value());

    auto result2 = ops::select_commit(temp_repo.repo(), {.commit = feature2});
    REQUIRE(result2.has_value());

    // Verify both files exist
    REQUIRE(std::filesystem::exists(temp_repo.path() / "feature1.txt"));
    REQUIRE(std::filesystem::exists(temp_repo.path() / "feature2.txt"));
}

TEST_CASE("Select commit - apply commit from different branch", "[integration][select-commit]") {
    TempRepo temp_repo;

    // Create initial commit on main
    auto main_base = CommitBuilder(temp_repo)
                         .with_file("main.txt", "main content")
                         .with_message("Main base")
                         .create();

    // Create feature commit
    auto feature_commit = CommitBuilder(temp_repo)
                              .with_file("feature.txt", "feature from branch")
                              .with_message("Feature commit")
                              .create();

    // Go back to main_base
    ops::rollback(temp_repo.repo(), {.target = main_base, .mode = ops::RollbackMode::Hard});

    // Add different content on main
    CommitBuilder(temp_repo)
        .with_file("main2.txt", "main continues")
        .with_message("Main development")
        .create();

    // Apply the feature commit
    auto result =
        ops::select_commit(temp_repo.repo(), {.commit = feature_commit, .no_commit = false});

    REQUIRE(result.has_value());

    // Verify we have both main2.txt and feature.txt
    REQUIRE(std::filesystem::exists(temp_repo.path() / "main2.txt"));
    REQUIRE(std::filesystem::exists(temp_repo.path() / "feature.txt"));
}

TEST_CASE("Select commit - invalid commit OID fails", "[integration][select-commit]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "content")
        .with_message("Initial commit")
        .create();

    // Try to apply invalid OID
    auto invalid_oid =
        domain::ObjectId::from_string("0000000000000000000000000000000000000001").value();

    auto result = ops::select_commit(temp_repo.repo(), {.commit = invalid_oid, .no_commit = false});

    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("Select commit - apply commit with file deletion", "[integration][select-commit]") {
    TempRepo temp_repo;

    // Create base with two files
    auto base = CommitBuilder(temp_repo)
                    .with_file("keep.txt", "keep this")
                    .with_file("delete.txt", "delete this")
                    .with_message("Base with two files")
                    .create();

    // Create commit that deletes one file
    std::filesystem::remove(temp_repo.path() / "delete.txt");
    ops::stage(temp_repo.repo(), {.paths = {"delete.txt"}});
    auto delete_commit = CommitBuilder(temp_repo).with_message("Delete file").create();

    // Rollback to base
    ops::rollback(temp_repo.repo(), {.target = base, .mode = ops::RollbackMode::Hard});

    // Apply the deletion
    auto result =
        ops::select_commit(temp_repo.repo(), {.commit = delete_commit, .no_commit = false});

    REQUIRE(result.has_value());

    // Verify delete.txt is gone
    REQUIRE_FALSE(std::filesystem::exists(temp_repo.path() / "delete.txt"));
    // Verify keep.txt still exists
    REQUIRE(std::filesystem::exists(temp_repo.path() / "keep.txt"));
}
