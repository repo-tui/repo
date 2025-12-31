#include <repo/ops/branch.hpp>
#include <repo/ops/list_commits.hpp>
#include <repo/ops/switch.hpp>

#include <catch2/catch_all.hpp>

#include <fstream>

#include "../test_utils.hpp"

using namespace repo;
using namespace repo::test;

TEST_CASE("Switch - basic branch switch", "[integration][switch]") {
    TempRepo temp_repo;

    // Create initial commit on main
    CommitBuilder(temp_repo)
        .with_file("README.md", "# Main Branch")
        .with_message("Initial commit")
        .create();

    // Get current branch (should be main or master)
    auto branches_before = ops::list_branches(temp_repo.repo(), {.include_remote = false});
    REQUIRE(branches_before.has_value());
    REQUIRE(branches_before->branches.size() == 1);
    std::string initial_branch = branches_before->branches[0].name;

    // Create a new branch
    auto create_result = ops::create_branch(
        temp_repo.repo(), {.name = "feature", .target = branches_before->branches[0].target});
    REQUIRE(create_result.has_value());

    // Switch to the new branch
    auto switch_result = ops::switch_branch(temp_repo.repo(), {.branch_name = "feature"});
    REQUIRE(switch_result.has_value());
    REQUIRE(switch_result->previous_branch == initial_branch);
    REQUIRE(switch_result->new_branch == "feature");

    // Verify HEAD points to feature branch
    auto head_result = temp_repo.repo().backend().get_head(temp_repo.repo().repo_handle());
    REQUIRE(head_result.has_value());
    // HEAD should point to feature branch (either symbolic or direct)
    if (head_result->is_symbolic() && std::holds_alternative<std::string>(head_result->target)) {
        auto target = std::get<std::string>(head_result->target);
        REQUIRE(target.ends_with("feature"));
    } else {
        // HEAD name should indicate the branch
        REQUIRE(head_result->name.ends_with("feature"));
    }
}

TEST_CASE("Switch - switch between multiple branches", "[integration][switch]") {
    TempRepo temp_repo;

    // Create initial commit
    auto commit1 = CommitBuilder(temp_repo)
                       .with_file("file.txt", "version 1")
                       .with_message("First commit")
                       .create();

    // Get initial branch
    auto branches = ops::list_branches(temp_repo.repo(), {.include_remote = false});
    REQUIRE(branches.has_value());
    std::string main_branch = branches->branches[0].name;

    // Create branch-a and add a commit
    ops::create_branch(temp_repo.repo(), {.name = "branch-a", .target = commit1});
    ops::switch_branch(temp_repo.repo(), {.branch_name = "branch-a"});

    CommitBuilder(temp_repo)
        .with_file("a.txt", "branch a content")
        .with_message("Branch A commit")
        .create();

    // Switch back to main
    auto switch_to_main = ops::switch_branch(temp_repo.repo(), {.branch_name = main_branch});
    REQUIRE(switch_to_main.has_value());
    REQUIRE(switch_to_main->previous_branch == "branch-a");

    // Create branch-b and add a commit
    ops::create_branch(temp_repo.repo(), {.name = "branch-b", .target = commit1});
    ops::switch_branch(temp_repo.repo(), {.branch_name = "branch-b"});

    CommitBuilder(temp_repo)
        .with_file("b.txt", "branch b content")
        .with_message("Branch B commit")
        .create();

    // Switch back to branch-a
    auto switch_to_a = ops::switch_branch(temp_repo.repo(), {.branch_name = "branch-a"});
    REQUIRE(switch_to_a.has_value());
    REQUIRE(switch_to_a->previous_branch == "branch-b");
    REQUIRE(switch_to_a->new_branch == "branch-a");

    // Verify we're on branch-a
    auto head = temp_repo.repo().backend().get_head(temp_repo.repo().repo_handle());
    REQUIRE(head.has_value());
    REQUIRE(head->name.ends_with("branch-a"));
}

