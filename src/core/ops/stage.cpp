#include <repo/ops/stage.hpp>
#include <repo/repository.hpp>

namespace repo::ops {

auto stage(Repository& repo, StageParams params) -> Result<StageResult> {
    auto index_result = repo.backend().get_index(repo.repo_handle());
    if (!index_result) {
        return std::unexpected(std::move(index_result.error()));
    }

    std::vector<std::filesystem::path> staged;
    for (const auto& path : params.paths) {
        auto result = repo.backend().stage_file(**index_result, path);
        if (!result) {
            return std::unexpected(std::move(result.error()));
        }
        staged.push_back(path);
    }

    auto write_result = repo.backend().write_index(**index_result);
    if (!write_result) {
        return std::unexpected(std::move(write_result.error()));
    }

    return StageResult{.staged = std::move(staged)};
}

auto unstage(Repository& repo, StageParams params) -> Result<StageResult> {
    auto index_result = repo.backend().get_index(repo.repo_handle());
    if (!index_result) {
        return std::unexpected(std::move(index_result.error()));
    }

    std::vector<std::filesystem::path> staged;
    for (const auto& path : params.paths) {
        auto result = repo.backend().unstage_file(**index_result, path);
        if (!result) {
            return std::unexpected(std::move(result.error()));
        }
        staged.push_back(path);
    }

    auto write_result = repo.backend().write_index(**index_result);
    if (!write_result) {
        return std::unexpected(std::move(write_result.error()));
    }

    return StageResult{.staged = std::move(staged)};
}

} // namespace repo::ops
