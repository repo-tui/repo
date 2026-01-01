#include <repo/ops/common.hpp>
#include <repo/ops/undo_commit.hpp>

namespace repo::ops {

auto undo_commit(Repository& repo, UndoCommitParams params) -> Status {
    // Check if repository has commits
    if (auto err = require_commits(repo, "undo commit")) {
        return std::unexpected(*err);
    }

    return repo.backend().undo_commit(repo.repo_handle(), params.commit, params.no_commit);
}

} // namespace repo::ops
