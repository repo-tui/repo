#pragma once

#include <vector>

#include "../domain/file_status.hpp"
#include "../result.hpp"

namespace repo {
class Repository;

namespace ops {

struct StatusParams {
    bool include_untracked = true;
    bool include_ignored = false;
};

struct StatusResult {
    std::vector<domain::FileStatus> files;

    [[nodiscard]] auto staged() const -> std::vector<domain::FileStatus>;
    [[nodiscard]] auto unstaged() const -> std::vector<domain::FileStatus>;
    [[nodiscard]] auto untracked() const -> std::vector<domain::FileStatus>;
    [[nodiscard]] auto is_clean() const -> bool;
};

auto status(Repository& repo, StatusParams params = {}) -> Result<StatusResult>;

} // namespace ops
} // namespace repo
