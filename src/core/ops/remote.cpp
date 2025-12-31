#include <repo/ops/branch.hpp>
#include <repo/ops/merge.hpp>
#include <repo/ops/remote.hpp>
#include <repo/repository.hpp>

#include <fmt/format.h>

namespace repo::ops {

auto list_remotes(Repository& repo) -> Result<ListRemotesResult> {
    auto remotes = repo.backend().list_remotes(repo.repo_handle());

    if (!remotes) {
        return std::unexpected(std::move(remotes.error()));
    }

    return ListRemotesResult{.remotes = std::move(*remotes)};
}

auto add_remote(Repository& repo, AddRemoteParams params) -> Status {
    if (params.name.empty()) {
        Error err;
        err.code = Error::Code::InvalidArgument;
        err.message = "Remote name cannot be empty";
        return std::unexpected(std::move(err));
    }

    if (params.url.empty()) {
        Error err;
        err.code = Error::Code::InvalidArgument;
        err.message = "Remote URL cannot be empty";
        return std::unexpected(std::move(err));
    }

    return repo.backend().add_remote(repo.repo_handle(), params.name, params.url);
}

auto remove_remote(Repository& repo, RemoveRemoteParams params) -> Status {
    if (params.name.empty()) {
        Error err;
        err.code = Error::Code::InvalidArgument;
        err.message = "Remote name cannot be empty";
        return std::unexpected(std::move(err));
    }

    return repo.backend().remove_remote(repo.repo_handle(), params.name);
}

auto fetch(Repository& repo, FetchParams params) -> Result<FetchResult> {
    // Use "origin" as default remote if not specified
    std::string remote = params.remote.empty() ? "origin" : params.remote;

    // Perform fetch
    auto stats =
        repo.backend().fetch(repo.repo_handle(), remote, params.refspec, params.prune, params.tags);

    if (!stats) {
        return std::unexpected(std::move(stats.error()));
    }

    // Convert backend stats to operation result
    FetchResult result;
    result.received_objects = stats->received_objects;
    result.indexed_objects = stats->indexed_objects;
    result.received_bytes = stats->received_bytes;
    result.updated_refs = std::move(stats->updated_refs);

    return result;
}

auto push(Repository& repo, PushParams params) -> Result<PushResult> {
    // Use "origin" as default remote if not specified
    std::string remote = params.remote.empty() ? "origin" : params.remote;

    // Check if current branch has upstream configured (unless set_upstream is true)
    if (!params.set_upstream && params.refspec.empty()) {
        auto branches_result = list_branches(repo, {.include_remote = false});
        if (branches_result.has_value()) {
            if (auto* current = branches_result->current()) {
                if (!current->upstream.has_value()) {
                    return std::unexpected(make_error(
                        Error::Code::InvalidArgument,
                        fmt::format("No upstream branch configured for '{}'", current->name),
                        "The current branch has no upstream branch configured.\n\n"
                        "To push and set an upstream branch:\n"
                        "  repo push --set-upstream\n"
                        "  repo push -u\n\n"
                        "Or set upstream for a specific remote:\n"
                        "  repo push --set-upstream origin"));
                }
            }
        }
    }

    // Perform push
    auto stats = repo.backend().push(repo.repo_handle(), remote, params.refspec, params.force,
                                     params.set_upstream);

    if (!stats) {
        return std::unexpected(std::move(stats.error()));
    }

    // Convert backend stats to operation result
    PushResult result;
    result.sent_objects = stats->sent_objects;
    result.sent_bytes = stats->sent_bytes;
    result.updated_refs = std::move(stats->updated_refs);

    return result;
}

auto pull(Repository& repo, PullParams params) -> Result<PullResult> {
    // Use "origin" as default remote if not specified
    std::string remote = params.remote.empty() ? "origin" : params.remote;

    // Get current branch name
    auto head = repo.backend().get_head(repo.repo_handle());
    if (!head) {
        return std::unexpected(std::move(head.error()));
    }

    // Extract branch name from HEAD reference (e.g., "refs/heads/main" -> "main")
    std::string branch_name;
    if (head->name.starts_with("refs/heads/")) {
        branch_name = head->name.substr(11); // Remove "refs/heads/" prefix
    } else {
        Error err;
        err.code = Error::Code::DetachedHead;
        err.message = "Cannot pull from detached HEAD";
        return std::unexpected(std::move(err));
    }

    // Step 1: Fetch from remote
    auto fetch_result =
        fetch(repo, {.remote = remote, .refspec = "", .prune = params.prune, .tags = params.tags});

    if (!fetch_result) {
        return std::unexpected(std::move(fetch_result.error()));
    }

    // Step 2: Merge remote-tracking branch into current branch
    // The remote-tracking branch is typically "remote/branch" (e.g., "origin/main")
    std::string remote_branch = remote + "/" + branch_name;

    // Check if we actually need to merge (compare HEAD with remote branch)
    // For now, attempt the merge - it will report "up to date" if nothing changed

    if (params.rebase) {
        // Rebase not yet implemented
        Error err;
        err.code = Error::Code::NotImplemented;
        err.message = "Rebase is not yet implemented";
        return std::unexpected(std::move(err));
    }

    // Perform merge
    auto merge_result =
        merge(repo, {.source = remote_branch,
                     .strategy = MergeParams::Strategy::FastForward, // Prefer fast-forward
                     .commit = true,
                     .message = "Merge " + remote_branch + " into " + branch_name});

    if (!merge_result) {
        return std::unexpected(std::move(merge_result.error()));
    }

    // Build result
    PullResult result;
    result.fetch_result = std::move(*fetch_result);

    // Determine merge type and whether we updated
    switch (merge_result->status) {
        case MergeResult::Status::FastForward:
            result.merge_type = "fast-forward";
            result.updated = true;
            break;
        case MergeResult::Status::MergeCommit:
            result.merge_type = "merge";
            result.updated = true;
            break;
        case MergeResult::Status::UpToDate:
            result.merge_type = "up-to-date";
            result.updated = false;
            break;
        case MergeResult::Status::Conflicts:
            result.merge_type = "conflicts";
            result.updated = false;
            break;
        case MergeResult::Status::Staged:
            result.merge_type = "staged";
            result.updated = false;
            break;
    }

    return result;
}

} // namespace repo::ops
