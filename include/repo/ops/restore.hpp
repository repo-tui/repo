#pragma once

#include <filesystem>
#include <vector>

#include "../result.hpp"

namespace repo {
class Repository;

namespace ops {

struct RestoreParams {
    std::vector<std::filesystem::path> paths;
    bool staged = false; // If true, restore staged changes; if false, restore working tree
};

struct RestoreResult {
    std::vector<std::filesystem::path> restored;
};

auto restore(Repository& repo, RestoreParams params) -> Result<RestoreResult>;

} // namespace ops
} // namespace repo
