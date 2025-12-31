#include <repo/ops/remote.hpp>

#include <catch2/catch_all.hpp>

#include "../test_utils.hpp"

using namespace repo;
using namespace repo::test;

TEST_CASE("Remote - list remotes in new repo (empty)", "[integration][remote]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("README.md", "# Test")
        .with_message("Initial commit")
        .create();

    // List remotes (should be empty)
    auto result = ops::list_remotes(temp_repo.repo());

    REQUIRE(result.has_value());
    REQUIRE(result->remotes.empty());
}

TEST_CASE("Remote - add remote", "[integration][remote]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "content")
        .with_message("Initial commit")
        .create();

    // Add a remote
    auto add_result = ops::add_remote(
        temp_repo.repo(), {.name = "origin", .url = "https://github.com/example/repo.git"});

    REQUIRE(add_result.has_value());

    // Verify remote was added
    auto list_result = ops::list_remotes(temp_repo.repo());
    REQUIRE(list_result.has_value());
    REQUIRE(list_result->remotes.size() == 1);
    REQUIRE(list_result->remotes[0].name == "origin");
    REQUIRE(list_result->remotes[0].url == "https://github.com/example/repo.git");
}

TEST_CASE("Remote - add multiple remotes", "[integration][remote]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "content")
        .with_message("Initial commit")
        .create();

    // Add multiple remotes
    ops::add_remote(temp_repo.repo(),
                    {.name = "origin", .url = "https://github.com/example/repo.git"});

    ops::add_remote(temp_repo.repo(),
                    {.name = "upstream", .url = "https://github.com/upstream/repo.git"});

    ops::add_remote(temp_repo.repo(), {.name = "fork", .url = "https://github.com/fork/repo.git"});

    // Verify all remotes
    auto result = ops::list_remotes(temp_repo.repo());
    REQUIRE(result.has_value());
    REQUIRE(result->remotes.size() == 3);

    // Check names (order may vary)
    std::vector<std::string> names;
    for (const auto& remote : result->remotes) {
        names.push_back(remote.name);
    }

    REQUIRE(std::find(names.begin(), names.end(), "origin") != names.end());
    REQUIRE(std::find(names.begin(), names.end(), "upstream") != names.end());
    REQUIRE(std::find(names.begin(), names.end(), "fork") != names.end());
}

TEST_CASE("Remote - remove remote", "[integration][remote]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "content")
        .with_message("Initial commit")
        .create();

    // Add remote
    ops::add_remote(temp_repo.repo(),
                    {.name = "origin", .url = "https://github.com/example/repo.git"});

    // Verify it exists
    auto list_before = ops::list_remotes(temp_repo.repo());
    REQUIRE(list_before.has_value());
    REQUIRE(list_before->remotes.size() == 1);

    // Remove the remote
    auto remove_result = ops::remove_remote(temp_repo.repo(), {.name = "origin"});
    REQUIRE(remove_result.has_value());

    // Verify it's gone
    auto list_after = ops::list_remotes(temp_repo.repo());
    REQUIRE(list_after.has_value());
    REQUIRE(list_after->remotes.empty());
}

TEST_CASE("Remote - remove one of multiple remotes", "[integration][remote]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "content")
        .with_message("Initial commit")
        .create();

    // Add multiple remotes
    ops::add_remote(temp_repo.repo(),
                    {.name = "origin", .url = "https://github.com/example/repo.git"});

    ops::add_remote(temp_repo.repo(),
                    {.name = "upstream", .url = "https://github.com/upstream/repo.git"});

    // Remove one
    ops::remove_remote(temp_repo.repo(), {.name = "origin"});

    // Verify only upstream remains
    auto result = ops::list_remotes(temp_repo.repo());
    REQUIRE(result.has_value());
    REQUIRE(result->remotes.size() == 1);
    REQUIRE(result->remotes[0].name == "upstream");
}

TEST_CASE("Remote - add with empty name fails", "[integration][remote]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "content")
        .with_message("Initial commit")
        .create();

    // Try to add remote with empty name
    auto result = ops::add_remote(temp_repo.repo(),
                                  {.name = "", .url = "https://github.com/example/repo.git"});

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == Error::Code::InvalidArgument);
}

TEST_CASE("Remote - add with empty URL fails", "[integration][remote]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "content")
        .with_message("Initial commit")
        .create();

    // Try to add remote with empty URL
    auto result = ops::add_remote(temp_repo.repo(), {.name = "origin", .url = ""});

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == Error::Code::InvalidArgument);
}

TEST_CASE("Remote - remove non-existent remote fails", "[integration][remote]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "content")
        .with_message("Initial commit")
        .create();

    // Try to remove non-existent remote
    auto result = ops::remove_remote(temp_repo.repo(), {.name = "nonexistent"});

    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("Remote - remove with empty name fails", "[integration][remote]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "content")
        .with_message("Initial commit")
        .create();

    // Try to remove with empty name
    auto result = ops::remove_remote(temp_repo.repo(), {.name = ""});

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == Error::Code::InvalidArgument);
}

TEST_CASE("Remote - different URL schemes", "[integration][remote]") {
    TempRepo temp_repo;

    // Create initial commit
    CommitBuilder(temp_repo)
        .with_file("file.txt", "content")
        .with_message("Initial commit")
        .create();

    // Add remotes with different URL schemes
    ops::add_remote(temp_repo.repo(),
                    {.name = "https", .url = "https://github.com/example/repo.git"});

    ops::add_remote(temp_repo.repo(), {.name = "ssh", .url = "git@github.com:example/repo.git"});

    ops::add_remote(temp_repo.repo(), {.name = "file", .url = "/local/path/to/repo.git"});

    // Verify all were added
    auto result = ops::list_remotes(temp_repo.repo());
    REQUIRE(result.has_value());
    REQUIRE(result->remotes.size() == 3);
}
