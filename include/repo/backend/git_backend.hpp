#pragma once

#include <filesystem>
#include <memory>
#include <span>
#include <vector>

#include "../domain/branch.hpp"
#include "../domain/commit.hpp"
#include "../domain/diff.hpp"
#include "../domain/file_status.hpp"
#include "../domain/index.hpp"
#include "../domain/reference.hpp"
#include "../domain/remote.hpp"
#include "../domain/stash.hpp"
#include "../domain/tag.hpp"
#include "../result.hpp"
#include "credential_callback.hpp"

namespace repo::backend {

/// Base class for opaque handles
struct RepoHandle {
    virtual ~RepoHandle() = default;
};

struct IndexHandle {
    virtual ~IndexHandle() = default;
};

/// Abstract interface for Git backend
/// This allows us to swap libgit2 for other implementations or mocks
class GitBackend {
  public:
    virtual ~GitBackend() = default;

    // Repository operations
    virtual auto open(const std::filesystem::path& path) -> Result<std::unique_ptr<RepoHandle>> = 0;
    virtual auto init(const std::filesystem::path& path, bool bare)
        -> Result<std::unique_ptr<RepoHandle>> = 0;
    virtual auto is_bare(const RepoHandle& repo) -> bool = 0;
    virtual auto workdir(const RepoHandle& repo) -> std::filesystem::path = 0;
    virtual auto git_dir(const RepoHandle& repo) -> std::filesystem::path = 0;

    // Authentication
    virtual auto set_credential_callback(CredentialCallback callback) -> void = 0;

    // Index operations
    virtual auto get_index(RepoHandle& repo) -> Result<std::unique_ptr<IndexHandle>> = 0;
    virtual auto stage_file(IndexHandle& index, const std::filesystem::path& path) -> Status = 0;
    virtual auto unstage_file(IndexHandle& index, const std::filesystem::path& path) -> Status = 0;
    virtual auto write_index(IndexHandle& index) -> Status = 0;

    // Status operations
    virtual auto get_status(const RepoHandle& repo) -> Result<std::vector<domain::FileStatus>> = 0;

    // Commit operations
    virtual auto create_commit(RepoHandle& repo, const std::string& message,
                               const domain::Signature& author, const domain::Signature& committer)
        -> Result<domain::ObjectId> = 0;

    virtual auto amend_commit(RepoHandle& repo, const std::optional<std::string>& message,
                              const std::optional<domain::Signature>& author,
                              const std::optional<domain::Signature>& committer)
        -> Result<domain::ObjectId> = 0;

    virtual auto get_commit(const RepoHandle& repo, const domain::ObjectId& oid)
        -> Result<domain::Commit> = 0;

    // Branch operations
    virtual auto list_branches(const RepoHandle& repo, bool include_remote)
        -> Result<std::vector<domain::Branch>> = 0;

    virtual auto create_branch(RepoHandle& repo, const std::string& name,
                               const domain::ObjectId& target, bool force)
        -> Result<domain::Branch> = 0;

    virtual auto delete_branch(RepoHandle& repo, const std::string& name) -> Status = 0;

    virtual auto rename_branch(RepoHandle& repo, const std::string& old_name,
                               const std::string& new_name, bool force)
        -> Result<domain::Branch> = 0;

    virtual auto switch_branch(RepoHandle& repo, const std::string& branch_name) -> Status = 0;

    // Restore operations
    virtual auto restore_files(RepoHandle& repo, const std::vector<std::filesystem::path>& paths,
                               bool staged) -> Status = 0;

    // Reference operations
    virtual auto get_head(const RepoHandle& repo) -> Result<domain::Reference> = 0;
    virtual auto resolve_reference(const RepoHandle& repo, const std::string& name)
        -> Result<domain::Reference> = 0;

    // Diff operations
    virtual auto diff_index_to_workdir(const RepoHandle& repo)
        -> Result<std::vector<domain::FileDiff>> = 0;

    virtual auto diff_tree_to_index(const RepoHandle& repo)
        -> Result<std::vector<domain::FileDiff>> = 0;

    // Remote operations
    virtual auto list_remotes(const RepoHandle& repo) -> Result<std::vector<domain::Remote>> = 0;

    virtual auto add_remote(RepoHandle& repo, const std::string& name, const std::string& url)
        -> Status = 0;

    virtual auto remove_remote(RepoHandle& repo, const std::string& name) -> Status = 0;

    struct FetchStats {
        size_t received_objects = 0;
        size_t indexed_objects = 0;
        size_t received_bytes = 0;
        std::vector<std::string> updated_refs;
    };

    virtual auto fetch(RepoHandle& repo, const std::string& remote, const std::string& refspec,
                       bool prune, bool tags) -> Result<FetchStats> = 0;

    struct PushStats {
        size_t sent_objects = 0;
        size_t sent_bytes = 0;
        std::vector<std::string> updated_refs;
    };

    virtual auto push(RepoHandle& repo, const std::string& remote, const std::string& refspec,
                      bool force, bool set_upstream) -> Result<PushStats> = 0;

    // Tag operations
    virtual auto list_tags(const RepoHandle& repo) -> Result<std::vector<domain::Tag>> = 0;

    virtual auto create_tag(RepoHandle& repo, const std::string& name,
                            const domain::ObjectId& target, const std::string& message,
                            const domain::Signature& tagger, bool force) -> Status = 0;

    virtual auto delete_tag(RepoHandle& repo, const std::string& name) -> Status = 0;

    // Stash operations
    virtual auto list_stashes(const RepoHandle& repo) -> Result<std::vector<domain::Stash>> = 0;

    virtual auto create_stash(RepoHandle& repo, const std::string& message,
                              const domain::Signature& stasher, bool include_untracked,
                              bool keep_index) -> Result<domain::ObjectId> = 0;

    virtual auto apply_stash(RepoHandle& repo, size_t index, bool reinstate_index) -> Status = 0;

    virtual auto drop_stash(RepoHandle& repo, size_t index) -> Status = 0;

    // Rollback operations (move branch pointer to previous commit)
    enum class ResetMode {
        Soft,  // Move branch pointer only
        Mixed, // Move branch and update index (default)
        Hard   // Move branch and update both index and working directory
    };

    virtual auto rollback(RepoHandle& repo, const domain::ObjectId& target, ResetMode mode)
        -> Status = 0;

    // Select commit operations (apply commit to current branch)
    virtual auto select_commit(RepoHandle& repo, const domain::ObjectId& commit, bool no_commit)
        -> Status = 0;

    // Undo commit operations (create new commit that reverses changes)
    virtual auto undo_commit(RepoHandle& repo, const domain::ObjectId& commit, bool no_commit)
        -> Status = 0;

    // Merge operations
    enum class MergeStrategy { FastForward, NoFastForward, FastForwardOnly };

    struct MergeStatus {
        enum class Type { FastForward, MergeCommit, Conflicts, UpToDate, Staged };

        Type type;
        std::string commit_id;
        std::vector<std::filesystem::path> conflicts;
    };

    virtual auto merge(RepoHandle& repo, const std::string& source, MergeStrategy strategy,
                       bool commit, const std::string& message) -> Result<MergeStatus> = 0;

    struct RebaseStats {
        domain::ObjectId new_head;
        size_t commits_replayed;
        std::vector<std::string> conflicts;
    };

    virtual auto rebase(RepoHandle& repo, const std::string& onto) -> Result<RebaseStats> = 0;
};

} // namespace repo::backend
