#include <repo/ops/common.hpp>
#include <repo/ops/rebase.hpp>
#include <repo/repository.hpp>

namespace repo::ops {

auto rebase(Repository& repo, RebaseParams params) -> Result<RebaseResult> {
    // Check if repository has commits
    if (auto err = require_commits(repo, "rebase")) {
        return std::unexpected(*err);
    }

    // Call backend rebase
    auto stats = repo.backend().rebase(repo.repo_handle(), params.onto);

    if (!stats) {
        return std::unexpected(std::move(stats.error()));
    }

    // Convert backend stats to operation result
    RebaseResult result;
    result.new_head = stats->new_head;
    result.commits_replayed = stats->commits_replayed;
    result.conflicts = std::move(stats->conflicts);

    return result;
}

} // namespace repo::ops
