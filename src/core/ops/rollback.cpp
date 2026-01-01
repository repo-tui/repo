#include <repo/ops/rollback.hpp>

namespace repo::ops {

auto rollback(Repository& repo, RollbackParams params) -> Status {
    return repo.backend().rollback(repo.repo_handle(), params.target, params.mode);
}

} // namespace repo::ops
