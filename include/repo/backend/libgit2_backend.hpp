#pragma once

#include <git2.h>
#include <memory>

#include "git_backend.hpp"

namespace repo::backend {

/// RAII wrapper for git_repository
struct LibGit2RepoHandle : RepoHandle {
    git_repository* repo = nullptr;

    explicit LibGit2RepoHandle(git_repository* r) : repo(r) {}
    ~LibGit2RepoHandle() {
        if (repo) {
            git_repository_free(repo);
        }
    }

    // Non-copyable, movable
    LibGit2RepoHandle(const LibGit2RepoHandle&) = delete;
    auto operator=(const LibGit2RepoHandle&) -> LibGit2RepoHandle& = delete;
    LibGit2RepoHandle(LibGit2RepoHandle&& other) noexcept : repo(other.repo) {
        other.repo = nullptr;
    }
    auto operator=(LibGit2RepoHandle&& other) noexcept -> LibGit2RepoHandle& {
        if (this != &other) {
            if (repo)
                git_repository_free(repo);
            repo = other.repo;
            other.repo = nullptr;
        }
        return *this;
    }
};

/// RAII wrapper for git_index
struct LibGit2IndexHandle : IndexHandle {
    git_index* index = nullptr;

    explicit LibGit2IndexHandle(git_index* idx) : index(idx) {}
    ~LibGit2IndexHandle() {
        if (index) {
            git_index_free(index);
        }
    }

    // Non-copyable, movable
    LibGit2IndexHandle(const LibGit2IndexHandle&) = delete;
    auto operator=(const LibGit2IndexHandle&) -> LibGit2IndexHandle& = delete;
    LibGit2IndexHandle(LibGit2IndexHandle&& other) noexcept : index(other.index) {
        other.index = nullptr;
    }
    auto operator=(LibGit2IndexHandle&& other) noexcept -> LibGit2IndexHandle& {
        if (this != &other) {
            if (index)
                git_index_free(index);
            index = other.index;
            other.index = nullptr;
        }
        return *this;
    }
};

/// libgit2 implementation of GitBackend
class LibGit2Backend : public GitBackend {
  public:
    LibGit2Backend();
    ~LibGit2Backend() override;

    // Repository operations
    auto open(const std::filesystem::path& path) -> Result<std::unique_ptr<RepoHandle>> override;
    auto init(const std::filesystem::path& path, bool bare)
        -> Result<std::unique_ptr<RepoHandle>> override;
    auto is_bare(const RepoHandle& repo) -> bool override;
    auto workdir(const RepoHandle& repo) -> std::filesystem::path override;
    auto git_dir(const RepoHandle& repo) -> std::filesystem::path override;

    // Index operations
    auto get_index(RepoHandle& repo) -> Result<std::unique_ptr<IndexHandle>> override;
    auto stage_file(IndexHandle& index, const std::filesystem::path& path) -> Status override;
    auto unstage_file(IndexHandle& index, const std::filesystem::path& path) -> Status override;
    auto write_index(IndexHandle& index) -> Status override;

    // Status operations
    auto get_status(const RepoHandle& repo) -> Result<std::vector<domain::FileStatus>> override;

    // Commit operations
    auto create_commit(RepoHandle& repo, const std::string& message,
                       const domain::Signature& author, const domain::Signature& committer)
        -> Result<domain::ObjectId> override;

    auto amend_commit(RepoHandle& repo, const std::optional<std::string>& message,
                      const std::optional<domain::Signature>& author,
                      const std::optional<domain::Signature>& committer)
        -> Result<domain::ObjectId> override;

    auto get_commit(const RepoHandle& repo, const domain::ObjectId& oid)
        -> Result<domain::Commit> override;

    // Branch operations
    auto list_branches(const RepoHandle& repo, bool include_remote)
        -> Result<std::vector<domain::Branch>> override;

    auto create_branch(RepoHandle& repo, const std::string& name, const domain::ObjectId& target,
                       bool force) -> Result<domain::Branch> override;

    auto delete_branch(RepoHandle& repo, const std::string& name) -> Status override;

    auto rename_branch(RepoHandle& repo, const std::string& old_name, const std::string& new_name,
                       bool force) -> Result<domain::Branch> override;

    auto switch_branch(RepoHandle& repo, const std::string& branch_name) -> Status override;

