#include <repo/ops/branch.hpp>
#include <repo/ops/commit.hpp>

#include <catch2/catch_all.hpp>

#include "../test_utils.hpp"

using namespace repo;
using namespace repo::test;

TEST_CASE("Branch - list branches in new repo", "[integration][branch]") {
    TempRepo temp_repo;

    // Create initial commit so we have a default branch
    CommitBuilder(temp_repo)
        .with_file("README.md", "# Test Project")
        .with_message("Initial commit")
        .create();

    auto result = ops::list_branches(temp_repo.repo());
    REQUIRE(result.has_value());

    // Should have at least one branch (the default branch)
    REQUIRE(result->branches.size() >= 1);

    // Should have a current branch
    auto* current = result->current();
    REQUIRE(current != nullptr);
    REQUIRE(current->is_head);
}

TEST_CASE("Branch - create new branch", "[integration][branch]") {
    TempRepo temp_repo;

    // Create initial commit
    auto commit_id = CommitBuilder(temp_repo)
                         .with_file("README.md", "# Test Project")
                         .with_message("Initial commit")
                         .create();

    // Create a new branch
    auto create_result = ops::create_branch(
        temp_repo.repo(), {.name = "feature-branch", .target = commit_id, .force = false});
    REQUIRE(create_result.has_value());
    REQUIRE(create_result->branch.name == "feature-branch");
    REQUIRE(create_result->branch.target == commit_id);
    REQUIRE_FALSE(create_result->branch.is_remote);

    // List branches and verify it exists
    auto list_result = ops::list_branches(temp_repo.repo());
    REQUIRE(list_result.has_value());
    REQUIRE(list_result->branches.size() >= 2);

    // Find the new branch
    bool found = false;
    for (const auto& branch : list_result->branches) {
        if (branch.name == "feature-branch") {
            found = true;
            REQUIRE(branch.target == commit_id);
            break;
        }
    }
    REQUIRE(found);
}

TEST_CASE("Branch - delete branch", "[integration][branch]") {
    TempRepo temp_repo;

    // Create initial commit
    auto commit_id = CommitBuilder(temp_repo)
                         .with_file("README.md", "# Test Project")
                         .with_message("Initial commit")
                         .create();

    // Create a branch to delete
    auto create_result =
        ops::create_branch(temp_repo.repo(), {.name = "temp-branch", .target = commit_id});
    REQUIRE(create_result.has_value());

    // Verify it exists
    auto list_before = ops::list_branches(temp_repo.repo());
    REQUIRE(list_before.has_value());
    size_t count_before = list_before->branches.size();

    // Delete the branch
    auto delete_result = ops::delete_branch(temp_repo.repo(), {.name = "temp-branch"});
    REQUIRE(delete_result.has_value());

    // Verify it's gone
    auto list_after = ops::list_branches(temp_repo.repo());
    REQUIRE(list_after.has_value());
    REQUIRE(list_after->branches.size() == count_before - 1);

    // Make sure the branch is not in the list
    for (const auto& branch : list_after->branches) {
        REQUIRE(branch.name != "temp-branch");
    }
}

TEST_CASE("Branch - create branch with force", "[integration][branch]") {
    TempRepo temp_repo;

    // Create two commits
    auto commit1 = CommitBuilder(temp_repo)
                       .with_file("file1.txt", "First")
                       .with_message("First commit")
                       .create();

    auto commit2 = CommitBuilder(temp_repo)
                       .with_file("file2.txt", "Second")
                       .with_message("Second commit")
                       .create();

    // Create a branch pointing to commit1
    auto create1 = ops::create_branch(temp_repo.repo(),
                                      {.name = "test-branch", .target = commit1, .force = false});
    REQUIRE(create1.has_value());
    REQUIRE(create1->branch.target == commit1);

    // Try to create it again without force (should fail)
    auto create2 = ops::create_branch(temp_repo.repo(),
                                      {.name = "test-branch", .target = commit2, .force = false});
    REQUIRE_FALSE(create2.has_value());

    // Create it again with force (should succeed and update)
    auto create3 = ops::create_branch(temp_repo.repo(),
                                      {.name = "test-branch", .target = commit2, .force = true});
    REQUIRE(create3.has_value());
    REQUIRE(create3->branch.target == commit2);
}

