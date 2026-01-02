#include <repo/ops/branch.hpp>
#include <repo/ops/commit.hpp>
#include <repo/ops/merge.hpp>
#include <repo/ops/stage.hpp>
#include <repo/ops/switch.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <variant>

#include "../test_utils.hpp"

using namespace repo;
using namespace repo::test;

TEST_CASE("Merge - fast-forward merge", "[integration][merge]") {
    TempRepo temp;

    // Create initial commit on main
    auto commit1 = CommitBuilder(temp)
                       .with_file("file1.txt", "line 1\n")
                       .with_message("Initial commit")
                       .create();

    // Get default branch name
    auto branches = ops::list_branches(temp.repo(), {.include_remote = false});
    REQUIRE(branches.has_value());
    std::string main_branch = branches->branches[0].name;

    // Create feature branch
    REQUIRE(ops::create_branch(temp.repo(), {.name = "feature", .target = commit1}).has_value());

    // Switch to feature and add commit
    REQUIRE(ops::switch_branch(temp.repo(), {.branch_name = "feature"}).has_value());

    CommitBuilder(temp)
        .with_file("file2.txt", "feature file\n")
        .with_message("Add feature")
        .create();

    // Switch back to main
    REQUIRE(ops::switch_branch(temp.repo(), {.branch_name = main_branch}).has_value());

    // Merge feature into main (should be fast-forward)
    auto merge_result = ops::merge(temp.repo(), {.source = "feature", .message = ""});
    REQUIRE(merge_result.has_value());
    CHECK(merge_result->status == ops::MergeResult::Status::FastForward);
    CHECK(!merge_result->commit_id.empty());
    CHECK(merge_result->conflicts.empty());

    // Verify file2 now exists
    CHECK(temp.file_exists("file2.txt"));
}

TEST_CASE("Merge - no fast-forward creates merge commit", "[integration][merge]") {
    TempRepo temp;

    // Create initial commit
    auto commit1 = CommitBuilder(temp)
                       .with_file("file1.txt", "line 1\n")
                       .with_message("Initial commit")
                       .create();

    // Get default branch name
    auto branches = ops::list_branches(temp.repo(), {.include_remote = false});
    REQUIRE(branches.has_value());
    std::string main_branch = branches->branches[0].name;

    // Create and switch to feature branch
    REQUIRE(ops::create_branch(temp.repo(), {.name = "feature", .target = commit1}).has_value());
    REQUIRE(ops::switch_branch(temp.repo(), {.branch_name = "feature"}).has_value());

    // Add commit on feature
    CommitBuilder(temp)
        .with_file("file2.txt", "feature file\n")
        .with_message("Add feature")
        .create();

    // Switch back to main and add different commit
    REQUIRE(ops::switch_branch(temp.repo(), {.branch_name = main_branch}).has_value());

    CommitBuilder(temp).with_file("file3.txt", "main file\n").with_message("Add on main").create();

    // Merge feature into main (cannot fast-forward, should create merge commit)
    auto merge_result = ops::merge(temp.repo(), {.source = "feature", .message = ""});
    REQUIRE(merge_result.has_value());
    CHECK(merge_result->status == ops::MergeResult::Status::MergeCommit);
    CHECK(!merge_result->commit_id.empty());
    CHECK(merge_result->conflicts.empty());

    // Verify both files exist
    CHECK(temp.file_exists("file2.txt"));
    CHECK(temp.file_exists("file3.txt"));
}

TEST_CASE("Merge - already up to date", "[integration][merge]") {
    TempRepo temp;

    // Create initial commit
    auto commit1 = CommitBuilder(temp)
                       .with_file("file1.txt", "line 1\n")
                       .with_message("Initial commit")
                       .create();

    // Create branch pointing to same commit
    REQUIRE(ops::create_branch(temp.repo(), {.name = "feature", .target = commit1}).has_value());

    // Try to merge (should be up to date)
    auto merge_result = ops::merge(temp.repo(), {.source = "feature", .message = ""});
    REQUIRE(merge_result.has_value());
    CHECK(merge_result->status == ops::MergeResult::Status::UpToDate);
}

TEST_CASE("Merge - fast-forward only mode fails when not possible", "[integration][merge]") {
    TempRepo temp;

    // Create initial commit
    auto commit1 = CommitBuilder(temp)
                       .with_file("file1.txt", "line 1\n")
                       .with_message("Initial commit")
                       .create();

    // Get default branch name
    auto branches = ops::list_branches(temp.repo(), {.include_remote = false});
    REQUIRE(branches.has_value());
    std::string main_branch = branches->branches[0].name;

    // Create and switch to feature branch
    REQUIRE(ops::create_branch(temp.repo(), {.name = "feature", .target = commit1}).has_value());
    REQUIRE(ops::switch_branch(temp.repo(), {.branch_name = "feature"}).has_value());

    // Add commit on feature
    CommitBuilder(temp)
        .with_file("file2.txt", "feature file\n")
        .with_message("Add feature")
        .create();

    // Switch back to main and add different commit
    REQUIRE(ops::switch_branch(temp.repo(), {.branch_name = main_branch}).has_value());

    CommitBuilder(temp).with_file("file3.txt", "main file\n").with_message("Add on main").create();

    // Try to merge with FastForwardOnly (should fail)
    auto merge_result =
        ops::merge(temp.repo(),
                   {.source = "feature", .strategy = ops::MergeParams::Strategy::FastForwardOnly});

    REQUIRE(!merge_result.has_value());
    CHECK(merge_result.error().code == Error::Code::MergeError);
}
