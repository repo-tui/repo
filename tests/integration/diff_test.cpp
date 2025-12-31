#include <repo/ops/diff.hpp>
#include <repo/ops/stage.hpp>

#include <catch2/catch_all.hpp>

#include "../test_utils.hpp"

using namespace repo;
using namespace repo::test;

TEST_CASE("Diff - unstaged changes (modified file)", "[integration][diff]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "original content\n")
        .with_message("Initial commit")
        .create();

    // Modify file
    temp_repo.write_file("file.txt", "modified content\n");

    // Get unstaged diff
    auto result = ops::diff(temp_repo.repo(), {.mode = ops::DiffParams::Mode::Unstaged});

    REQUIRE(result.has_value());
    REQUIRE(result->files_changed() == 1);
    REQUIRE(result->diffs.size() == 1);
    REQUIRE(result->diffs[0].path == "file.txt");
    REQUIRE(result->diffs[0].status == domain::FileDiff::Status::Modified);
    REQUIRE_FALSE(result->diffs[0].is_binary);
}

TEST_CASE("Diff - staged changes (modified file)", "[integration][diff]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "original\n")
        .with_message("Initial commit")
        .create();

    // Modify and stage
    temp_repo.write_file("file.txt", "modified\n");
    auto stage_result = ops::stage(temp_repo.repo(), {.paths = {"file.txt"}});
    REQUIRE(stage_result.has_value());

    // Get staged diff
    auto result = ops::diff(temp_repo.repo(), {.mode = ops::DiffParams::Mode::Staged});

    REQUIRE(result.has_value());
    REQUIRE(result->files_changed() == 1);
    REQUIRE(result->diffs.size() == 1);
    REQUIRE(result->diffs[0].path == "file.txt");
    REQUIRE(result->diffs[0].status == domain::FileDiff::Status::Modified);
}

TEST_CASE("Diff - new file unstaged", "[integration][diff]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("existing.txt", "content")
        .with_message("Initial commit")
        .create();

    // Add new file (not staged)
    temp_repo.write_file("new.txt", "new content\n");

    // Get unstaged diff
    auto result = ops::diff(temp_repo.repo(), {.mode = ops::DiffParams::Mode::Unstaged});

    REQUIRE(result.has_value());
    // Note: Untracked files may or may not appear in diff depending on libgit2 behavior
    // This test validates that the operation succeeds
}

TEST_CASE("Diff - new file staged", "[integration][diff]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("existing.txt", "content")
        .with_message("Initial commit")
        .create();

    // Add and stage new file
    temp_repo.write_file("new.txt", "new content\n");
    auto stage_result = ops::stage(temp_repo.repo(), {.paths = {"new.txt"}});
    REQUIRE(stage_result.has_value());

    // Get staged diff
    auto result = ops::diff(temp_repo.repo(), {.mode = ops::DiffParams::Mode::Staged});

    REQUIRE(result.has_value());
    REQUIRE(result->files_changed() >= 1);

    // Find the new file in diffs
    bool found_new_file = false;
    for (const auto& file_diff : result->diffs) {
        if (file_diff.path == "new.txt") {
            found_new_file = true;
            REQUIRE(file_diff.status == domain::FileDiff::Status::Added);
            REQUIRE(file_diff.additions > 0);
            REQUIRE(file_diff.deletions == 0);
        }
    }
    REQUIRE(found_new_file);
}

TEST_CASE("Diff - deleted file unstaged", "[integration][diff]") {
    TempRepo temp_repo;

    // Create initial commit with file
    CommitBuilder(temp_repo)
        .with_file("to_delete.txt", "content to delete\n")
        .with_message("Initial commit")
        .create();

    // Delete the file
    temp_repo.delete_file("to_delete.txt");

    // Get unstaged diff
    auto result = ops::diff(temp_repo.repo(), {.mode = ops::DiffParams::Mode::Unstaged});

    REQUIRE(result.has_value());
    REQUIRE(result->files_changed() == 1);
    REQUIRE(result->diffs[0].path == "to_delete.txt");
    REQUIRE(result->diffs[0].status == domain::FileDiff::Status::Deleted);
    REQUIRE(result->diffs[0].deletions > 0);
    REQUIRE(result->diffs[0].additions == 0);
}

