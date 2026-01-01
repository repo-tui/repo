#include <repo/ops/switch.hpp>
#include <repo/repository.hpp>

namespace repo::ops {

auto switch_branch(Repository& repo, SwitchParams params) -> Result<SwitchResult> {
    // Get current branch name before switching
    auto head_result = repo.backend().get_head(repo.repo_handle());
    std::string previous_branch;

    if (head_result.has_value()) {
        // If HEAD is symbolic and points to a branch
        if (head_result->is_symbolic() &&
            std::holds_alternative<std::string>(head_result->target)) {
            const auto& target_ref = std::get<std::string>(head_result->target);
            if (target_ref.starts_with("refs/heads/")) {
                previous_branch = target_ref.substr(11);
            }
        }
        // Or if HEAD name itself is a branch
        else if (head_result->name.starts_with("refs/heads/")) {
            previous_branch = head_result->name.substr(11);
        }
    }

    // Switch to the new branch
    auto switch_result = repo.backend().switch_branch(repo.repo_handle(), params.branch_name);
    if (!switch_result) {
        return std::unexpected(std::move(switch_result.error()));
    }

    return SwitchResult{.previous_branch = std::move(previous_branch),
                        .new_branch = params.branch_name};
}

} // namespace repo::ops
