#include <repo/ops/restore.hpp>
#include <repo/repository.hpp>

namespace repo::ops {

auto restore(Repository& repo, RestoreParams params) -> Result<RestoreResult> {
    if (params.paths.empty()) {
        Error err;
        err.code = Error::Code::InvalidArgument;
        err.message = "No paths specified for restore";
        return std::unexpected(std::move(err));
    }

    auto restore_result =
        repo.backend().restore_files(repo.repo_handle(), params.paths, params.staged);

    if (!restore_result) {
        return std::unexpected(std::move(restore_result.error()));
    }

    return RestoreResult{.restored = params.paths};
}

} // namespace repo::ops