TEST_CASE("Branch - list local vs remote", "[integration][branch]") {
    TempRepo temp_repo;

    // Create initial commit
    auto commit_id = CommitBuilder(temp_repo)
                         .with_file("README.md", "# Test Project")
                         .with_message("Initial commit")
                         .create();

    // Create a local branch
    auto create_result =
        ops::create_branch(temp_repo.repo(), {.name = "local-branch", .target = commit_id});
    REQUIRE(create_result.has_value());

    // List only local branches
    auto local_result = ops::list_branches(temp_repo.repo(), {.include_remote = false});
    REQUIRE(local_result.has_value());

    auto local_branches = local_result->local();
    REQUIRE(local_branches.size() >= 2); // At least main/master and local-branch

    auto remote_branches = local_result->remote();
    REQUIRE(remote_branches.empty()); // No remotes in this test

    // All returned branches should be local
    for (const auto& branch : local_result->branches) {
        REQUIRE_FALSE(branch.is_remote);
    }
}

TEST_CASE("Branch - current branch is marked", "[integration][branch]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("README.md", "# Test Project")
        .with_message("Initial commit")
        .create();

    auto result = ops::list_branches(temp_repo.repo());
    REQUIRE(result.has_value());

    // Exactly one branch should be marked as HEAD
    size_t head_count = 0;
    for (const auto& branch : result->branches) {
        if (branch.is_head) {
            head_count++;
        }
    }
    REQUIRE(head_count == 1);

    // current() should return the HEAD branch
    auto* current = result->current();
    REQUIRE(current != nullptr);
    REQUIRE(current->is_head);
}

TEST_CASE("Branch - rename existing branch succeeds", "[integration][branch]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("README.md", "# Test Project")
        .with_message("Initial commit")
        .create();

    // Get the default branch name (usually master or main)
    auto list_before = ops::list_branches(temp_repo.repo());
    REQUIRE(list_before.has_value());
    auto* current = list_before->current();
    REQUIRE(current != nullptr);
    std::string old_name = current->name;

    // Rename the branch
    auto rename_result = ops::rename_branch(
        temp_repo.repo(), {.old_name = old_name, .new_name = "develop", .force = false});
    REQUIRE(rename_result.has_value());
    REQUIRE(rename_result->branch.name == "develop");

    // Verify the branch list shows the new name
    auto list_after = ops::list_branches(temp_repo.repo());
    REQUIRE(list_after.has_value());

    // New branch should exist
    bool found_new = false;
    bool found_old = false;
    for (const auto& branch : list_after->branches) {
        if (branch.name == "develop") {
            found_new = true;
            REQUIRE(branch.is_head); // Should still be current branch
        }
        if (branch.name == old_name) {
            found_old = true;
        }
    }
    REQUIRE(found_new);
    REQUIRE_FALSE(found_old); // Old branch should be gone
}

TEST_CASE("Branch - rename on empty repository shows helpful error", "[integration][branch]") {
    TempRepo temp_repo;

    // Try to rename branch in empty repository (no commits yet)
    auto rename_result = ops::rename_branch(
        temp_repo.repo(), {.old_name = "master", .new_name = "main", .force = false});

    // Should fail with InvalidArgument
    REQUIRE_FALSE(rename_result.has_value());
    REQUIRE(rename_result.error().code == Error::Code::InvalidArgument);

    // Error message should mention "no commits"
    REQUIRE(rename_result.error().message.find("no commits") != std::string::npos);

    // Error detail should provide guidance
    REQUIRE(rename_result.error().detail.has_value());
    REQUIRE(rename_result.error().detail->find("set-default") != std::string::npos);
}

