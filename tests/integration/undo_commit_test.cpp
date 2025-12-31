#include <repo/ops/list_commits.hpp>
#include <repo/ops/stage.hpp>
#include <repo/ops/status.hpp>
#include <repo/ops/undo_commit.hpp>

#include <catch2/catch_all.hpp>

#include <fstream>

#include "../test_utils.hpp"

using namespace repo;
using namespace repo::test;

TEST_CASE("Undo commit - undo single commit with file addition", "[integration][undo-commit]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("base.txt", "base content")
        .with_message("Initial commit")
        .create();

    // Create a commit that adds a file
    auto bad_commit = CommitBuilder(temp_repo)
                          .with_file("mistake.txt", "this was a mistake")
                          .with_message("Add mistake file")
                          .create();

    // Undo the commit
    auto result = ops::undo_commit(temp_repo.repo(), {.commit = bad_commit, .no_commit = false});

    REQUIRE(result.has_value());

    // Verify mistake.txt no longer exists
    REQUIRE_FALSE(std::filesystem::exists(temp_repo.path() / "mistake.txt"));

    // Verify base.txt still exists
    REQUIRE(std::filesystem::exists(temp_repo.path() / "base.txt"));

    // Verify a new undo commit was created
    auto commits = ops::list_commits(temp_repo.repo());
    REQUIRE(commits.has_value());
    REQUIRE(commits->commits.size() == 3); // initial + mistake + undo
    REQUIRE(commits->commits[0].message.find("Revert") != std::string::npos);
}

TEST_CASE("Undo commit - undo commit with file modification", "[integration][undo-commit]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "original content")
        .with_message("Initial commit")
        .create();

    // Create a commit that modifies the file
    auto modify_commit = CommitBuilder(temp_repo)
                             .with_file("file.txt", "modified content")
                             .with_message("Modify file")
                             .create();

    // Undo the modification
    auto result = ops::undo_commit(temp_repo.repo(), {.commit = modify_commit, .no_commit = false});

    REQUIRE(result.has_value());

    // Verify file content is back to original
    auto file_path = temp_repo.path() / "file.txt";
    std::ifstream file(file_path);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    REQUIRE(content == "original content");
}

TEST_CASE("Undo commit - with no_commit flag", "[integration][undo-commit]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo).with_file("base.txt", "base").with_message("Initial commit").create();

    // Create a commit with a new file
    auto feature_commit = CommitBuilder(temp_repo)
                              .with_file("feature.txt", "feature")
                              .with_message("Add feature")
                              .create();

    // Undo with no_commit
    auto result = ops::undo_commit(temp_repo.repo(), {.commit = feature_commit, .no_commit = true});

    REQUIRE(result.has_value());

    // Verify changes are staged but not committed
    auto status = ops::status(temp_repo.repo());
    REQUIRE(status.has_value());
    auto staged = status->staged();
    REQUIRE(staged.size() == 1);

    // Verify no new commit was created
    auto commits = ops::list_commits(temp_repo.repo());
    REQUIRE(commits.has_value());
    REQUIRE(commits->commits.size() == 2); // Only initial + feature
}

TEST_CASE("Undo commit - undo commit with file deletion", "[integration][undo-commit]") {
    TempRepo temp_repo;

    // Create commit with file
    CommitBuilder(temp_repo)
        .with_file("important.txt", "important data")
        .with_message("Initial commit")
        .create();

    // Delete the file in a new commit
    std::filesystem::remove(temp_repo.path() / "important.txt");
    ops::stage(temp_repo.repo(), {.paths = {"important.txt"}});
    auto delete_commit = CommitBuilder(temp_repo).with_message("Delete important file").create();

    // Undo the deletion
    auto result = ops::undo_commit(temp_repo.repo(), {.commit = delete_commit, .no_commit = false});

    REQUIRE(result.has_value());

    // Verify file is restored
    auto file_path = temp_repo.path() / "important.txt";
    REQUIRE(std::filesystem::exists(file_path));

    std::ifstream file(file_path);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    REQUIRE(content == "important data");
}

TEST_CASE("Undo commit - invalid commit OID fails", "[integration][undo-commit]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "content")
        .with_message("Initial commit")
        .create();

    // Try to undo invalid OID
    auto invalid_oid =
        domain::ObjectId::from_string("0000000000000000000000000000000000000001").value();

    auto result = ops::undo_commit(temp_repo.repo(), {.commit = invalid_oid, .no_commit = false});

    REQUIRE_FALSE(result.has_value());
}

// TODO: Investigate - reverting middle commits can cause conflicts when subsequent commits modified
// the same lines This is expected Git behavior and requires manual conflict resolution
/*
TEST_CASE("Undo commit - undo middle commit in history", "[integration][undo-commit]") {
    TempRepo temp_repo;

    // Create first commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "line 1\n")
        .with_message("First commit")
        .create();

    // Create second commit (this is what we'll undo)
    auto middle_commit = CommitBuilder(temp_repo)
        .with_file("file.txt", "line 1\nline 2\n")
        .with_message("Add line 2")
        .create();

    // Create third commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "line 1\nline 2\nline 3\n")
        .with_message("Add line 3")
        .create();

    // Undo the middle commit
    auto result = ops::undo_commit(temp_repo.repo(), {
        .commit = middle_commit,
        .no_commit = false
    });

    REQUIRE(result.has_value());

    // Verify file content has line 2 removed but line 3 remains
    auto file_path = temp_repo.path() / "file.txt";
    std::ifstream file(file_path);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    REQUIRE(content == "line 1\nline 3\n");

    // Verify commit count
    auto commits = ops::list_commits(temp_repo.repo());
    REQUIRE(commits.has_value());
    REQUIRE(commits->commits.size() == 4);  // 3 original + 1 undo
}
*/

TEST_CASE("Undo commit - multiple file changes", "[integration][undo-commit]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file1.txt", "content 1")
        .with_file("file2.txt", "content 2")
        .with_message("Initial commit")
        .create();

    // Create commit that modifies both files
    auto multi_commit = CommitBuilder(temp_repo)
                            .with_file("file1.txt", "modified 1")
                            .with_file("file2.txt", "modified 2")
                            .with_message("Modify both files")
                            .create();

    // Undo the modifications
    auto result = ops::undo_commit(temp_repo.repo(), {.commit = multi_commit, .no_commit = false});

    REQUIRE(result.has_value());

    // Verify both files are back to original content
    std::ifstream file1(temp_repo.path() / "file1.txt");
    std::string content1((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
    REQUIRE(content1 == "content 1");

    std::ifstream file2(temp_repo.path() / "file2.txt");
    std::string content2((std::istreambuf_iterator<char>(file2)), std::istreambuf_iterator<char>());
    REQUIRE(content2 == "content 2");
}
