#include <repo/ops/common.hpp>
#include <repo/ops/rollback.hpp>

namespace repo::ops {

auto rollback(Repository& repo, RollbackParams params) -> Status {
    // Check if repository has commits
    if (auto err = require_commits(repo, "rollback")) {
        return std::unexpected(*err);
    }

    return repo.backend().rollback(repo.repo_handle(), params.target, params.mode);
}

} // namespace repo::ops