    // Restore operations
    auto restore_files(RepoHandle& repo, const std::vector<std::filesystem::path>& paths,
                       bool staged) -> Status override;

    // Reference operations
    auto get_head(const RepoHandle& repo) -> Result<domain::Reference> override;
    auto resolve_reference(const RepoHandle& repo, const std::string& name)
        -> Result<domain::Reference> override;

    // Diff operations
    auto diff_index_to_workdir(const RepoHandle& repo)
        -> Result<std::vector<domain::FileDiff>> override;

    auto diff_tree_to_index(const RepoHandle& repo)
        -> Result<std::vector<domain::FileDiff>> override;

    // Remote operations
    auto list_remotes(const RepoHandle& repo) -> Result<std::vector<domain::Remote>> override;

    auto add_remote(RepoHandle& repo, const std::string& name, const std::string& url)
        -> Status override;

    auto remove_remote(RepoHandle& repo, const std::string& name) -> Status override;

    auto fetch(RepoHandle& repo, const std::string& remote, const std::string& refspec, bool prune,
               bool tags) -> Result<FetchStats> override;

    auto push(RepoHandle& repo, const std::string& remote, const std::string& refspec, bool force,
              bool set_upstream) -> Result<PushStats> override;

    // Tag operations
    auto list_tags(const RepoHandle& repo) -> Result<std::vector<domain::Tag>> override;

    auto create_tag(RepoHandle& repo, const std::string& name, const domain::ObjectId& target,
                    const std::string& message, const domain::Signature& tagger, bool force)
        -> Status override;

    auto delete_tag(RepoHandle& repo, const std::string& name) -> Status override;

    // Stash operations
    auto list_stashes(const RepoHandle& repo) -> Result<std::vector<domain::Stash>> override;

    auto create_stash(RepoHandle& repo, const std::string& message,
                      const domain::Signature& stasher, bool include_untracked, bool keep_index)
        -> Result<domain::ObjectId> override;

    auto apply_stash(RepoHandle& repo, size_t index, bool reinstate_index) -> Status override;

    auto drop_stash(RepoHandle& repo, size_t index) -> Status override;

    // Rollback operations (move branch pointer to previous commit)
    auto rollback(RepoHandle& repo, const domain::ObjectId& target, ResetMode mode)
        -> Status override;

    // Select commit operations (apply commit to current branch)
    auto select_commit(RepoHandle& repo, const domain::ObjectId& commit, bool no_commit)
        -> Status override;

    // Undo commit operations (create new commit that reverses changes)
    auto undo_commit(RepoHandle& repo, const domain::ObjectId& commit, bool no_commit)
        -> Status override;

    // Merge operations
    auto merge(RepoHandle& repo, const std::string& source, MergeStrategy strategy, bool commit,
               const std::string& message) -> Result<MergeStatus> override;

    auto rebase(RepoHandle& repo, const std::string& onto) -> Result<RebaseStats> override;

    // Authentication
    auto set_credential_callback(CredentialCallback callback) -> void override;

  private:
    // Helper to convert libgit2 errors to our Error type
    auto make_libgit2_error(int error_code, const std::string& context) -> Error;

    // Helper to get git_repository* from handle
    static auto get_repo(const RepoHandle& handle) -> git_repository*;
    static auto get_repo(RepoHandle& handle) -> git_repository*;

    // Helper to get git_index* from handle
    static auto get_index(IndexHandle& handle) -> git_index*;

    // Convert domain types to/from libgit2 types
    static auto to_oid(const domain::ObjectId& oid) -> git_oid;
    static auto from_oid(const git_oid& oid) -> domain::ObjectId;
    static auto to_signature(const domain::Signature& sig) -> git_signature*;
    static auto from_signature(const git_signature* sig) -> domain::Signature;

    // Diff conversion
    auto convert_diff_to_domain(git_diff* diff) -> Result<std::vector<domain::FileDiff>>;

    // Default authentication strategy (used if no custom callback provided)
    auto default_authentication_strategy(const AuthenticationContext& context)
        -> Result<Credential>;

    // Authentication state
    std::optional<CredentialCallback> credential_callback_;
    AuthenticationTracker auth_tracker_;

    // Make auth_tracker_ accessible to credential callback
    friend auto credential_callback(git_credential**, const char*, const char*, unsigned int,
                                   void*) -> int;
};

} // namespace repo::backend