TEST_CASE("Diff - deleted file staged", "[integration][diff]") {
    TempRepo temp_repo;

    // Create initial commit with file
    CommitBuilder(temp_repo)
        .with_file("to_delete.txt", "content to delete\n")
        .with_message("Initial commit")
        .create();

    // Delete and stage
    temp_repo.delete_file("to_delete.txt");
    auto stage_result = ops::stage(temp_repo.repo(), {.paths = {"to_delete.txt"}});
    REQUIRE(stage_result.has_value());

    // Get staged diff
    auto result = ops::diff(temp_repo.repo(), {.mode = ops::DiffParams::Mode::Staged});

    REQUIRE(result.has_value());
    REQUIRE(result->files_changed() == 1);
    REQUIRE(result->diffs[0].path == "to_delete.txt");
    REQUIRE(result->diffs[0].status == domain::FileDiff::Status::Deleted);
}

TEST_CASE("Diff - all changes (staged + unstaged)", "[integration][diff]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file1.txt", "content1\n")
        .with_file("file2.txt", "content2\n")
        .with_message("Initial commit")
        .create();

    // Modify file1 and stage
    temp_repo.write_file("file1.txt", "modified1\n");
    auto stage_result = ops::stage(temp_repo.repo(), {.paths = {"file1.txt"}});
    REQUIRE(stage_result.has_value());

    // Modify file2 (not staged)
    temp_repo.write_file("file2.txt", "modified2\n");

    // Get all diffs
    auto result = ops::diff(temp_repo.repo(), {.mode = ops::DiffParams::Mode::All});

    REQUIRE(result.has_value());
    REQUIRE(result->files_changed() == 2);
    REQUIRE(result->total_additions() > 0);
    REQUIRE(result->total_deletions() > 0);
}

TEST_CASE("Diff - clean repository (no changes)", "[integration][diff]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "content")
        .with_message("Initial commit")
        .create();

    // No changes made

    // Get unstaged diff
    auto result = ops::diff(temp_repo.repo(), {.mode = ops::DiffParams::Mode::Unstaged});

    REQUIRE(result.has_value());
    REQUIRE(result->files_changed() == 0);
    REQUIRE(result->diffs.empty());
    REQUIRE(result->total_additions() == 0);
    REQUIRE(result->total_deletions() == 0);
}

TEST_CASE("Diff - multiple hunks in single file", "[integration][diff]") {
    TempRepo temp_repo;

    // Create initial commit with multi-line file
    CommitBuilder(temp_repo)
        .with_file("multi.txt",
                   "line1\nline2\nline3\nline4\nline5\nline6\nline7\nline8\nline9\nline10\n")
        .with_message("Initial commit")
        .create();

    // Modify multiple non-adjacent lines
    temp_repo.write_file(
        "multi.txt",
        "modified1\nline2\nline3\nline4\nmodified5\nline6\nline7\nline8\nline9\nmodified10\n");

    // Get unstaged diff
    auto result = ops::diff(temp_repo.repo(), {.mode = ops::DiffParams::Mode::Unstaged});

    REQUIRE(result.has_value());
    REQUIRE(result->files_changed() == 1);
    REQUIRE(result->diffs[0].path == "multi.txt");
    REQUIRE(result->diffs[0].status == domain::FileDiff::Status::Modified);

    // Should have hunks
    REQUIRE_FALSE(result->diffs[0].hunks.empty());
}

TEST_CASE("Diff - total_additions and total_deletions", "[integration][diff]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file1.txt", "line1\nline2\n")
        .with_file("file2.txt", "line1\n")
        .with_message("Initial commit")
        .create();

    // Modify both files
    temp_repo.write_file("file1.txt", "modified1\nmodified2\nmodified3\n"); // +3, -2
    temp_repo.write_file("file2.txt", "");                                  // +0, -1

    auto stage_result = ops::stage(temp_repo.repo(), {.paths = {"file1.txt", "file2.txt"}});
    REQUIRE(stage_result.has_value());

    // Get staged diff
    auto result = ops::diff(temp_repo.repo(), {.mode = ops::DiffParams::Mode::Staged});

    REQUIRE(result.has_value());
    REQUIRE(result->files_changed() == 2);

    // Verify totals are computed correctly
    size_t total_adds = result->total_additions();
    size_t total_dels = result->total_deletions();

    REQUIRE(total_adds > 0);
    REQUIRE(total_dels > 0);

    // Verify totals match sum of individual file diffs
    size_t sum_adds = 0, sum_dels = 0;
    for (const auto& diff : result->diffs) {
        sum_adds += diff.additions;
        sum_dels += diff.deletions;
    }
    REQUIRE(total_adds == sum_adds);
    REQUIRE(total_dels == sum_dels);
}
