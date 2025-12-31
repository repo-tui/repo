#include <repo/ops/branch.hpp>
#include <repo/ops/commit.hpp>
#include <repo/ops/list_commits.hpp>

#include <catch2/catch_all.hpp>

#include "../test_utils.hpp"

using namespace repo;
using namespace repo::test;

TEST_CASE("List commits - single commit", "[integration][list-commits]") {
    TempRepo temp_repo;

    // Create a commit
    auto commit_id = CommitBuilder(temp_repo)
                         .with_file("README.md", "# Test Project")
                         .with_message("Initial commit")
                         .create();

    // Get commit list
    auto result = ops::list_commits(temp_repo.repo());
    REQUIRE(result.has_value());
    REQUIRE(result->commits.size() == 1);

    // Verify the commit
    REQUIRE(result->commits[0].id == commit_id);
    REQUIRE(result->commits[0].message == "Initial commit");
    REQUIRE(result->commits[0].is_root());

    // Test helper methods
    auto* first = result->first();
    REQUIRE(first != nullptr);
    REQUIRE(first->id == commit_id);

    auto* last = result->last();
    REQUIRE(last != nullptr);
    REQUIRE(last->id == commit_id);
}

TEST_CASE("List commits - multiple commits", "[integration][list-commits]") {
    TempRepo temp_repo;

    // Create multiple commits
    auto commit1 = CommitBuilder(temp_repo)
                       .with_file("file1.txt", "First")
                       .with_message("First commit")
                       .create();

    auto commit2 = CommitBuilder(temp_repo)
                       .with_file("file2.txt", "Second")
                       .with_message("Second commit")
                       .create();

    auto commit3 = CommitBuilder(temp_repo)
                       .with_file("file3.txt", "Third")
                       .with_message("Third commit")
                       .create();

    // Get commit list
    auto result = ops::list_commits(temp_repo.repo());
    REQUIRE(result.has_value());
    REQUIRE(result->commits.size() == 3);

    // Commits should be in reverse chronological order (newest first)
    REQUIRE(result->commits[0].id == commit3);
    REQUIRE(result->commits[0].message == "Third commit");
    REQUIRE(result->commits[0].parent_ids.size() == 1);

    REQUIRE(result->commits[1].id == commit2);
    REQUIRE(result->commits[1].message == "Second commit");

    REQUIRE(result->commits[2].id == commit1);
    REQUIRE(result->commits[2].message == "First commit");
    REQUIRE(result->commits[2].is_root());
}

TEST_CASE("List commits - limit max count", "[integration][list-commits]") {
    TempRepo temp_repo;

    // Create 5 commits
    CommitBuilder(temp_repo).with_file("file1.txt", "1").with_message("Commit 1").create();

    CommitBuilder(temp_repo).with_file("file2.txt", "2").with_message("Commit 2").create();

    CommitBuilder(temp_repo).with_file("file3.txt", "3").with_message("Commit 3").create();

    CommitBuilder(temp_repo).with_file("file4.txt", "4").with_message("Commit 4").create();

    CommitBuilder(temp_repo).with_file("file5.txt", "5").with_message("Commit 5").create();

    // Get only the last 3 commits
    auto result = ops::list_commits(temp_repo.repo(), {.max_count = 3});
    REQUIRE(result.has_value());
    REQUIRE(result->commits.size() == 3);

    // Should have commits 5, 4, and 3 (most recent)
    REQUIRE(result->commits[0].message == "Commit 5");
    REQUIRE(result->commits[1].message == "Commit 4");
    REQUIRE(result->commits[2].message == "Commit 3");
}

TEST_CASE("List commits - commit details", "[integration][list-commits]") {
    TempRepo temp_repo;

    // Create a commit
    auto commit_id = CommitBuilder(temp_repo)
                         .with_file("README.md", "# Test")
                         .with_message("Test commit message")
                         .create();

    // Get commit list
    auto result = ops::list_commits(temp_repo.repo());
    REQUIRE(result.has_value());
    REQUIRE(result->commits.size() == 1);

    auto& commit = result->commits[0];

    // Verify commit details
    REQUIRE(commit.id == commit_id);
    REQUIRE(commit.message == "Test commit message");
    REQUIRE(commit.author.name == "Test User");
    REQUIRE(commit.author.email == "test@example.com");
    REQUIRE(commit.committer.name == "Test User");
    REQUIRE(commit.committer.email == "test@example.com");
    REQUIRE_FALSE(commit.tree_id.is_zero());
    REQUIRE(commit.parent_ids.empty());
}

TEST_CASE("List commits - parent relationships", "[integration][list-commits]") {
    TempRepo temp_repo;

    // Create commits
    auto commit1 =
        CommitBuilder(temp_repo).with_file("file1.txt", "1").with_message("First").create();

    auto commit2 =
        CommitBuilder(temp_repo).with_file("file2.txt", "2").with_message("Second").create();

    // Get commit list
    auto result = ops::list_commits(temp_repo.repo());
    REQUIRE(result.has_value());
    REQUIRE(result->commits.size() == 2);

    // Second commit should have first as parent
    auto& second = result->commits[0];
    REQUIRE(second.id == commit2);
    REQUIRE(second.parent_ids.size() == 1);
    REQUIRE(second.parent_ids[0] == commit1);
    REQUIRE_FALSE(second.is_root());

    // First commit should have no parents
    auto& first = result->commits[1];
    REQUIRE(first.id == commit1);
    REQUIRE(first.parent_ids.empty());
    REQUIRE(first.is_root());
}

TEST_CASE("List commits - from specific branch", "[integration][list-commits]") {
    TempRepo temp_repo;

    // Create commits on main
    auto main_commit =
        CommitBuilder(temp_repo).with_file("main.txt", "main").with_message("Main commit").create();

    // Create a branch
    auto branch_result =
        ops::create_branch(temp_repo.repo(), {.name = "feature", .target = main_commit});
    REQUIRE(branch_result.has_value());

    // The current HEAD should still point to main
    // Get commit list from main branch
    auto main_log = ops::list_commits(temp_repo.repo(), {.ref_name = "HEAD"});
    REQUIRE(main_log.has_value());
    REQUIRE(main_log->commits.size() >= 1);
    REQUIRE(main_log->commits[0].id == main_commit);
}
