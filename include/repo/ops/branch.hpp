#pragma once

#include <string>
#include <vector>

#include "../domain/branch.hpp"
#include "../domain/object_id.hpp"
#include "../result.hpp"

namespace repo {
class Repository;

namespace ops {

struct ListBranchesParams {
    bool include_remote = false;
};

struct ListBranchesResult {
    std::vector<domain::Branch> branches;

    [[nodiscard]] auto local() const -> std::vector<domain::Branch>;
    [[nodiscard]] auto remote() const -> std::vector<domain::Branch>;
    [[nodiscard]] auto current() const -> domain::Branch*;
};

struct CreateBranchParams {
    std::string name;
    domain::ObjectId target; // Commit to base the branch on
    bool force = false;      // Overwrite if exists
};

struct CreateBranchResult {
    domain::Branch branch;
};

struct DeleteBranchParams {
    std::string name;
};

struct RenameBranchParams {
    std::string old_name;
    std::string new_name;
    bool force = false; // Overwrite if new_name exists
};

struct RenameBranchResult {
    domain::Branch branch;
};

struct SetDefaultBranchParams {
    std::string branch_name;
};

auto list_branches(Repository& repo, ListBranchesParams params = {}) -> Result<ListBranchesResult>;
auto create_branch(Repository& repo, CreateBranchParams params) -> Result<CreateBranchResult>;
auto delete_branch(Repository& repo, DeleteBranchParams params) -> Status;
auto rename_branch(Repository& repo, RenameBranchParams params) -> Result<RenameBranchResult>;

// Set default branch for empty repository (before first commit)
// Only works when repository has no commits yet
auto set_default_branch(Repository& repo, SetDefaultBranchParams params) -> Status;

} // namespace ops
} // namespace repo
