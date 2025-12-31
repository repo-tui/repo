#include <repo/ops/stash.hpp>

namespace repo::ops {

auto list_stashes(Repository& repo) -> Result<ListStashesResult> {
    auto backend_result = repo.backend().list_stashes(repo.repo_handle());

    if (!backend_result.has_value()) {
        return std::unexpected(backend_result.error());
    }

    ListStashesResult result;
    result.stashes = std::move(*backend_result);
    return result;
}

auto create_stash(Repository& repo, CreateStashParams params) -> Result<CreateStashResult> {
    // Validate message
    if (params.message.empty()) {
        Error err;
        err.code = Error::Code::InvalidArgument;
        err.message = "Stash message cannot be empty";
        return std::unexpected(std::move(err));
    }

    // Validate stasher signature
    if (params.stasher.name.empty()) {
        Error err;
        err.code = Error::Code::InvalidArgument;
        err.message = "Stasher name cannot be empty";
        return std::unexpected(std::move(err));
    }

    if (params.stasher.email.empty()) {
        Error err;
        err.code = Error::Code::InvalidArgument;
        err.message = "Stasher email cannot be empty";
        return std::unexpected(std::move(err));
    }

    auto stash_id_result =
        repo.backend().create_stash(repo.repo_handle(), params.message, params.stasher,
                                    params.include_untracked, params.keep_index);

    if (!stash_id_result.has_value()) {
        return std::unexpected(stash_id_result.error());
    }

    CreateStashResult result;
    result.stash_id = *stash_id_result;
    return result;
}

auto apply_stash(Repository& repo, ApplyStashParams params) -> Status {
    return repo.backend().apply_stash(repo.repo_handle(), params.index, params.reinstate_index);
}

auto pop_stash(Repository& repo, PopStashParams params) -> Status {
    // Apply the stash first
    auto apply_result =
        repo.backend().apply_stash(repo.repo_handle(), params.index, params.reinstate_index);

    if (!apply_result.has_value()) {
        return apply_result;
    }

    // If apply succeeded, drop the stash
    return repo.backend().drop_stash(repo.repo_handle(), params.index);
}

auto drop_stash(Repository& repo, DropStashParams params) -> Status {
    return repo.backend().drop_stash(repo.repo_handle(), params.index);
}

} // namespace repo::ops
