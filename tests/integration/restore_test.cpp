#include <repo/ops/restore.hpp>
#include <repo/ops/stage.hpp>
#include <repo/ops/status.hpp>

#include <catch2/catch_all.hpp>

#include <fstream>

#include "../test_utils.hpp"

using namespace repo;
using namespace repo::test;

TEST_CASE("Restore - discard unstaged changes", "[integration][restore]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "original content\n")
        .with_message("Initial commit")
        .create();

    // Modify file
    temp_repo.write_file("file.txt", "modified content\n");

    // Verify file is modified
    auto status_before = ops::status(temp_repo.repo());
    REQUIRE(status_before.has_value());
    REQUIRE_FALSE(status_before->is_clean());

    // Restore the file
    auto restore_result = ops::restore(temp_repo.repo(), {.paths = {"file.txt"}});
    REQUIRE(restore_result.has_value());
    REQUIRE(restore_result->restored.size() == 1);
    REQUIRE(restore_result->restored[0] == "file.txt");

    // Verify file is restored
    auto workdir = temp_repo.repo().backend().workdir(temp_repo.repo().repo_handle());
    auto file_path = workdir / "file.txt";
    std::ifstream file(file_path);
    REQUIRE(file.is_open());
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    REQUIRE(content == "original content\n");

    // Verify repository is clean
    auto status_after = ops::status(temp_repo.repo());
    REQUIRE(status_after.has_value());
    REQUIRE(status_after->is_clean());
}

TEST_CASE("Restore - discard staged changes", "[integration][restore]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "original content\n")
        .with_message("Initial commit")
        .create();

    // Modify and stage file
    temp_repo.write_file("file.txt", "modified content\n");
    auto stage_result = ops::stage(temp_repo.repo(), {.paths = {"file.txt"}});
    REQUIRE(stage_result.has_value());

    // Verify file is staged
    auto status_before = ops::status(temp_repo.repo());
    REQUIRE(status_before.has_value());
    auto staged = status_before->staged();
    REQUIRE(staged.size() == 1);

    // Restore staged changes
    auto restore_result = ops::restore(temp_repo.repo(), {.paths = {"file.txt"}, .staged = true});
    REQUIRE(restore_result.has_value());

    // Verify file is unstaged and content restored
    auto workdir = temp_repo.repo().backend().workdir(temp_repo.repo().repo_handle());
    auto file_path = workdir / "file.txt";
    std::ifstream file(file_path);
    REQUIRE(file.is_open());
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    REQUIRE(content == "original content\n");

    auto status_after = ops::status(temp_repo.repo());
    REQUIRE(status_after.has_value());
    REQUIRE(status_after->is_clean());
}

TEST_CASE("Restore - multiple files", "[integration][restore]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file1.txt", "content1\n")
        .with_file("file2.txt", "content2\n")
        .with_file("file3.txt", "content3\n")
        .with_message("Initial commit")
        .create();

    // Modify all files
    temp_repo.write_file("file1.txt", "modified1\n");
    temp_repo.write_file("file2.txt", "modified2\n");
    temp_repo.write_file("file3.txt", "modified3\n");

    // Restore multiple files
    auto restore_result =
        ops::restore(temp_repo.repo(), {.paths = {"file1.txt", "file2.txt", "file3.txt"}});
    REQUIRE(restore_result.has_value());
    REQUIRE(restore_result->restored.size() == 3);

    // Verify all files are restored
    auto workdir = temp_repo.repo().backend().workdir(temp_repo.repo().repo_handle());

    std::ifstream file1(workdir / "file1.txt");
    std::string content1((std::istreambuf_iterator<char>(file1)), std::istreambuf_iterator<char>());
    REQUIRE(content1 == "content1\n");

    std::ifstream file2(workdir / "file2.txt");
    std::string content2((std::istreambuf_iterator<char>(file2)), std::istreambuf_iterator<char>());
    REQUIRE(content2 == "content2\n");

    std::ifstream file3(workdir / "file3.txt");
    std::string content3((std::istreambuf_iterator<char>(file3)), std::istreambuf_iterator<char>());
    REQUIRE(content3 == "content3\n");
}

