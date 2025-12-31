#include <repo/ops/undo_commit.hpp>

namespace repo::ops {

auto undo_commit(Repository& repo, UndoCommitParams params) -> Status {
    return repo.backend().undo_commit(repo.repo_handle(), params.commit, params.no_commit);
}

} // namespace repo::ops
