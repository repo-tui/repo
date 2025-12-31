#include <repo/backend/libgit2_backend.hpp>
#include <repo/ops/branch.hpp>
#include <repo/repository.hpp>

#include <fmt/format.h>

#include <git2.h>

namespace repo::ops {

auto ListBranchesResult::local() const -> std::vector<domain::Branch> {
    std::vector<domain::Branch> result;
    for (const auto& branch : branches) {
        if (!branch.is_remote) {
            result.push_back(branch);
        }
    }
    return result;
}

auto ListBranchesResult::remote() const -> std::vector<domain::Branch> {
    std::vector<domain::Branch> result;
    for (const auto& branch : branches) {
        if (branch.is_remote) {
            result.push_back(branch);
        }
    }
    return result;
}

auto ListBranchesResult::current() const -> domain::Branch* {
    for (auto& branch : const_cast<std::vector<domain::Branch>&>(branches)) {
        if (branch.is_head) {
            return &branch;
        }
    }
    return nullptr;
}

auto list_branches(Repository& repo, ListBranchesParams params) -> Result<ListBranchesResult> {
    auto branches = repo.backend().list_branches(repo.repo_handle(), params.include_remote);
    if (!branches) {
        return std::unexpected(std::move(branches.error()));
    }
    return ListBranchesResult{.branches = std::move(*branches)};
}

auto create_branch(Repository& repo, CreateBranchParams params) -> Result<CreateBranchResult> {
    auto branch =
        repo.backend().create_branch(repo.repo_handle(), params.name, params.target, params.force);
    if (!branch) {
        return std::unexpected(std::move(branch.error()));
    }
    return CreateBranchResult{.branch = std::move(*branch)};
}

auto delete_branch(Repository& repo, DeleteBranchParams params) -> Status {
    return repo.backend().delete_branch(repo.repo_handle(), params.name);
}

auto rename_branch(Repository& repo, RenameBranchParams params) -> Result<RenameBranchResult> {
    // Check if repository is empty (no commits yet)
    auto head_result = repo.head();
    if (!head_result.has_value()) {
        // Check if error is because repo is unborn (no commits)
        if (head_result.error().code == Error::Code::ReferenceNotFound ||
            head_result.error().code == Error::Code::DetachedHead ||
            head_result.error().code == Error::Code::InvalidReference) {

            // Repository has no commits yet - provide helpful guidance
            return std::unexpected(make_error(
                Error::Code::InvalidArgument,
                fmt::format("Cannot rename branch '{}' - repository has no commits yet",
                            params.old_name),
                fmt::format("The branch '{}' hasn't been created because no commits exist.\n\n"
                            "Quick fix - Change default branch name:\n"
                            "  repo branch set-default {}\n\n"
                            "Or create first commit, then rename:\n"
                            "  1. repo stage .\n"
                            "  2. repo commit -m \"Initial commit\"\n"
                            "  3. repo branch rename {} {}\n\n"
                            "Advanced (Git command):\n"
                            "  git symbolic-ref HEAD refs/heads/{}",
                            params.old_name, params.new_name, params.old_name, params.new_name,
                            params.new_name)));
        }
    }

    auto branch = repo.backend().rename_branch(repo.repo_handle(), params.old_name, params.new_name,
                                               params.force);
    if (!branch) {
        return std::unexpected(std::move(branch.error()));
    }
    return RenameBranchResult{.branch = std::move(*branch)};
}

auto set_default_branch(Repository& repo, SetDefaultBranchParams params) -> Status {
    // Check if repository already has commits
    auto head_result = repo.head();
    if (head_result.has_value()) {
        return std::unexpected(make_error(Error::Code::InvalidArgument,
                                          "Repository already has commits",
                                          "Use 'repo branch rename' to rename an existing branch"));
    }

    // Repository is empty - we can set the default branch
    // We need to set the symbolic ref HEAD to point to the new branch
    // This is done via libgit2's git_reference_symbolic_create

    // Use libgit2 backend directly to set symbolic reference
    // TODO: Add proper backend method for this
    auto ref_name = fmt::format("refs/heads/{}", params.branch_name);

    auto* git_repo = static_cast<backend::LibGit2RepoHandle&>(repo.repo_handle()).repo;

    git_reference* ref = nullptr;
    int error = git_reference_symbolic_create(&ref, git_repo, "HEAD", ref_name.c_str(), 1, nullptr);

    if (error < 0) {
        const git_error* e = git_error_last();
        return std::unexpected(make_error(Error::Code::Unknown, "Failed to set default branch",
                                          e ? e->message : "Unknown error"));
    }

    git_reference_free(ref);
    return {};
}

} // namespace repo::ops