TEST_CASE("Branch - rename with force overwrites existing branch", "[integration][branch]") {
    TempRepo temp_repo;

    // Create two commits
    auto commit1 = CommitBuilder(temp_repo)
                       .with_file("file1.txt", "First")
                       .with_message("First commit")
                       .create();

    auto commit2 = CommitBuilder(temp_repo)
                       .with_file("file2.txt", "Second")
                       .with_message("Second commit")
                       .create();

    // Create a branch pointing to commit1
    auto create_result = ops::create_branch(temp_repo.repo(),
                                            {.name = "feature", .target = commit1, .force = false});
    REQUIRE(create_result.has_value());

    // Get current branch name
    auto list_result = ops::list_branches(temp_repo.repo());
    REQUIRE(list_result.has_value());
    auto* current = list_result->current();
    REQUIRE(current != nullptr);
    std::string current_name = current->name;

    // Rename current branch to "feature" with force
    auto rename_result = ops::rename_branch(
        temp_repo.repo(), {.old_name = current_name, .new_name = "feature", .force = true});
    REQUIRE(rename_result.has_value());
    REQUIRE(rename_result->branch.name == "feature");
    REQUIRE(rename_result->branch.target == commit2); // Should have current branch's target

    // Verify only one "feature" branch exists
    auto list_after = ops::list_branches(temp_repo.repo());
    REQUIRE(list_after.has_value());

    size_t feature_count = 0;
    for (const auto& branch : list_after->branches) {
        if (branch.name == "feature") {
            feature_count++;
        }
    }
    REQUIRE(feature_count == 1);
}

TEST_CASE("Branch - rename without force fails when target exists", "[integration][branch]") {
    TempRepo temp_repo;

    // Create initial commit
    auto commit_id = CommitBuilder(temp_repo)
                         .with_file("README.md", "# Test Project")
                         .with_message("Initial commit")
                         .create();

    // Create a branch
    auto create_result =
        ops::create_branch(temp_repo.repo(), {.name = "existing", .target = commit_id});
    REQUIRE(create_result.has_value());

    // Get current branch name
    auto list_result = ops::list_branches(temp_repo.repo());
    REQUIRE(list_result.has_value());
    auto* current = list_result->current();
    REQUIRE(current != nullptr);
    std::string current_name = current->name;

    // Try to rename current branch to existing name without force
    auto rename_result = ops::rename_branch(
        temp_repo.repo(), {.old_name = current_name, .new_name = "existing", .force = false});

    // Should fail
    REQUIRE_FALSE(rename_result.has_value());
}

TEST_CASE("Branch - set default branch on empty repository", "[integration][branch]") {
    TempRepo temp_repo;

    // Set default branch to "main" on empty repository
    auto set_result = ops::set_default_branch(temp_repo.repo(), {.branch_name = "main"});
    REQUIRE(set_result.has_value());

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("README.md", "# Test Project")
        .with_message("Initial commit")
        .create();

    // Verify the branch is now "main" not the default
    auto list_result = ops::list_branches(temp_repo.repo());
    REQUIRE(list_result.has_value());
    auto* current = list_result->current();
    REQUIRE(current != nullptr);
    REQUIRE(current->name == "main");
}

TEST_CASE("Branch - set default branch fails when commits exist", "[integration][branch]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("README.md", "# Test Project")
        .with_message("Initial commit")
        .create();

    // Try to set default branch after commits exist
    auto set_result = ops::set_default_branch(temp_repo.repo(), {.branch_name = "develop"});

    // Should fail with InvalidArgument
    REQUIRE_FALSE(set_result.has_value());
    REQUIRE(set_result.error().code == Error::Code::InvalidArgument);

    // Error message should mention "already has commits"
    REQUIRE(set_result.error().message.find("already has commits") != std::string::npos);

    // Error detail should suggest using rename
    REQUIRE(set_result.error().detail.has_value());
    REQUIRE(set_result.error().detail->find("branch rename") != std::string::npos);
}
