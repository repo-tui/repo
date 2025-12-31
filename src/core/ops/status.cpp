#include <repo/ops/status.hpp>
#include <repo/repository.hpp>

namespace repo::ops {

auto StatusResult::staged() const -> std::vector<domain::FileStatus> {
    std::vector<domain::FileStatus> result;
    for (const auto& file : files) {
        if (file.is_staged()) {
            result.push_back(file);
        }
    }
    return result;
}

auto StatusResult::unstaged() const -> std::vector<domain::FileStatus> {
    std::vector<domain::FileStatus> result;
    for (const auto& file : files) {
        if (file.is_unstaged()) {
            result.push_back(file);
        }
    }
    return result;
}

auto StatusResult::untracked() const -> std::vector<domain::FileStatus> {
    std::vector<domain::FileStatus> result;
    for (const auto& file : files) {
        if (file.is_untracked()) {
            result.push_back(file);
        }
    }
    return result;
}

auto StatusResult::is_clean() const -> bool {
    return files.empty();
}

auto status(Repository& repo, StatusParams /*params*/) -> Result<StatusResult> {
    auto files = repo.backend().get_status(repo.repo_handle());
    if (!files) {
        return std::unexpected(std::move(files.error()));
    }
    return StatusResult{.files = std::move(*files)};
}

} // namespace repo::ops
