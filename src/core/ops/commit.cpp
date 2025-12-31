#include <repo/ops/commit.hpp>
#include <repo/repository.hpp>

#include <chrono>

namespace repo::ops {

namespace {
// Get default signature from git config or use fallback
auto get_default_signature() -> domain::Signature {
    // TODO: Read from git config
    // For now, use a default test signature
    return domain::Signature{.name = "Test User",
                             .email = "test@example.com",
                             .when = std::chrono::system_clock::now(),
                             .tz_offset = std::chrono::minutes(0)};
}
} // namespace

auto commit(Repository& repo, CommitParams params) -> Result<CommitResult> {
    // Use provided signatures or default
    auto author = params.author.value_or(get_default_signature());
    auto committer = params.committer.value_or(get_default_signature());

    // Create the commit
    auto commit_id =
        repo.backend().create_commit(repo.repo_handle(), params.message, author, committer);

    if (!commit_id) {
        return std::unexpected(std::move(commit_id.error()));
    }

    // Create commit object
    domain::Commit commit{.id = *commit_id,
                          .tree_id = {},    // TODO: Get tree ID
                          .parent_ids = {}, // TODO: Get parent IDs
                          .author = author,
                          .committer = committer,
                          .message = params.message};

    // TODO: Calculate diff stats (files_changed, insertions, deletions)
    return CommitResult{
        .commit = std::move(commit), .files_changed = 0, .insertions = 0, .deletions = 0};
}

auto show_commit(Repository& repo, ShowCommitParams params) -> Result<ShowCommitResult> {
    domain::ObjectId oid;

    // Try to parse as ObjectId first
    auto oid_result = domain::ObjectId::from_string(params.ref);
    if (oid_result) {
        oid = *oid_result;
    } else {
        // Not a direct ObjectId, try to resolve as a reference (e.g., "HEAD", "main")
        auto ref = repo.backend().resolve_reference(repo.repo_handle(), params.ref);
        if (!ref) {
            // Still couldn't resolve, return error
            Error err;
            err.code = Error::Code::ReferenceNotFound;
            err.message = "Could not resolve ref: " + params.ref;
            return std::unexpected(std::move(err));
        }

        // Get the target OID from the reference
        if (std::holds_alternative<domain::ObjectId>(ref->target)) {
            oid = std::get<domain::ObjectId>(ref->target);
        } else {
            // It's a symbolic reference, need to resolve it further
            auto target_name = std::get<std::string>(ref->target);
            auto target_ref = repo.backend().resolve_reference(repo.repo_handle(), target_name);
            if (!target_ref || !std::holds_alternative<domain::ObjectId>(target_ref->target)) {
                Error err;
                err.code = Error::Code::ReferenceNotFound;
                err.message = "Could not resolve symbolic ref: " + target_name;
                return std::unexpected(std::move(err));
            }
            oid = std::get<domain::ObjectId>(target_ref->target);
        }
    }

    // Get the commit
    auto commit = repo.backend().get_commit(repo.repo_handle(), oid);
    if (!commit) {
        return std::unexpected(std::move(commit.error()));
    }

    return ShowCommitResult{.commit = *commit};
}

auto amend_commit(Repository& repo, AmendCommitParams params) -> Result<AmendCommitResult> {
    // Call backend amend_commit
    auto commit_id = repo.backend().amend_commit(repo.repo_handle(), params.message, params.author,
                                                 params.committer);

    if (!commit_id) {
        return std::unexpected(std::move(commit_id.error()));
    }

    // Get the amended commit
    auto commit = repo.backend().get_commit(repo.repo_handle(), *commit_id);
    if (!commit) {
        return std::unexpected(std::move(commit.error()));
    }

    return AmendCommitResult{.commit = *commit};
}

} // namespace repo::ops
