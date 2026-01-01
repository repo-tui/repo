#include <repo/ops/list_commits.hpp>
#include <repo/ops/merge.hpp>
#include <repo/ops/rebase.hpp>
#include <repo/ops/rollback.hpp>
#include <repo/ops/select_commit.hpp>
#include <repo/ops/switch.hpp>
#include <repo/ops/undo_commit.hpp>

#include <catch2/catch_all.hpp>

#include "../test_utils.hpp"

using namespace repo;
using namespace repo::test;

TEST_CASE("list_commits - fails with helpful message on empty repo", "[integration][empty]") {
    TempRepo temp_repo;

    // Try to list commits on empty repository
    auto result = ops::list_commits(temp_repo.repo());

    // Should fail
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == Error::Code::InvalidReference);

    // Error message should be helpful
    REQUIRE(result.error().message.find("no commits yet") != std::string::npos);

    // Error detail should provide guidance
    REQUIRE(result.error().detail.has_value());
    REQUIRE(result.error().detail->find("repo stage") != std::string::npos);
    REQUIRE(result.error().detail->find("repo commit create") != std::string::npos);
}

TEST_CASE("switch_branch - fails with helpful message on empty repo", "[integration][empty]") {
    TempRepo temp_repo;

    // Try to switch branches on empty repository
    auto result = ops::switch_branch(temp_repo.repo(), {.branch_name = "develop"});

    // Should fail
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == Error::Code::InvalidReference);

    // Error message should be helpful
    REQUIRE(result.error().message.find("no commits yet") != std::string::npos);

    // Error detail should provide guidance
    REQUIRE(result.error().detail.has_value());
    REQUIRE(result.error().detail->find("repo stage") != std::string::npos);
}

TEST_CASE("select_commit - fails with helpful message on empty repo", "[integration][empty]") {
    TempRepo temp_repo;

    // Create a dummy object ID (won't be found, but that's not the point)
    domain::ObjectId dummy_commit;

    // Try to select commit on empty repository
    auto result =
        ops::select_commit(temp_repo.repo(), {.commit = dummy_commit, .no_commit = false});

    // Should fail
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == Error::Code::InvalidReference);

    // Error message should be helpful
    REQUIRE(result.error().message.find("no commits yet") != std::string::npos);

    // Error detail should provide guidance
    REQUIRE(result.error().detail.has_value());
    REQUIRE(result.error().detail->find("Create your first commit") != std::string::npos);
}

TEST_CASE("undo_commit - fails with helpful message on empty repo", "[integration][empty]") {
    TempRepo temp_repo;

    // Create a dummy object ID
    domain::ObjectId dummy_commit;

    // Try to undo commit on empty repository
    auto result = ops::undo_commit(temp_repo.repo(), {.commit = dummy_commit, .no_commit = false});

    // Should fail
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == Error::Code::InvalidReference);

    // Error message should be helpful
    REQUIRE(result.error().message.find("no commits yet") != std::string::npos);

    // Error detail should provide guidance
    REQUIRE(result.error().detail.has_value());
}

TEST_CASE("rollback - fails with helpful message on empty repo", "[integration][empty]") {
    TempRepo temp_repo;

    // Create a dummy object ID
    domain::ObjectId dummy_target;

    // Try to rollback on empty repository
    auto result =
        ops::rollback(temp_repo.repo(), {.target = dummy_target, .mode = ops::RollbackMode::Soft});

    // Should fail
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == Error::Code::InvalidReference);

    // Error message should be helpful
    REQUIRE(result.error().message.find("no commits yet") != std::string::npos);

    // Error detail should provide guidance
    REQUIRE(result.error().detail.has_value());
}

TEST_CASE("merge - fails with helpful message on empty repo", "[integration][empty]") {
    TempRepo temp_repo;

    // Try to merge on empty repository
    auto result = ops::merge(temp_repo.repo(), {.source = "develop",
                                                .strategy = ops::MergeParams::Strategy::FastForward,
                                                .commit = true,
                                                .message = "Merge develop"});

    // Should fail
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == Error::Code::InvalidReference);

    // Error message should be helpful
    REQUIRE(result.error().message.find("no commits yet") != std::string::npos);

    // Error detail should provide guidance
    REQUIRE(result.error().detail.has_value());
}

TEST_CASE("rebase - fails with helpful message on empty repo", "[integration][empty]") {
    TempRepo temp_repo;

    // Try to rebase on empty repository
    auto result = ops::rebase(temp_repo.repo(), {.onto = "main"});

    // Should fail
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == Error::Code::InvalidReference);

    // Error message should be helpful
    REQUIRE(result.error().message.find("no commits yet") != std::string::npos);

    // Error detail should provide guidance
    REQUIRE(result.error().detail.has_value());
}