TEST_CASE("Switch - working directory is updated", "[integration][switch]") {
    TempRepo temp_repo;

    // Create initial commit on main
    auto commit1 = CommitBuilder(temp_repo)
                       .with_file("shared.txt", "main content")
                       .with_message("Initial commit")
                       .create();

    auto branches = ops::list_branches(temp_repo.repo(), {.include_remote = false});
    std::string main_branch = branches->branches[0].name;

    // Create feature branch and modify the file
    ops::create_branch(temp_repo.repo(), {.name = "feature", .target = commit1});
    ops::switch_branch(temp_repo.repo(), {.branch_name = "feature"});

    temp_repo.write_file("shared.txt", "feature content");
    CommitBuilder(temp_repo)
        .with_file("shared.txt", "feature content")
        .with_message("Update on feature")
        .create();

    // Switch back to main
    auto switch_result = ops::switch_branch(temp_repo.repo(), {.branch_name = main_branch});
    REQUIRE(switch_result.has_value());

    // Verify file content is restored to main version
    auto workdir = temp_repo.repo().backend().workdir(temp_repo.repo().repo_handle());
    auto file_path = workdir / "shared.txt";

    std::ifstream file(file_path);
    REQUIRE(file.is_open());
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    REQUIRE(content == "main content");
}

TEST_CASE("Switch - non-existent branch fails", "[integration][switch]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "content")
        .with_message("Initial commit")
        .create();

    // Try to switch to non-existent branch
    auto switch_result = ops::switch_branch(temp_repo.repo(), {.branch_name = "nonexistent"});

    REQUIRE_FALSE(switch_result.has_value());
    REQUIRE(switch_result.error().code != Error::Code::Unknown);
}

TEST_CASE("Switch - verify current branch tracking", "[integration][switch]") {
    TempRepo temp_repo;

    // Create initial commit
    auto commit = CommitBuilder(temp_repo)
                      .with_file("file.txt", "content")
                      .with_message("Initial commit")
                      .create();

    // Get initial branch
    auto branches = ops::list_branches(temp_repo.repo(), {.include_remote = false});
    REQUIRE(branches.has_value());
    std::string main_branch = branches->branches[0].name;

    // Create and switch to new branch
    ops::create_branch(temp_repo.repo(), {.name = "test", .target = commit});
    ops::switch_branch(temp_repo.repo(), {.branch_name = "test"});

    // List branches and verify current branch is marked
    auto branches_after = ops::list_branches(temp_repo.repo(), {.include_remote = false});
    REQUIRE(branches_after.has_value());

    bool found_current = false;
    for (const auto& branch : branches_after->branches) {
        if (branch.name == "test") {
            REQUIRE(branch.is_head);
            found_current = true;
        } else if (branch.name == main_branch) {
            REQUIRE_FALSE(branch.is_head);
        }
    }
    REQUIRE(found_current);
}

TEST_CASE("Switch - back and forth preserves state", "[integration][switch]") {
    TempRepo temp_repo;

    // Create initial commit
    auto commit1 = CommitBuilder(temp_repo)
                       .with_file("base.txt", "base")
                       .with_message("Initial commit")
                       .create();

    auto branches = ops::list_branches(temp_repo.repo(), {.include_remote = false});
    std::string main_branch = branches->branches[0].name;

    // Create feature branch with additional commits
    ops::create_branch(temp_repo.repo(), {.name = "feature", .target = commit1});
    ops::switch_branch(temp_repo.repo(), {.branch_name = "feature"});

    CommitBuilder(temp_repo)
        .with_file("feature.txt", "feature work")
        .with_message("Feature commit 1")
        .create();

    CommitBuilder(temp_repo)
        .with_file("feature2.txt", "more feature work")
        .with_message("Feature commit 2")
        .create();

    // Get log on feature branch
    auto feature_log = ops::list_commits(temp_repo.repo(), {});
    REQUIRE(feature_log.has_value());
    size_t feature_commit_count = feature_log->commits.size();
    REQUIRE(feature_commit_count == 3); // Initial + 2 feature commits

    // Switch to main
    ops::switch_branch(temp_repo.repo(), {.branch_name = main_branch});

    // Verify main only has initial commit
    auto main_log = ops::list_commits(temp_repo.repo(), {});
    REQUIRE(main_log.has_value());
    REQUIRE(main_log->commits.size() == 1);

    // Switch back to feature
    ops::switch_branch(temp_repo.repo(), {.branch_name = "feature"});

    // Verify feature still has all commits
    auto feature_log2 = ops::list_commits(temp_repo.repo(), {});
    REQUIRE(feature_log2.has_value());
    REQUIRE(feature_log2->commits.size() == feature_commit_count);
}
