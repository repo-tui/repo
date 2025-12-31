#include <repo/ops/rollback.hpp>
#include <repo/ops/stage.hpp>
#include <repo/ops/status.hpp>

#include <catch2/catch_all.hpp>

#include <fstream>

#include "../test_utils.hpp"

using namespace repo;
using namespace repo::test;

TEST_CASE("Rollback - soft mode preserves index and working directory", "[integration][rollback]") {
    TempRepo temp_repo;

    // Create first commit
    auto commit1 = CommitBuilder(temp_repo)
                       .with_file("file.txt", "version 1")
                       .with_message("Commit 1")
                       .create();

    // Create second commit
    CommitBuilder(temp_repo).with_file("file.txt", "version 2").with_message("Commit 2").create();

    // Modify file and stage it
    auto file_path = temp_repo.path() / "file.txt";
    temp_repo.write_file("file.txt", "version 3");
    ops::stage(temp_repo.repo(), {.paths = {"file.txt"}});

    // Soft rollback to commit1
    auto result =
        ops::rollback(temp_repo.repo(), {.target = commit1, .mode = ops::RollbackMode::Soft});

    REQUIRE(result.has_value());

    // Verify index still has staged changes
    auto status = ops::status(temp_repo.repo());
    REQUIRE(status.has_value());
    auto staged = status->staged();
    REQUIRE(staged.size() == 1);

    // Verify working directory still has "version 3"
    std::ifstream file(file_path);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    REQUIRE(content == "version 3");
}

TEST_CASE("Rollback - mixed mode updates index but preserves working directory",
          "[integration][rollback]") {
    TempRepo temp_repo;

    // Create first commit
    auto commit1 = CommitBuilder(temp_repo)
                       .with_file("file.txt", "version 1")
                       .with_message("Commit 1")
                       .create();

    // Create second commit
    CommitBuilder(temp_repo).with_file("file.txt", "version 2").with_message("Commit 2").create();

    // Modify file and stage it
    auto file_path = temp_repo.path() / "file.txt";
    temp_repo.write_file("file.txt", "version 3");
    ops::stage(temp_repo.repo(), {.paths = {"file.txt"}});

    // Mixed rollback to commit1
    auto result =
        ops::rollback(temp_repo.repo(), {.target = commit1, .mode = ops::RollbackMode::Mixed});

    REQUIRE(result.has_value());

    // Verify index is reset (changes are unstaged)
    auto status = ops::status(temp_repo.repo());
    REQUIRE(status.has_value());
    auto unstaged = status->unstaged();
    REQUIRE(unstaged.size() == 1);

    // Verify working directory still has "version 3"
    std::ifstream file(file_path);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    REQUIRE(content == "version 3");
}

TEST_CASE("Rollback - hard mode updates both index and working directory",
          "[integration][rollback]") {
    TempRepo temp_repo;

    // Create first commit
    auto commit1 = CommitBuilder(temp_repo)
                       .with_file("file.txt", "version 1")
                       .with_message("Commit 1")
                       .create();

    // Create second commit
    CommitBuilder(temp_repo).with_file("file.txt", "version 2").with_message("Commit 2").create();

    // Modify file and stage it
    auto file_path = temp_repo.path() / "file.txt";
    temp_repo.write_file("file.txt", "version 3");
    ops::stage(temp_repo.repo(), {.paths = {"file.txt"}});

    // Hard rollback to commit1
    auto result =
        ops::rollback(temp_repo.repo(), {.target = commit1, .mode = ops::RollbackMode::Hard});

    REQUIRE(result.has_value());

    // Verify repository is clean
    auto status = ops::status(temp_repo.repo());
    REQUIRE(status.has_value());
    REQUIRE(status->is_clean());

    // Verify working directory has "version 1"
    std::ifstream file(file_path);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    REQUIRE(content == "version 1");
}

TEST_CASE("Rollback - rollback to parent commit", "[integration][rollback]") {
    TempRepo temp_repo;

    // Create three commits
    CommitBuilder(temp_repo).with_file("file.txt", "version 1").with_message("Commit 1").create();

    auto commit2 = CommitBuilder(temp_repo)
                       .with_file("file.txt", "version 2")
                       .with_message("Commit 2")
                       .create();

    CommitBuilder(temp_repo).with_file("file.txt", "version 3").with_message("Commit 3").create();

    // Hard rollback to commit2
    auto result =
        ops::rollback(temp_repo.repo(), {.target = commit2, .mode = ops::RollbackMode::Hard});

    REQUIRE(result.has_value());

    // Verify working directory has "version 2"
    auto file_path = temp_repo.path() / "file.txt";
    std::ifstream file(file_path);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    REQUIRE(content == "version 2");
}