TEST_CASE("Restore - partial restore", "[integration][restore]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("keep.txt", "keep content\n")
        .with_file("restore.txt", "restore content\n")
        .with_message("Initial commit")
        .create();

    // Modify both files
    temp_repo.write_file("keep.txt", "modified keep\n");
    temp_repo.write_file("restore.txt", "modified restore\n");

    // Restore only one file
    auto restore_result = ops::restore(temp_repo.repo(), {.paths = {"restore.txt"}});
    REQUIRE(restore_result.has_value());

    auto workdir = temp_repo.repo().backend().workdir(temp_repo.repo().repo_handle());

    // Verify restored file
    std::ifstream restore_file(workdir / "restore.txt");
    std::string restore_content((std::istreambuf_iterator<char>(restore_file)),
                                std::istreambuf_iterator<char>());
    REQUIRE(restore_content == "restore content\n");

    // Verify other file is still modified
    std::ifstream keep_file(workdir / "keep.txt");
    std::string keep_content((std::istreambuf_iterator<char>(keep_file)),
                             std::istreambuf_iterator<char>());
    REQUIRE(keep_content == "modified keep\n");
}

TEST_CASE("Restore - deleted file", "[integration][restore]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "content\n")
        .with_message("Initial commit")
        .create();

    // Delete file
    temp_repo.delete_file("file.txt");

    // Verify file is deleted
    auto status_before = ops::status(temp_repo.repo());
    REQUIRE(status_before.has_value());
    auto unstaged = status_before->unstaged();
    REQUIRE(unstaged.size() == 1);
    REQUIRE(unstaged[0].worktree_status == domain::FileStatus::State::Deleted);

    // Restore deleted file
    auto restore_result = ops::restore(temp_repo.repo(), {.paths = {"file.txt"}});
    REQUIRE(restore_result.has_value());

    // Verify file is restored
    auto workdir = temp_repo.repo().backend().workdir(temp_repo.repo().repo_handle());
    auto file_path = workdir / "file.txt";
    REQUIRE(std::filesystem::exists(file_path));

    std::ifstream file(file_path);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    REQUIRE(content == "content\n");
}

TEST_CASE("Restore - empty paths fails", "[integration][restore]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "content")
        .with_message("Initial commit")
        .create();

    // Try to restore with empty paths
    auto restore_result = ops::restore(temp_repo.repo(), {.paths = {}});

    REQUIRE_FALSE(restore_result.has_value());
    REQUIRE(restore_result.error().code == Error::Code::InvalidArgument);
}

TEST_CASE("Restore - combined staged and unstaged", "[integration][restore]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "original\n")
        .with_message("Initial commit")
        .create();

    // Modify file in two stages
    temp_repo.write_file("file.txt", "staged version\n");
    ops::stage(temp_repo.repo(), {.paths = {"file.txt"}});

    temp_repo.write_file("file.txt", "unstaged version\n");

    // Status should show both staged and unstaged changes
    auto status = ops::status(temp_repo.repo());
    REQUIRE(status.has_value());
    REQUIRE_FALSE(status->is_clean());

    // First restore staged (this will restore to original and update index)
    ops::restore(temp_repo.repo(), {.paths = {"file.txt"}, .staged = true});

    // Verify working tree still has unstaged changes relative to index
    // (After restoring staged, index is back to original, but working tree may vary)
    auto status_after_staged = ops::status(temp_repo.repo());
    REQUIRE(status_after_staged.has_value());

    // Now restore working tree
    ops::restore(temp_repo.repo(), {.paths = {"file.txt"}, .staged = false});

    // Verify everything is back to original
    auto workdir = temp_repo.repo().backend().workdir(temp_repo.repo().repo_handle());
    std::ifstream file(workdir / "file.txt");
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    REQUIRE(content == "original\n");

    auto final_status = ops::status(temp_repo.repo());
    REQUIRE(final_status.has_value());
    REQUIRE(final_status->is_clean());
}
