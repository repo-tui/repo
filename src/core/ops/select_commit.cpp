#include <repo/ops/select_commit.hpp>

namespace repo::ops {

auto select_commit(Repository& repo, SelectCommitParams params) -> Status {
    return repo.backend().select_commit(repo.repo_handle(), params.commit, params.no_commit);
}

} // namespace repo::ops
