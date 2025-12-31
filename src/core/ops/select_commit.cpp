#include <repo/ops/common.hpp>
#include <repo/ops/select_commit.hpp>

namespace repo::ops {

auto select_commit(Repository& repo, SelectCommitParams params) -> Status {
    // Check if repository has commits
    if (auto err = require_commits(repo, "select commit")) {
        return std::unexpected(*err);
    }

    return repo.backend().select_commit(repo.repo_handle(), params.commit, params.no_commit);
}

} // namespace repo::ops