TEST_CASE("Rollback - soft mode with multiple files", "[integration][rollback]") {
    TempRepo temp_repo;

    // Create first commit with multiple files
    auto commit1 = CommitBuilder(temp_repo)
                       .with_file("file1.txt", "content 1")
                       .with_file("file2.txt", "content 2")
                       .with_message("Commit 1")
                       .create();

    // Create second commit modifying files
    CommitBuilder(temp_repo)
        .with_file("file1.txt", "modified 1")
        .with_file("file2.txt", "modified 2")
        .with_message("Commit 2")
        .create();

    // Soft rollback to commit1
    auto result =
        ops::rollback(temp_repo.repo(), {.target = commit1, .mode = ops::RollbackMode::Soft});

    REQUIRE(result.has_value());

    // Verify both files are staged
    auto status = ops::status(temp_repo.repo());
    REQUIRE(status.has_value());
    auto staged = status->staged();
    REQUIRE(staged.size() == 2);
}

TEST_CASE("Rollback - mixed mode with new file", "[integration][rollback]") {
    TempRepo temp_repo;

    // Create first commit
    auto commit1 = CommitBuilder(temp_repo)
                       .with_file("file1.txt", "content 1")
                       .with_message("Commit 1")
                       .create();

    // Create second commit with new file
    CommitBuilder(temp_repo).with_file("file2.txt", "content 2").with_message("Commit 2").create();

    // Mixed rollback to commit1
    auto result =
        ops::rollback(temp_repo.repo(), {.target = commit1, .mode = ops::RollbackMode::Mixed});

    REQUIRE(result.has_value());

    // Verify file2.txt is now untracked
    auto status = ops::status(temp_repo.repo());
    REQUIRE(status.has_value());
    auto untracked = status->untracked();
    REQUIRE(untracked.size() == 1);
    REQUIRE(untracked[0].is_untracked());
}

TEST_CASE("Rollback - hard mode with deleted file", "[integration][rollback]") {
    TempRepo temp_repo;

    // Create first commit with file
    auto commit1 =
        CommitBuilder(temp_repo).with_file("file.txt", "content").with_message("Commit 1").create();

    // Delete file and commit
    std::filesystem::remove(temp_repo.path() / "file.txt");
    ops::stage(temp_repo.repo(), {.paths = {"file.txt"}});
    CommitBuilder(temp_repo).with_message("Commit 2").create();

    // Hard rollback to commit1
    auto result =
        ops::rollback(temp_repo.repo(), {.target = commit1, .mode = ops::RollbackMode::Hard});

    REQUIRE(result.has_value());

    // Verify file is restored
    auto file_path = temp_repo.path() / "file.txt";
    REQUIRE(std::filesystem::exists(file_path));

    std::ifstream file(file_path);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    REQUIRE(content == "content");
}

TEST_CASE("Rollback - soft mode multiple commits back", "[integration][rollback]") {
    TempRepo temp_repo;

    // Create commit chain
    auto commit1 =
        CommitBuilder(temp_repo).with_file("file.txt", "v1").with_message("Commit 1").create();

    CommitBuilder(temp_repo).with_file("file.txt", "v2").with_message("Commit 2").create();

    CommitBuilder(temp_repo).with_file("file.txt", "v3").with_message("Commit 3").create();

    CommitBuilder(temp_repo).with_file("file.txt", "v4").with_message("Commit 4").create();

    // Soft rollback back to commit1 (3 commits back)
    auto result =
        ops::rollback(temp_repo.repo(), {.target = commit1, .mode = ops::RollbackMode::Soft});

    REQUIRE(result.has_value());

    // Verify file content is still "v4"
    auto file_path = temp_repo.path() / "file.txt";
    std::ifstream file(file_path);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    REQUIRE(content == "v4");

    // Verify changes are staged
    auto status = ops::status(temp_repo.repo());
    REQUIRE(status.has_value());
    auto staged = status->staged();
    REQUIRE(staged.size() == 1);
}

TEST_CASE("Rollback - invalid commit OID fails", "[integration][rollback]") {
    TempRepo temp_repo;

    // Create commit
    CommitBuilder(temp_repo).with_file("file.txt", "content").with_message("Commit 1").create();

    // Try to rollback to invalid OID
    auto invalid_oid =
        domain::ObjectId::from_string("0000000000000000000000000000000000000001").value();

    auto result =
        ops::rollback(temp_repo.repo(), {.target = invalid_oid, .mode = ops::RollbackMode::Hard});

    REQUIRE_FALSE(result.has_value());
}
