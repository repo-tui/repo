#pragma once

#include <filesystem>
#include <vector>

#include "../result.hpp"

namespace repo {
class Repository;

namespace ops {

struct StageParams {
    std::vector<std::filesystem::path> paths;
};

struct StageResult {
    std::vector<std::filesystem::path> staged;
};

auto stage(Repository& repo, StageParams params) -> Result<StageResult>;
auto unstage(Repository& repo, StageParams params) -> Result<StageResult>;

} // namespace ops
} // namespace repo
