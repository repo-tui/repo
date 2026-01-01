#include <repo/error.hpp>
#include <repo/ops/merge.hpp>
#include <repo/repository.hpp>

namespace repo::ops {

auto merge(Repository& repo, MergeParams params) -> Result<MergeResult> {
    // Convert strategy enum
    backend::GitBackend::MergeStrategy backend_strategy;
    switch (params.strategy) {
        case MergeParams::Strategy::FastForward:
            backend_strategy = backend::GitBackend::MergeStrategy::FastForward;
            break;
        case MergeParams::Strategy::NoFastForward:
            backend_strategy = backend::GitBackend::MergeStrategy::NoFastForward;
            break;
        case MergeParams::Strategy::FastForwardOnly:
            backend_strategy = backend::GitBackend::MergeStrategy::FastForwardOnly;
            break;
        default:
            return std::unexpected(make_error(Error::Code::InvalidArgument, "Invalid merge strategy"));
    }

    // Delegate to backend
    auto backend_result = repo.backend().merge(repo.repo_handle(), params.source, backend_strategy,
                                               params.commit, params.message);

    if (!backend_result.has_value()) {
        return std::unexpected(backend_result.error());
    }

    // Convert backend result to operation result
    MergeResult result;

    switch (backend_result->type) {
        case backend::GitBackend::MergeStatus::Type::FastForward:
            result.status = MergeResult::Status::FastForward;
            break;
        case backend::GitBackend::MergeStatus::Type::MergeCommit:
            result.status = MergeResult::Status::MergeCommit;
            break;
        case backend::GitBackend::MergeStatus::Type::Conflicts:
            result.status = MergeResult::Status::Conflicts;
            break;
        case backend::GitBackend::MergeStatus::Type::UpToDate:
            result.status = MergeResult::Status::UpToDate;
            break;
        case backend::GitBackend::MergeStatus::Type::Staged:
            result.status = MergeResult::Status::Staged;
            break;
        default:
            return std::unexpected(make_error(Error::Code::Unknown, "Unknown merge status from backend"));
    }

    result.commit_id = backend_result->commit_id;
    result.conflicts = backend_result->conflicts;

    return result;
}

} // namespace repo::ops
