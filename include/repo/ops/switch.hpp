#pragma once

#include <string>

#include "../result.hpp"

namespace repo {
class Repository;

namespace ops {

struct SwitchParams {
    std::string branch_name;
    bool create_if_missing = false; // Future: create branch if it doesn't exist
    bool detach = false;            // Future: detach HEAD (not implemented yet)
};

struct SwitchResult {
    std::string previous_branch;
    std::string new_branch;
};

auto switch_branch(Repository& repo, SwitchParams params) -> Result<SwitchResult>;

} // namespace ops
} // namespace repo
