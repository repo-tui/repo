#include <repo/ops/list_commits.hpp>
#include <repo/repository.hpp>

namespace repo::ops {

auto ListCommitsResult::first() const -> const domain::Commit* {
    return commits.empty() ? nullptr : &commits.front();
}

auto ListCommitsResult::last() const -> const domain::Commit* {
    return commits.empty() ? nullptr : &commits.back();
}

auto list_commits(Repository& repo, ListCommitsParams params) -> Result<ListCommitsResult> {
    // Determine starting point
    std::string ref_name = params.ref_name.value_or("HEAD");

    // Resolve the reference to get the starting commit
    auto ref_result = repo.backend().resolve_reference(repo.repo_handle(), ref_name);
    if (!ref_result) {
        return std::unexpected(std::move(ref_result.error()));
    }

    // Get the OID to start from
    domain::ObjectId start_oid;
    if (ref_result->is_direct()) {
        start_oid = std::get<domain::ObjectId>(ref_result->target);
    } else {
        // Resolve symbolic reference
        auto resolved = repo.backend().resolve_reference(repo.repo_handle(),
                                                         std::get<std::string>(ref_result->target));
        if (!resolved || !resolved->is_direct()) {
            return std::unexpected(
                make_error(Error::Code::InvalidReference, "Could not resolve reference to commit"));
        }
        start_oid = std::get<domain::ObjectId>(resolved->target);
    }

    // Get the first commit
    auto first_commit = repo.backend().get_commit(repo.repo_handle(), start_oid);
    if (!first_commit) {
        return std::unexpected(std::move(first_commit.error()));
    }

    // Build commit list by walking parent chain
    std::vector<domain::Commit> commits;
    commits.push_back(*first_commit);

    size_t count = 1;
    auto current_commit = *first_commit;

    while (true) {
        // Check max count
        if (params.max_count && count >= *params.max_count) {
            break;
        }

        // Check if we've reached the end
        if (current_commit.parent_ids.empty()) {
            break;
        }

        // Get first parent (follow main history)
        auto parent_id = current_commit.parent_ids[0];

        // Check until condition
        if (params.until && parent_id == *params.until) {
            break;
        }

        // Get parent commit
        auto parent_commit = repo.backend().get_commit(repo.repo_handle(), parent_id);
        if (!parent_commit) {
            break; // Stop on error
        }

        commits.push_back(*parent_commit);
        current_commit = *parent_commit;
        count++;
    }

    return ListCommitsResult{.commits = std::move(commits)};
}

} // namespace repo::ops
