#include <repo/backend/libgit2_backend.hpp>

#include <fmt/format.h>

namespace repo::backend {

LibGit2Backend::LibGit2Backend() {
    git_libgit2_init();
}

LibGit2Backend::~LibGit2Backend() {
    git_libgit2_shutdown();
}

// Helper implementations

auto LibGit2Backend::get_repo(const RepoHandle& handle) -> git_repository* {
    return static_cast<const LibGit2RepoHandle&>(handle).repo;
}

auto LibGit2Backend::get_repo(RepoHandle& handle) -> git_repository* {
    return static_cast<LibGit2RepoHandle&>(handle).repo;
}

auto LibGit2Backend::get_index(IndexHandle& handle) -> git_index* {
    return static_cast<LibGit2IndexHandle&>(handle).index;
}

auto LibGit2Backend::to_oid(const domain::ObjectId& oid) -> git_oid {
    git_oid result;
    static_assert(sizeof(result.id) == domain::ObjectId::SIZE);
    std::memcpy(result.id, oid.bytes.data(), domain::ObjectId::SIZE);
    return result;
}

auto LibGit2Backend::from_oid(const git_oid& oid) -> domain::ObjectId {
    domain::ObjectId result;
    std::memcpy(result.bytes.data(), oid.id, domain::ObjectId::SIZE);
    return result;
}

auto LibGit2Backend::to_signature(const domain::Signature& sig) -> git_signature* {
    git_signature* result = nullptr;
    auto time = std::chrono::system_clock::to_time_t(sig.when);
    auto offset = static_cast<int>(sig.tz_offset.count());

    git_signature_new(&result, sig.name.c_str(), sig.email.c_str(), time, offset);
    return result;
}

auto LibGit2Backend::from_signature(const git_signature* sig) -> domain::Signature {
    return domain::Signature{.name = sig->name,
                             .email = sig->email,
                             .when = std::chrono::system_clock::from_time_t(sig->when.time),
                             .tz_offset = std::chrono::minutes(sig->when.offset)};
}

auto LibGit2Backend::make_libgit2_error(int error_code, const std::string& context) -> Error {
    const git_error* err = git_error_last();
    std::string message = context;
    std::string detail;

    if (err && err->message) {
        detail = err->message;
    }

    // Map libgit2 error codes to our error codes
    Error::Code code = Error::Code::Unknown;
    switch (error_code) {
        case GIT_ENOTFOUND:
            code = Error::Code::ObjectNotFound;
            break;
        case GIT_EEXISTS:
            code = Error::Code::AlreadyExists;
            break;
        case GIT_EBAREREPO:
            code = Error::Code::BareRepository;
            break;
        case GIT_EUNBORNBRANCH:
        case GIT_EINVALIDSPEC:
            code = Error::Code::InvalidReference;
            break;
        case GIT_ECONFLICT:
            code = Error::Code::MergeConflict;
            break;
        default:
            code = Error::Code::Unknown;
    }

    return make_error(code, message, detail);
}

// Repository operations

auto LibGit2Backend::open(const std::filesystem::path& path)
    -> Result<std::unique_ptr<RepoHandle>> {
    git_repository* repo = nullptr;
    int error = git_repository_open(&repo, path.string().c_str());

    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to open repository"));
    }

    return std::make_unique<LibGit2RepoHandle>(repo);
}

auto LibGit2Backend::init(const std::filesystem::path& path, bool bare)
    -> Result<std::unique_ptr<RepoHandle>> {
    git_repository* repo = nullptr;
    int error = git_repository_init(&repo, path.string().c_str(), bare ? 1 : 0);

    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to initialize repository"));
    }

    return std::make_unique<LibGit2RepoHandle>(repo);
}

auto LibGit2Backend::is_bare(const RepoHandle& repo) -> bool {
    return git_repository_is_bare(get_repo(repo)) != 0;
}

auto LibGit2Backend::workdir(const RepoHandle& repo) -> std::filesystem::path {
    const char* path = git_repository_workdir(get_repo(repo));
    return path ? std::filesystem::path(path) : std::filesystem::path();
}

auto LibGit2Backend::git_dir(const RepoHandle& repo) -> std::filesystem::path {
    const char* path = git_repository_path(get_repo(repo));
    return path ? std::filesystem::path(path) : std::filesystem::path();
}

// Index operations

auto LibGit2Backend::get_index(RepoHandle& repo) -> Result<std::unique_ptr<IndexHandle>> {
    git_index* index = nullptr;
    int error = git_repository_index(&index, get_repo(repo));

    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to get index"));
    }

    return std::make_unique<LibGit2IndexHandle>(index);
}

auto LibGit2Backend::stage_file(IndexHandle& index, const std::filesystem::path& path) -> Status {
    git_index* idx = get_index(index);

    // Get the repository to find the working directory
    git_repository* repo = git_index_owner(idx);
    const char* workdir = git_repository_workdir(repo);

    // Build full path to the file
    std::filesystem::path full_path = std::filesystem::path(workdir) / path;

    // Check if file exists in the index
    const git_index_entry* entry = git_index_get_bypath(idx, path.string().c_str(), 0);

    // Check if file exists on disk
    std::error_code ec;
    bool file_exists = std::filesystem::exists(full_path, ec);

    int error;
    if (!file_exists && entry) {
        // File existed in index but is now deleted - remove from index
        error = git_index_remove_bypath(idx, path.string().c_str());
        if (error < 0) {
            return std::unexpected(make_libgit2_error(error, "Failed to stage deleted file"));
        }
    } else {
        // File exists or is new - add to index
        error = git_index_add_bypath(idx, path.string().c_str());
        if (error < 0) {
            return std::unexpected(make_libgit2_error(error, "Failed to stage file"));
        }
    }

    return {};
}

auto LibGit2Backend::unstage_file(IndexHandle& index, const std::filesystem::path& path) -> Status {
    int error = git_index_remove_bypath(get_index(index), path.string().c_str());

    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to unstage file"));
    }

    return {};
}

auto LibGit2Backend::write_index(IndexHandle& index) -> Status {
    int error = git_index_write(get_index(index));

    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to write index"));
    }

    return {};
}

// Status operations

namespace {
// Convert libgit2 status flags to our FileStatus::State
auto status_to_state(unsigned int flags, bool is_index) -> domain::FileStatus::State {
    using State = domain::FileStatus::State;

    // Check for conflicts first
    if (flags & GIT_STATUS_CONFLICTED) {
        return State::Conflicted;
    }

    // Check for untracked/ignored (these apply to both index and worktree)
    if (flags & GIT_STATUS_WT_NEW)
        return State::Untracked;
    if (flags & GIT_STATUS_IGNORED)
        return State::Ignored;

    if (is_index) {
        // Index status (staged changes)
        if (flags & GIT_STATUS_INDEX_NEW)
            return State::Added;
        if (flags & GIT_STATUS_INDEX_MODIFIED)
            return State::Modified;
        if (flags & GIT_STATUS_INDEX_DELETED)
            return State::Deleted;
        if (flags & GIT_STATUS_INDEX_RENAMED)
            return State::Renamed;
        if (flags & GIT_STATUS_INDEX_TYPECHANGE)
            return State::TypeChanged;
    } else {
        // Worktree status (unstaged changes)
        if (flags & GIT_STATUS_WT_MODIFIED)
            return State::Modified;
        if (flags & GIT_STATUS_WT_DELETED)
            return State::Deleted;
        if (flags & GIT_STATUS_WT_RENAMED)
            return State::Renamed;
        if (flags & GIT_STATUS_WT_TYPECHANGE)
            return State::TypeChanged;
    }

    return State::Unmodified;
}
} // namespace

auto LibGit2Backend::get_status(const RepoHandle& handle)
    -> Result<std::vector<domain::FileStatus>> {
    git_repository* repo = get_repo(handle);

    git_status_options opts{};
    git_status_options_init(&opts, GIT_STATUS_OPTIONS_VERSION);
    opts.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
    opts.flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED | GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX |
                 GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR;

    git_status_list* status_list = nullptr;
    int error = git_status_list_new(&status_list, repo, &opts);

    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to get repository status"));
    }

    // RAII wrapper for status list
    struct StatusListDeleter {
        void operator()(git_status_list* list) const {
            if (list)
                git_status_list_free(list);
        }
    };
    std::unique_ptr<git_status_list, StatusListDeleter> status_guard(status_list);

    std::vector<domain::FileStatus> results;
    size_t count = git_status_list_entrycount(status_list);
    results.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        const git_status_entry* entry = git_status_byindex(status_list, i);
        if (!entry)
            continue;

        domain::FileStatus file_status;

        // Determine the path
        const char* path = nullptr;

        // Try workdir path first (for untracked, modified, deleted files)
        if (entry->index_to_workdir) {
            if (entry->index_to_workdir->new_file.path) {
                path = entry->index_to_workdir->new_file.path;
            } else if (entry->index_to_workdir->old_file.path) {
                path = entry->index_to_workdir->old_file.path;
            }
        }

        // Fall back to index path (for staged files)
        if (!path && entry->head_to_index) {
            if (entry->head_to_index->new_file.path) {
                path = entry->head_to_index->new_file.path;
            } else if (entry->head_to_index->old_file.path) {
                path = entry->head_to_index->old_file.path;
            }
        }

        if (!path) {
            continue; // Skip entries without a path
        }

        file_status.path = path;

        // Set index and worktree status
        file_status.index_status = status_to_state(entry->status, true);
        file_status.worktree_status = status_to_state(entry->status, false);

        // Handle renames
        if (entry->status & (GIT_STATUS_INDEX_RENAMED | GIT_STATUS_WT_RENAMED)) {
            const char* old_path = nullptr;
            if (entry->head_to_index && entry->head_to_index->old_file.path) {
                old_path = entry->head_to_index->old_file.path;
            } else if (entry->index_to_workdir && entry->index_to_workdir->old_file.path) {
                old_path = entry->index_to_workdir->old_file.path;
            }

            if (old_path) {
                file_status.old_path = old_path;
                // Similarity is not directly available in git_status_entry
                // Would need to compute from the diff, so leaving it empty for now
            }
        }

        results.push_back(std::move(file_status));
    }

    return results;
}

auto LibGit2Backend::create_commit(RepoHandle& handle, const std::string& message,
                                   const domain::Signature& author,
                                   const domain::Signature& committer) -> Result<domain::ObjectId> {
    git_repository* repo = get_repo(handle);

    // Get the index
    git_index* index = nullptr;
    int error = git_repository_index(&index, repo);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to get repository index"));
    }

    // RAII wrapper for index
    struct IndexDeleter {
        void operator()(git_index* idx) const {
            if (idx)
                git_index_free(idx);
        }
    };
    std::unique_ptr<git_index, IndexDeleter> index_guard(index);

    // Write the index as a tree
    git_oid tree_oid;
    error = git_index_write_tree(&tree_oid, index);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to write tree from index"));
    }

    // Get the tree object
    git_tree* tree = nullptr;
    error = git_tree_lookup(&tree, repo, &tree_oid);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to lookup tree"));
    }

    // RAII wrapper for tree
    struct TreeDeleter {
        void operator()(git_tree* t) const {
            if (t)
                git_tree_free(t);
        }
    };
    std::unique_ptr<git_tree, TreeDeleter> tree_guard(tree);

    // Convert signatures
    git_signature* author_sig = to_signature(author);
    git_signature* committer_sig = to_signature(committer);

    // RAII wrappers for signatures
    struct SigDeleter {
        void operator()(git_signature* sig) const {
            if (sig)
                git_signature_free(sig);
        }
    };
    std::unique_ptr<git_signature, SigDeleter> author_guard(author_sig);
    std::unique_ptr<git_signature, SigDeleter> committer_guard(committer_sig);

    // Get the HEAD commit as parent (if it exists)
    git_oid parent_oid;
    git_commit* parent = nullptr;
    const git_commit* parents[1] = {nullptr};
    size_t parent_count = 0;

    error = git_reference_name_to_id(&parent_oid, repo, "HEAD");
    if (error == 0) {
        // HEAD exists, get the commit
        error = git_commit_lookup(&parent, repo, &parent_oid);
        if (error == 0) {
            parents[0] = parent;
            parent_count = 1;
        }
    }
    // If HEAD doesn't exist, this is the first commit (parent_count = 0)

    // RAII wrapper for parent
    struct CommitDeleter {
        void operator()(git_commit* c) const {
            if (c)
                git_commit_free(c);
        }
    };
    std::unique_ptr<git_commit, CommitDeleter> parent_guard(parent);

    // Create the commit
    git_oid commit_oid;
    error = git_commit_create(&commit_oid, repo,
                              "HEAD", // Update HEAD
                              author_sig, committer_sig,
                              "UTF-8", // Message encoding
                              message.c_str(), tree, parent_count, parents);

    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to create commit"));
    }

    return from_oid(commit_oid);
}

auto LibGit2Backend::amend_commit(RepoHandle& handle, const std::optional<std::string>& message,
                                  const std::optional<domain::Signature>& author,
                                  const std::optional<domain::Signature>& committer)
    -> Result<domain::ObjectId> {
    git_repository* repo = get_repo(handle);

    // Get the HEAD commit to amend
    git_oid head_oid;
    int error = git_reference_name_to_id(&head_oid, repo, "HEAD");
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to get HEAD for amend"));
    }

    git_commit* head_commit = nullptr;
    error = git_commit_lookup(&head_commit, repo, &head_oid);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to lookup HEAD commit"));
    }

    struct CommitDeleter {
        void operator()(git_commit* c) const {
            if (c)
                git_commit_free(c);
        }
    };
    std::unique_ptr<git_commit, CommitDeleter> head_guard(head_commit);

    // Get the current index and write it as a tree (if there are staged changes)
    git_index* index = nullptr;
    error = git_repository_index(&index, repo);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to get repository index"));
    }

    struct IndexDeleter {
        void operator()(git_index* idx) const {
            if (idx)
                git_index_free(idx);
        }
    };
    std::unique_ptr<git_index, IndexDeleter> index_guard(index);

    // Write the index as a tree
    git_oid tree_oid;
    error = git_index_write_tree(&tree_oid, index);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to write tree from index"));
    }

    git_tree* tree = nullptr;
    error = git_tree_lookup(&tree, repo, &tree_oid);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to lookup tree"));
    }

    struct TreeDeleter {
        void operator()(git_tree* t) const {
            if (t)
                git_tree_free(t);
        }
    };
    std::unique_ptr<git_tree, TreeDeleter> tree_guard(tree);

    // Prepare signatures
    git_signature* author_sig = nullptr;
    git_signature* committer_sig = nullptr;

    if (author.has_value()) {
        author_sig = to_signature(*author);
    } else {
        // Use original author
        author_sig = const_cast<git_signature*>(git_commit_author(head_commit));
    }

    if (committer.has_value()) {
        committer_sig = to_signature(*committer);
    }
    // If no committer specified, git_commit_amend will use default

    struct SigDeleter {
        void operator()(git_signature* sig) const {
            if (sig)
                git_signature_free(sig);
        }
    };
    std::unique_ptr<git_signature, SigDeleter> author_guard(author.has_value() ? author_sig
                                                                               : nullptr);
    std::unique_ptr<git_signature, SigDeleter> committer_guard(committer.has_value() ? committer_sig
                                                                                     : nullptr);

    // Prepare message
    const char* msg_ptr = message.has_value() ? message->c_str() : nullptr;

    // Amend the commit
    git_oid new_commit_oid;
    error = git_commit_amend(&new_commit_oid, head_commit,
                             "HEAD",        // Update HEAD reference
                             author_sig,    // Use provided or original author
                             committer_sig, // Use provided or default committer
                             "UTF-8",
                             msg_ptr, // Use provided or keep original message
                             tree     // Use the new tree from staged changes
    );

    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to amend commit"));
    }

    return from_oid(new_commit_oid);
}

auto LibGit2Backend::get_commit(const RepoHandle& handle, const domain::ObjectId& oid)
    -> Result<domain::Commit> {
    git_repository* repo = get_repo(handle);

    // Lookup the commit
    git_oid commit_oid = to_oid(oid);
    git_commit* commit = nullptr;
    int error = git_commit_lookup(&commit, repo, &commit_oid);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to lookup commit"));
    }

    // RAII wrapper for commit
    struct CommitDeleter {
        void operator()(git_commit* c) const {
            if (c)
                git_commit_free(c);
        }
    };
    std::unique_ptr<git_commit, CommitDeleter> commit_guard(commit);

    // Build commit object
    domain::Commit result;
    result.id = oid;

    // Get tree ID
    const git_oid* tree_oid = git_commit_tree_id(commit);
    if (tree_oid) {
        result.tree_id = from_oid(*tree_oid);
    }

    // Get parent IDs
    unsigned int parent_count = git_commit_parentcount(commit);
    result.parent_ids.reserve(parent_count);
    for (unsigned int i = 0; i < parent_count; ++i) {
        const git_oid* parent_oid = git_commit_parent_id(commit, i);
        if (parent_oid) {
            result.parent_ids.push_back(from_oid(*parent_oid));
        }
    }

    // Get author
    const git_signature* author = git_commit_author(commit);
    if (author) {
        result.author = from_signature(author);
    }

    // Get committer
    const git_signature* committer = git_commit_committer(commit);
    if (committer) {
        result.committer = from_signature(committer);
    }

    // Get message
    const char* message = git_commit_message(commit);
    if (message) {
        result.message = message;
    }

    return result;
}

auto LibGit2Backend::list_branches(const RepoHandle& handle, bool include_remote)
    -> Result<std::vector<domain::Branch>> {
    git_repository* repo = get_repo(handle);

    git_branch_iterator* iter = nullptr;
    git_branch_t list_flags = include_remote ? GIT_BRANCH_ALL : GIT_BRANCH_LOCAL;

    int error = git_branch_iterator_new(&iter, repo, list_flags);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to create branch iterator"));
    }

    // RAII wrapper for iterator
    struct IterDeleter {
        void operator()(git_branch_iterator* it) const {
            if (it)
                git_branch_iterator_free(it);
        }
    };
    std::unique_ptr<git_branch_iterator, IterDeleter> iter_guard(iter);

    std::vector<domain::Branch> branches;

    git_reference* ref = nullptr;
    git_branch_t branch_type;

    while ((error = git_branch_next(&ref, &branch_type, iter)) == 0) {
        // RAII wrapper for reference
        struct RefDeleter {
            void operator()(git_reference* r) const {
                if (r)
                    git_reference_free(r);
            }
        };
        std::unique_ptr<git_reference, RefDeleter> ref_guard(ref);

        domain::Branch branch;

        // Get branch name
        const char* branch_name = nullptr;
        error = git_branch_name(&branch_name, ref);
        if (error < 0) {
            continue; // Skip this branch
        }
        branch.name = branch_name;
        branch.full_name = git_reference_name(ref);
        branch.is_remote = (branch_type == GIT_BRANCH_REMOTE);

        // Get target OID
        const git_oid* oid = git_reference_target(ref);
        if (oid) {
            branch.target = from_oid(*oid);
        }

        // Check if this is HEAD
        branch.is_head = git_branch_is_head(ref) == 1;

        // Get upstream if it's a local branch
        if (!branch.is_remote) {
            git_reference* upstream = nullptr;
            error = git_branch_upstream(&upstream, ref);
            if (error == 0 && upstream) {
                const char* upstream_name = nullptr;
                git_branch_name(&upstream_name, upstream);
                if (upstream_name) {
                    branch.upstream = upstream_name;
                }

                // Get tracking info (ahead/behind counts)
                size_t ahead = 0, behind = 0;
                error = git_graph_ahead_behind(&ahead, &behind, repo, git_reference_target(ref),
                                               git_reference_target(upstream));
                if (error == 0) {
                    branch.tracking =
                        domain::Branch::TrackingInfo{.ahead = ahead, .behind = behind};
                }

                git_reference_free(upstream);
            }
        }

        branches.push_back(std::move(branch));
    }

    // GIT_ITEROVER is the normal end condition
    if (error != GIT_ITEROVER && error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to iterate branches"));
    }

    return branches;
}

auto LibGit2Backend::create_branch(RepoHandle& handle, const std::string& name,
                                   const domain::ObjectId& target, bool force)
    -> Result<domain::Branch> {
    git_repository* repo = get_repo(handle);

    // Get the target commit
    git_oid oid = to_oid(target);
    git_commit* commit = nullptr;
    int error = git_commit_lookup(&commit, repo, &oid);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to lookup target commit"));
    }

    // RAII wrapper for commit
    struct CommitDeleter {
        void operator()(git_commit* c) const {
            if (c)
                git_commit_free(c);
        }
    };
    std::unique_ptr<git_commit, CommitDeleter> commit_guard(commit);

    // Create the branch
    git_reference* ref = nullptr;
    error = git_branch_create(&ref, repo, name.c_str(), commit, force ? 1 : 0);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to create branch"));
    }

    // RAII wrapper for reference
    struct RefDeleter {
        void operator()(git_reference* r) const {
            if (r)
                git_reference_free(r);
        }
    };
    std::unique_ptr<git_reference, RefDeleter> ref_guard(ref);

    // Build branch object
    domain::Branch branch;
    branch.name = name;
    branch.full_name = git_reference_name(ref);
    branch.is_remote = false;
    branch.is_head = false;

    const git_oid* branch_oid = git_reference_target(ref);
    if (branch_oid) {
        branch.target = from_oid(*branch_oid);
    }

    return branch;
}

auto LibGit2Backend::delete_branch(RepoHandle& handle, const std::string& name) -> Status {
    git_repository* repo = get_repo(handle);

    // Lookup the branch
    git_reference* ref = nullptr;
    int error = git_branch_lookup(&ref, repo, name.c_str(), GIT_BRANCH_LOCAL);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to lookup branch"));
    }

    // RAII wrapper for reference
    struct RefDeleter {
        void operator()(git_reference* r) const {
            if (r)
                git_reference_free(r);
        }
    };
    std::unique_ptr<git_reference, RefDeleter> ref_guard(ref);

    // Delete the branch
    error = git_branch_delete(ref);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to delete branch"));
    }

    return {};
}

auto LibGit2Backend::rename_branch(RepoHandle& handle, const std::string& old_name,
                                   const std::string& new_name, bool force)
    -> Result<domain::Branch> {
    git_repository* repo = get_repo(handle);

    // Lookup the branch to rename
    git_reference* branch_ref = nullptr;
    int error = git_branch_lookup(&branch_ref, repo, old_name.c_str(), GIT_BRANCH_LOCAL);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to lookup branch: " + old_name));
    }

    struct RefDeleter {
        void operator()(git_reference* r) const {
            if (r)
                git_reference_free(r);
        }
    };
    std::unique_ptr<git_reference, RefDeleter> branch_guard(branch_ref);

    // Rename the branch
    git_reference* new_ref = nullptr;
    error = git_branch_move(&new_ref, branch_ref, new_name.c_str(), force ? 1 : 0);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to rename branch"));
    }

    // RAII wrapper for the new reference
    std::unique_ptr<git_reference, RefDeleter> new_ref_guard(new_ref);

    // Get the target OID
    const git_oid* target_oid = git_reference_target(new_ref);
    if (!target_oid) {
        Error err;
        err.code = Error::Code::ReferenceNotFound;
        err.message = "Branch reference has no target";
        return std::unexpected(std::move(err));
    }

    // Check if this is the current branch
    int is_head = git_branch_is_head(new_ref);

    // Get the full name
    const char* full_name_ptr = git_reference_name(new_ref);

    // Build the domain::Branch object
    domain::Branch result;
    result.name = new_name;
    result.full_name = full_name_ptr ? full_name_ptr : ("refs/heads/" + new_name);
    result.target = from_oid(*target_oid);
    result.is_remote = false;
    result.is_head = (is_head == 1);

    return result;
}

auto LibGit2Backend::switch_branch(RepoHandle& handle, const std::string& branch_name) -> Status {
    git_repository* repo = get_repo(handle);

    // Lookup the branch
    git_reference* branch_ref = nullptr;
    int error = git_branch_lookup(&branch_ref, repo, branch_name.c_str(), GIT_BRANCH_LOCAL);

    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to lookup branch"));
    }

    struct RefDeleter {
        void operator()(git_reference* r) const {
            if (r)
                git_reference_free(r);
        }
    };
    std::unique_ptr<git_reference, RefDeleter> ref_guard(branch_ref);

    // Get the commit that the branch points to
    const git_oid* target_oid = git_reference_target(branch_ref);
    if (!target_oid) {
        Error err;
        err.code = Error::Code::InvalidReference;
        err.message = "Branch reference does not point to a commit";
        return std::unexpected(std::move(err));
    }

    git_object* target_obj = nullptr;
    error = git_object_lookup(&target_obj, repo, target_oid, GIT_OBJECT_COMMIT);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to lookup target commit"));
    }

    struct ObjectDeleter {
        void operator()(git_object* obj) const {
            if (obj)
                git_object_free(obj);
        }
    };
    std::unique_ptr<git_object, ObjectDeleter> obj_guard(target_obj);

    // Checkout options
    git_checkout_options opts{};
    git_checkout_options_init(&opts, GIT_CHECKOUT_OPTIONS_VERSION);
    opts.checkout_strategy = GIT_CHECKOUT_SAFE;

    // Checkout the tree
    error = git_checkout_tree(repo, target_obj, &opts);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to checkout branch"));
    }

    // Set HEAD to the branch
    std::string ref_name = "refs/heads/" + branch_name;
    error = git_repository_set_head(repo, ref_name.c_str());
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to set HEAD"));
    }

    return {};
}

auto LibGit2Backend::restore_files(RepoHandle& handle,
                                   const std::vector<std::filesystem::path>& paths, bool staged)
    -> Status {
    git_repository* repo = get_repo(handle);

    git_checkout_options opts{};
    git_checkout_options_init(&opts, GIT_CHECKOUT_OPTIONS_VERSION);
    opts.checkout_strategy = GIT_CHECKOUT_FORCE;

    // Convert paths to git_strarray - keep strings alive
    std::vector<std::string> path_strings;
    path_strings.reserve(paths.size());
    for (const auto& path : paths) {
        path_strings.push_back(path.string());
    }

    std::vector<char*> path_ptrs;
    path_ptrs.reserve(path_strings.size());
    for (auto& str : path_strings) {
        path_ptrs.push_back(const_cast<char*>(str.c_str()));
    }

    git_strarray paths_array;
    paths_array.strings = path_ptrs.data();
    paths_array.count = path_ptrs.size();
    opts.paths = paths_array;

    int error;

    if (staged) {
        // Restore staged files from HEAD
        git_object* head_obj = nullptr;
        error = git_revparse_single(&head_obj, repo, "HEAD");
        if (error < 0) {
            return std::unexpected(make_libgit2_error(error, "Failed to resolve HEAD"));
        }

        struct ObjectDeleter {
            void operator()(git_object* obj) const {
                if (obj)
                    git_object_free(obj);
            }
        };
        std::unique_ptr<git_object, ObjectDeleter> obj_guard(head_obj);

        error = git_checkout_tree(repo, head_obj, &opts);
        if (error < 0) {
            return std::unexpected(make_libgit2_error(error, "Failed to checkout from HEAD"));
        }

        // Also update the index to match HEAD for these files
        git_index* index = nullptr;
        error = git_repository_index(&index, repo);
        if (error < 0) {
            return std::unexpected(make_libgit2_error(error, "Failed to get index"));
        }

        struct IndexDeleter {
            void operator()(git_index* idx) const {
                if (idx)
                    git_index_free(idx);
            }
        };
        std::unique_ptr<git_index, IndexDeleter> index_guard(index);

        // Remove staged changes by resetting index entries to HEAD
        git_tree* head_tree = nullptr;
        git_commit* head_commit = nullptr;
        error = git_commit_lookup(&head_commit, repo, git_object_id(head_obj));
        if (error == 0) {
            error = git_commit_tree(&head_tree, head_commit);
            if (error == 0) {
                for (const auto& path : paths) {
                    git_tree_entry* entry = nullptr;
                    error = git_tree_entry_bypath(&entry, head_tree, path.string().c_str());
                    if (error == 0) {
                        const git_oid* oid = git_tree_entry_id(entry);
                        git_index_entry index_entry{};
                        index_entry.mode = git_tree_entry_filemode(entry);
                        index_entry.path = path.string().c_str();
                        git_oid_cpy(&index_entry.id, oid);
                        git_index_add(index, &index_entry);
                        git_tree_entry_free(entry);
                    }
                }
                git_tree_free(head_tree);
            }
            git_commit_free(head_commit);
        }

        error = git_index_write(index);
        if (error < 0) {
            return std::unexpected(make_libgit2_error(error, "Failed to write index"));
        }
    } else {
        // Restore working tree from index
        git_index* index = nullptr;
        error = git_repository_index(&index, repo);
        if (error < 0) {
            return std::unexpected(make_libgit2_error(error, "Failed to get index"));
        }

        struct IndexDeleter {
            void operator()(git_index* idx) const {
                if (idx)
                    git_index_free(idx);
            }
        };
        std::unique_ptr<git_index, IndexDeleter> index_guard(index);

        error = git_checkout_index(repo, index, &opts);
        if (error < 0) {
            return std::unexpected(make_libgit2_error(error, "Failed to restore files from index"));
        }
    }

    return {};
}

auto LibGit2Backend::get_head(const RepoHandle& handle) -> Result<domain::Reference> {
    git_repository* repo = get_repo(handle);

    git_reference* ref = nullptr;
    int error = git_repository_head(&ref, repo);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to get HEAD"));
    }

    // RAII wrapper for reference
    struct RefDeleter {
        void operator()(git_reference* r) const {
            if (r)
                git_reference_free(r);
        }
    };
    std::unique_ptr<git_reference, RefDeleter> ref_guard(ref);

    domain::Reference result;
    result.name = git_reference_name(ref);

    // Check if it's symbolic or direct
    git_reference_t ref_type = git_reference_type(ref);
    if (ref_type == GIT_REFERENCE_SYMBOLIC) {
        result.type = domain::Reference::Type::Symbolic;
        const char* target = git_reference_symbolic_target(ref);
        if (target) {
            result.target = std::string(target);
        }
    } else {
        result.type = domain::Reference::Type::Direct;
        const git_oid* oid = git_reference_target(ref);
        if (oid) {
            result.target = from_oid(*oid);
        }
    }

    return result;
}

auto LibGit2Backend::resolve_reference(const RepoHandle& handle, const std::string& name)
    -> Result<domain::Reference> {
    git_repository* repo = get_repo(handle);

    git_reference* ref = nullptr;
    int error = git_reference_lookup(&ref, repo, name.c_str());
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to lookup reference"));
    }

    // RAII wrapper for reference
    struct RefDeleter {
        void operator()(git_reference* r) const {
            if (r)
                git_reference_free(r);
        }
    };
    std::unique_ptr<git_reference, RefDeleter> ref_guard(ref);

    domain::Reference result;
    result.name = git_reference_name(ref);

    // Check if it's symbolic or direct
    git_reference_t ref_type = git_reference_type(ref);
    if (ref_type == GIT_REFERENCE_SYMBOLIC) {
        result.type = domain::Reference::Type::Symbolic;
        const char* target = git_reference_symbolic_target(ref);
        if (target) {
            result.target = std::string(target);
        }
    } else {
        result.type = domain::Reference::Type::Direct;
        const git_oid* oid = git_reference_target(ref);
        if (oid) {
            result.target = from_oid(*oid);
        }
    }

    return result;
}

// Diff conversion helper
namespace {
auto convert_diff_status(git_delta_t status) -> domain::FileDiff::Status {
    using Status = domain::FileDiff::Status;
    switch (status) {
        case GIT_DELTA_ADDED:
            return Status::Added;
        case GIT_DELTA_DELETED:
            return Status::Deleted;
        case GIT_DELTA_MODIFIED:
            return Status::Modified;
        case GIT_DELTA_RENAMED:
            return Status::Renamed;
        case GIT_DELTA_COPIED:
            return Status::Copied;
        case GIT_DELTA_TYPECHANGE:
            return Status::TypeChanged;
        default:
            return Status::Modified;
    }
}

auto convert_diff_line_origin(char origin) -> domain::DiffLine::Origin {
    using Origin = domain::DiffLine::Origin;
    switch (origin) {
        case GIT_DIFF_LINE_ADDITION:
            return Origin::Addition;
        case GIT_DIFF_LINE_DELETION:
            return Origin::Deletion;
        default:
            return Origin::Context;
    }
}
} // namespace

auto LibGit2Backend::convert_diff_to_domain(git_diff* diff)
    -> Result<std::vector<domain::FileDiff>> {
    std::vector<domain::FileDiff> results;

    size_t delta_count = git_diff_num_deltas(diff);
    results.reserve(delta_count);

    for (size_t i = 0; i < delta_count; ++i) {
        const git_diff_delta* delta = git_diff_get_delta(diff, i);
        if (!delta)
            continue;

        domain::FileDiff file_diff;
        file_diff.path = delta->new_file.path ? delta->new_file.path : "";

        if (delta->old_file.path && delta->new_file.path &&
            std::string(delta->old_file.path) != std::string(delta->new_file.path)) {
            file_diff.old_path = delta->old_file.path;
        }

        file_diff.status = convert_diff_status(delta->status);
        file_diff.is_binary = (delta->flags & GIT_DIFF_FLAG_BINARY) != 0;
        file_diff.additions = 0;
        file_diff.deletions = 0;

        // Get patch for this file to extract hunks
        git_patch* patch = nullptr;
        int error = git_patch_from_diff(&patch, diff, i);

        if (error == 0 && patch) {
            struct PatchDeleter {
                void operator()(git_patch* p) const {
                    if (p)
                        git_patch_free(p);
                }
            };
            std::unique_ptr<git_patch, PatchDeleter> patch_guard(patch);

            size_t hunk_count = git_patch_num_hunks(patch);
            file_diff.hunks.reserve(hunk_count);

            for (size_t h = 0; h < hunk_count; ++h) {
                const git_diff_hunk* hunk = nullptr;
                error = git_patch_get_hunk(&hunk, nullptr, patch, h);

                if (error == 0 && hunk) {
                    domain::DiffHunk diff_hunk;
                    diff_hunk.old_start = hunk->old_start;
                    diff_hunk.old_lines = hunk->old_lines;
                    diff_hunk.new_start = hunk->new_start;
                    diff_hunk.new_lines = hunk->new_lines;
                    diff_hunk.header = std::string(hunk->header, hunk->header_len);

                    // Get lines for this hunk
                    size_t line_count = git_patch_num_lines_in_hunk(patch, h);
                    diff_hunk.lines.reserve(line_count);

                    for (size_t l = 0; l < line_count; ++l) {
                        const git_diff_line* line = nullptr;
                        error = git_patch_get_line_in_hunk(&line, patch, h, l);

                        if (error == 0 && line) {
                            domain::DiffLine diff_line;
                            diff_line.origin = convert_diff_line_origin(line->origin);
                            diff_line.content = std::string(line->content, line->content_len);

                            if (line->old_lineno != -1) {
                                diff_line.old_lineno = line->old_lineno;
                            }
                            if (line->new_lineno != -1) {
                                diff_line.new_lineno = line->new_lineno;
                            }

                            // Count additions/deletions
                            if (diff_line.origin == domain::DiffLine::Origin::Addition) {
                                file_diff.additions++;
                            } else if (diff_line.origin == domain::DiffLine::Origin::Deletion) {
                                file_diff.deletions++;
                            }

                            diff_hunk.lines.push_back(std::move(diff_line));
                        }
                    }

                    file_diff.hunks.push_back(std::move(diff_hunk));
                }
            }
        }

        results.push_back(std::move(file_diff));
    }

    return results;
}

auto LibGit2Backend::diff_index_to_workdir(const RepoHandle& handle)
    -> Result<std::vector<domain::FileDiff>> {
    git_repository* repo = get_repo(handle);

    // Get the index
    git_index* index = nullptr;
    int error = git_repository_index(&index, repo);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to get index"));
    }

    struct IndexDeleter {
        void operator()(git_index* idx) const {
            if (idx)
                git_index_free(idx);
        }
    };
    std::unique_ptr<git_index, IndexDeleter> index_guard(index);

    // Create diff between index and workdir
    git_diff* diff = nullptr;
    error = git_diff_index_to_workdir(&diff, repo, index, nullptr);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to create diff"));
    }

    struct DiffDeleter {
        void operator()(git_diff* d) const {
            if (d)
                git_diff_free(d);
        }
    };
    std::unique_ptr<git_diff, DiffDeleter> diff_guard(diff);

    return convert_diff_to_domain(diff);
}

auto LibGit2Backend::diff_tree_to_index(const RepoHandle& handle)
    -> Result<std::vector<domain::FileDiff>> {
    git_repository* repo = get_repo(handle);

    // Get HEAD commit
    git_object* head_obj = nullptr;
    int error = git_revparse_single(&head_obj, repo, "HEAD");
    if (error < 0) {
        // No HEAD (empty repo), return empty diff
        return std::vector<domain::FileDiff>{};
    }

    struct ObjectDeleter {
        void operator()(git_object* obj) const {
            if (obj)
                git_object_free(obj);
        }
    };
    std::unique_ptr<git_object, ObjectDeleter> head_guard(head_obj);

    git_commit* commit = nullptr;
    error = git_commit_lookup(&commit, repo, git_object_id(head_obj));
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to lookup HEAD commit"));
    }

    struct CommitDeleter {
        void operator()(git_commit* c) const {
            if (c)
                git_commit_free(c);
        }
    };
    std::unique_ptr<git_commit, CommitDeleter> commit_guard(commit);

    // Get the tree from the commit
    git_tree* tree = nullptr;
    error = git_commit_tree(&tree, commit);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to get tree from commit"));
    }

    struct TreeDeleter {
        void operator()(git_tree* t) const {
            if (t)
                git_tree_free(t);
        }
    };
    std::unique_ptr<git_tree, TreeDeleter> tree_guard(tree);

    // Get the index
    git_index* index = nullptr;
    error = git_repository_index(&index, repo);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to get index"));
    }

    struct IndexDeleter {
        void operator()(git_index* idx) const {
            if (idx)
                git_index_free(idx);
        }
    };
    std::unique_ptr<git_index, IndexDeleter> index_guard(index);

    // Create diff between tree (HEAD) and index (staged)
    git_diff* diff = nullptr;
    error = git_diff_tree_to_index(&diff, repo, tree, index, nullptr);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to create diff"));
    }

    struct DiffDeleter {
        void operator()(git_diff* d) const {
            if (d)
                git_diff_free(d);
        }
    };
    std::unique_ptr<git_diff, DiffDeleter> diff_guard(diff);

    return convert_diff_to_domain(diff);
}

auto LibGit2Backend::list_remotes(const RepoHandle& handle) -> Result<std::vector<domain::Remote>> {
    git_repository* repo = get_repo(handle);

    git_strarray remote_names{};
    int error = git_remote_list(&remote_names, repo);

    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to list remotes"));
    }

    struct StrArrayDeleter {
        void operator()(git_strarray* arr) const {
            if (arr)
                git_strarray_dispose(arr);
        }
    };
    std::unique_ptr<git_strarray, StrArrayDeleter> names_guard(&remote_names);

    std::vector<domain::Remote> results;
    results.reserve(remote_names.count);

    for (size_t i = 0; i < remote_names.count; ++i) {
        git_remote* remote = nullptr;
        error = git_remote_lookup(&remote, repo, remote_names.strings[i]);

        if (error == 0 && remote) {
            struct RemoteDeleter {
                void operator()(git_remote* r) const {
                    if (r)
                        git_remote_free(r);
                }
            };
            std::unique_ptr<git_remote, RemoteDeleter> remote_guard(remote);

            domain::Remote remote_info;
            remote_info.name = remote_names.strings[i];

            const char* url = git_remote_url(remote);
            if (url) {
                remote_info.url = url;
            }

            const char* push_url = git_remote_pushurl(remote);
            if (push_url) {
                remote_info.push_url = push_url;
            }

            results.push_back(std::move(remote_info));
        }
    }

    return results;
}

auto LibGit2Backend::add_remote(RepoHandle& handle, const std::string& name, const std::string& url)
    -> Status {
    git_repository* repo = get_repo(handle);

    git_remote* remote = nullptr;
    int error = git_remote_create(&remote, repo, name.c_str(), url.c_str());

    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to add remote"));
    }

    struct RemoteDeleter {
        void operator()(git_remote* r) const {
            if (r)
                git_remote_free(r);
        }
    };
    std::unique_ptr<git_remote, RemoteDeleter> remote_guard(remote);

    return {};
}

auto LibGit2Backend::remove_remote(RepoHandle& handle, const std::string& name) -> Status {
    git_repository* repo = get_repo(handle);

    int error = git_remote_delete(repo, name.c_str());

    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to remove remote"));
    }

    return {};
}

auto LibGit2Backend::fetch(RepoHandle& handle, const std::string& remote_name,
                           const std::string& refspec, bool prune, bool tags)
    -> Result<FetchStats> {
    git_repository* repo = get_repo(handle);

    // Lookup the remote
    git_remote* remote = nullptr;
    int error = git_remote_lookup(&remote, repo, remote_name.c_str());
    if (error < 0) {
        return std::unexpected(
            make_libgit2_error(error, "Failed to lookup remote: " + remote_name));
    }

    struct RemoteDeleter {
        void operator()(git_remote* r) const {
            if (r)
                git_remote_free(r);
        }
    };
    std::unique_ptr<git_remote, RemoteDeleter> remote_guard(remote);

    // Configure fetch options
    git_fetch_options fetch_opts;
    git_fetch_options_init(&fetch_opts, GIT_FETCH_OPTIONS_VERSION);

    // Set prune option
    if (prune) {
        fetch_opts.prune = GIT_FETCH_PRUNE;
    }

    // Set download tags option
    if (tags) {
        fetch_opts.download_tags = GIT_REMOTE_DOWNLOAD_TAGS_ALL;
    } else {
        fetch_opts.download_tags = GIT_REMOTE_DOWNLOAD_TAGS_NONE;
    }

    // Prepare refspecs
    const char* refspecs[] = {refspec.empty() ? nullptr : refspec.c_str()};
    git_strarray refspec_array;
    if (refspec.empty()) {
        refspec_array.strings = nullptr;
        refspec_array.count = 0;
    } else {
        refspec_array.strings = const_cast<char**>(refspecs);
        refspec_array.count = 1;
    }

    // Perform fetch
    error = git_remote_fetch(remote, refspec_array.count > 0 ? &refspec_array : nullptr,
                             &fetch_opts, nullptr);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Fetch failed"));
    }

    // Get fetch statistics
    const git_indexer_progress* stats = git_remote_stats(remote);

    FetchStats result;
    result.received_objects = stats->received_objects;
    result.indexed_objects = stats->indexed_objects;
    result.received_bytes = stats->received_bytes;

    // Get list of updated refs
    size_t reflog_count = git_remote_refspec_count(remote);
    for (size_t i = 0; i < reflog_count; i++) {
        const git_refspec* refspec_ptr = git_remote_get_refspec(remote, i);
        if (refspec_ptr) {
            const char* dst = git_refspec_dst(refspec_ptr);
            if (dst) {
                result.updated_refs.push_back(dst);
            }
        }
    }

    return result;
}

auto LibGit2Backend::push(RepoHandle& handle, const std::string& remote_name,
                          const std::string& refspec, bool force, bool set_upstream)
    -> Result<PushStats> {
    git_repository* repo = get_repo(handle);

    // Lookup the remote
    git_remote* remote = nullptr;
    int error = git_remote_lookup(&remote, repo, remote_name.c_str());
    if (error < 0) {
        return std::unexpected(
            make_libgit2_error(error, "Failed to lookup remote: " + remote_name));
    }

    struct RemoteDeleter {
        void operator()(git_remote* r) const {
            if (r)
                git_remote_free(r);
        }
    };
    std::unique_ptr<git_remote, RemoteDeleter> remote_guard(remote);

    // Configure push options
    git_push_options push_opts;
    git_push_options_init(&push_opts, GIT_PUSH_OPTIONS_VERSION);

    // Determine refspec to push
    std::string push_refspec;
    if (refspec.empty()) {
        // Get current HEAD to push current branch
        git_reference* head = nullptr;
        error = git_repository_head(&head, repo);
        if (error < 0) {
            return std::unexpected(make_libgit2_error(error, "Failed to get HEAD"));
        }

        struct RefDeleter {
            void operator()(git_reference* r) const {
                if (r)
                    git_reference_free(r);
            }
        };
        std::unique_ptr<git_reference, RefDeleter> head_guard(head);

        const char* branch_name = git_reference_shorthand(head);
        if (!branch_name) {
            Error err;
            err.code = Error::Code::DetachedHead;
            err.message = "Not on a branch (detached HEAD)";
            return std::unexpected(std::move(err));
        }

        // Build refspec: refs/heads/branch:refs/heads/branch
        push_refspec = std::string("refs/heads/") + branch_name + ":refs/heads/" + branch_name;

        // Set upstream if requested
        if (set_upstream) {
            // Create upstream tracking after successful push
            // This will be done after push succeeds
        }
    } else {
        push_refspec = refspec;
    }

    // Add force prefix if needed
    if (force) {
        push_refspec = "+" + push_refspec;
    }

    // Prepare refspecs array
    const char* refspecs[] = {push_refspec.c_str()};
    git_strarray refspec_array;
    refspec_array.strings = const_cast<char**>(refspecs);
    refspec_array.count = 1;

    // Perform push
    error = git_remote_push(remote, &refspec_array, &push_opts);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Push failed"));
    }

    // Create tracking branch if set_upstream was requested
    if (set_upstream && refspec.empty()) {
        git_reference* head = nullptr;
        error = git_repository_head(&head, repo);
        if (error == 0) {
            struct RefDeleter {
                void operator()(git_reference* r) const {
                    if (r)
                        git_reference_free(r);
                }
            };
            std::unique_ptr<git_reference, RefDeleter> head_guard(head);

            const char* branch_name = git_reference_shorthand(head);
            if (branch_name) {
                // Set upstream tracking
                git_branch_set_upstream(head, (remote_name + "/" + branch_name).c_str());
            }
        }
    }

    // Build result with statistics
    PushStats result;
    // Note: libgit2 doesn't provide detailed push statistics like it does for fetch
    // We track the updated refs from the refspec
    result.updated_refs.push_back(push_refspec);

    return result;
}

auto LibGit2Backend::list_tags(const RepoHandle& handle) -> Result<std::vector<domain::Tag>> {
    git_repository* repo = get_repo(handle);

    git_strarray tag_names{};
    int error = git_tag_list(&tag_names, repo);

    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to list tags"));
    }

    struct StrArrayDeleter {
        void operator()(git_strarray* arr) const {
            if (arr)
                git_strarray_dispose(arr);
        }
    };
    std::unique_ptr<git_strarray, StrArrayDeleter> names_guard(&tag_names);

    std::vector<domain::Tag> results;
    results.reserve(tag_names.count);

    for (size_t i = 0; i < tag_names.count; ++i) {
        domain::Tag tag;
        tag.name = tag_names.strings[i];

        // Try to get the tag reference
        std::string ref_name = "refs/tags/" + tag.name;
        git_reference* ref = nullptr;
        error = git_reference_lookup(&ref, repo, ref_name.c_str());

        if (error == 0 && ref) {
            struct RefDeleter {
                void operator()(git_reference* r) const {
                    if (r)
                        git_reference_free(r);
                }
            };
            std::unique_ptr<git_reference, RefDeleter> ref_guard(ref);

            // Get the target OID
            const git_oid* target_oid = git_reference_target(ref);
            if (target_oid) {
                tag.target = from_oid(*target_oid);

                // Check if it's an annotated tag
                git_tag* tag_obj = nullptr;
                error = git_tag_lookup(&tag_obj, repo, target_oid);

                if (error == 0 && tag_obj) {
                    struct TagDeleter {
                        void operator()(git_tag* t) const {
                            if (t)
                                git_tag_free(t);
                        }
                    };
                    std::unique_ptr<git_tag, TagDeleter> tag_guard(tag_obj);

                    tag.is_annotated = true;
                    const char* message = git_tag_message(tag_obj);
                    if (message) {
                        tag.message = message;
                    }

                    // Get the actual target (what the tag points to)
                    const git_oid* tag_target = git_tag_target_id(tag_obj);
                    if (tag_target) {
                        tag.target = from_oid(*tag_target);
                    }
                } else {
                    // Lightweight tag
                    tag.is_annotated = false;
                }
            }

            results.push_back(std::move(tag));
        }
    }

    return results;
}

auto LibGit2Backend::create_tag(RepoHandle& handle, const std::string& name,
                                const domain::ObjectId& target, const std::string& message,
                                const domain::Signature& tagger, bool force) -> Status {
    git_repository* repo = get_repo(handle);

    git_oid target_oid = to_oid(target);

    // Lookup the target object
    git_object* target_obj = nullptr;
    int error = git_object_lookup(&target_obj, repo, &target_oid, GIT_OBJECT_ANY);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to lookup target object"));
    }

    struct ObjectDeleter {
        void operator()(git_object* obj) const {
            if (obj)
                git_object_free(obj);
        }
    };
    std::unique_ptr<git_object, ObjectDeleter> obj_guard(target_obj);

    git_oid tag_oid;

    if (message.empty()) {
        // Create lightweight tag
        error = git_tag_create_lightweight(&tag_oid, repo, name.c_str(), target_obj, force ? 1 : 0);
        if (error < 0) {
            return std::unexpected(make_libgit2_error(error, "Failed to create lightweight tag"));
        }
    } else {
        // Create annotated tag
        git_signature* sig = to_signature(tagger);
        if (!sig) {
            Error err;
            err.code = Error::Code::InvalidArgument;
            err.message = "Failed to create signature";
            return std::unexpected(std::move(err));
        }

        struct SigDeleter {
            void operator()(git_signature* s) const {
                if (s)
                    git_signature_free(s);
            }
        };
        std::unique_ptr<git_signature, SigDeleter> sig_guard(sig);

        error = git_tag_create(&tag_oid, repo, name.c_str(), target_obj, sig, message.c_str(),
                               force ? 1 : 0);
        if (error < 0) {
            return std::unexpected(make_libgit2_error(error, "Failed to create annotated tag"));
        }
    }

    return {};
}

auto LibGit2Backend::delete_tag(RepoHandle& handle, const std::string& name) -> Status {
    git_repository* repo = get_repo(handle);

    int error = git_tag_delete(repo, name.c_str());

    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to delete tag"));
    }

    return {};
}

// ============================================================================
// Stash Operations
// ============================================================================

auto LibGit2Backend::list_stashes(const RepoHandle& handle) -> Result<std::vector<domain::Stash>> {

    git_repository* repo = get_repo(handle);
    std::vector<domain::Stash> results;

    // Callback to collect stash entries
    auto callback = [](size_t index, const char* message, const git_oid* stash_id,
                       void* payload) -> int {
        auto* stashes = static_cast<std::vector<domain::Stash>*>(payload);

        domain::Stash stash;
        stash.index = index;
        stash.commit_id = from_oid(*stash_id);
        stash.message = message ? message : "";

        // Note: libgit2 doesn't provide author/timestamp in stash_foreach
        // We'd need to lookup the commit to get that info
        // For now, leaving them empty/default

        stashes->push_back(std::move(stash));
        return 0; // Continue iteration
    };

    int error = git_stash_foreach(repo, callback, &results);

    if (error < 0 && error != GIT_ENOTFOUND) {
        return std::unexpected(make_libgit2_error(error, "Failed to list stashes"));
    }

    return results;
}

auto LibGit2Backend::create_stash(RepoHandle& handle, const std::string& message,
                                  const domain::Signature& stasher, bool include_untracked,
                                  bool keep_index) -> Result<domain::ObjectId> {

    git_repository* repo = get_repo(handle);

    // Convert signature
    git_signature* sig = to_signature(stasher);
    if (!sig) {
        Error err;
        err.code = Error::Code::InvalidArgument;
        err.message = "Failed to convert signature";
        return std::unexpected(std::move(err));
    }

    // RAII wrapper for signature
    struct SigDeleter {
        git_signature* sig;
        ~SigDeleter() {
            if (sig)
                git_signature_free(sig);
        }
    } sig_guard{sig};

    // Set stash flags
    uint32_t flags = GIT_STASH_DEFAULT;
    if (include_untracked) {
        flags |= GIT_STASH_INCLUDE_UNTRACKED;
    }
    if (keep_index) {
        flags |= GIT_STASH_KEEP_INDEX;
    }

    git_oid stash_oid;
    int error = git_stash_save(&stash_oid, repo, sig, message.c_str(), flags);

    if (error < 0) {
        if (error == GIT_ENOTFOUND) {
            Error err;
            err.code = Error::Code::NothingToCommit;
            err.message = "No local changes to stash";
            return std::unexpected(std::move(err));
        }
        return std::unexpected(make_libgit2_error(error, "Failed to create stash"));
    }

    return from_oid(stash_oid);
}

auto LibGit2Backend::apply_stash(RepoHandle& handle, size_t index, bool reinstate_index) -> Status {

    git_repository* repo = get_repo(handle);

    // Set apply options
    git_stash_apply_options opts{};
    git_stash_apply_options_init(&opts, GIT_STASH_APPLY_OPTIONS_VERSION);

    if (reinstate_index) {
        opts.flags = GIT_STASH_APPLY_REINSTATE_INDEX;
    }

    int error = git_stash_apply(repo, index, &opts);

    if (error < 0) {
        if (error == GIT_ENOTFOUND) {
            Error err;
            err.code = Error::Code::ObjectNotFound;
            err.message = "Stash not found at index " + std::to_string(index);
            return std::unexpected(std::move(err));
        }
        if (error == GIT_ECONFLICT) {
            Error err;
            err.code = Error::Code::MergeConflict;
            err.message = "Conflicts occurred while applying stash";
            return std::unexpected(std::move(err));
        }
        return std::unexpected(make_libgit2_error(error, "Failed to apply stash"));
    }

    return {};
}

auto LibGit2Backend::drop_stash(RepoHandle& handle, size_t index) -> Status {
    git_repository* repo = get_repo(handle);

    int error = git_stash_drop(repo, index);

    if (error < 0) {
        if (error == GIT_ENOTFOUND) {
            Error err;
            err.code = Error::Code::ObjectNotFound;
            err.message = "Stash not found at index " + std::to_string(index);
            return std::unexpected(std::move(err));
        }
        return std::unexpected(make_libgit2_error(error, "Failed to drop stash"));
    }

    return {};
}

// ============================================================================
// Rollback Operations (Move branch pointer to previous commit)
// ============================================================================

auto LibGit2Backend::rollback(RepoHandle& handle, const domain::ObjectId& target, ResetMode mode)
    -> Status {

    git_repository* repo = get_repo(handle);

    // Convert target OID
    git_oid target_oid = to_oid(target);

    // Lookup target object
    git_object* target_obj = nullptr;
    int error = git_object_lookup(&target_obj, repo, &target_oid, GIT_OBJECT_COMMIT);

    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to find target commit"));
    }

    // RAII wrapper for target object
    struct ObjDeleter {
        git_object* obj;
        ~ObjDeleter() {
            if (obj)
                git_object_free(obj);
        }
    } obj_guard{target_obj};

    // Convert rollback mode
    git_reset_t reset_type;
    switch (mode) {
        case ResetMode::Soft:
            reset_type = GIT_RESET_SOFT;
            break;
        case ResetMode::Mixed:
            reset_type = GIT_RESET_MIXED;
            break;
        case ResetMode::Hard:
            reset_type = GIT_RESET_HARD;
            break;
        default:
            Error err;
            err.code = Error::Code::InvalidArgument;
            err.message = "Invalid rollback mode";
            return std::unexpected(std::move(err));
    }

    // Move branch to target commit
    error = git_reset(repo, target_obj, reset_type, nullptr);

    if (error < 0) {
        if (error == GIT_EUNMERGED) {
            Error err;
            err.code = Error::Code::UnmergedEntries;
            err.message = "Cannot rollback with unmerged changes";
            return std::unexpected(std::move(err));
        }
        return std::unexpected(make_libgit2_error(error, "Failed to rollback to target commit"));
    }

    return {};
}

// ============================================================================
// Select Commit Operations (Apply commits to current branch)
// ============================================================================

auto LibGit2Backend::select_commit(RepoHandle& handle, const domain::ObjectId& commit,
                                   bool no_commit) -> Status {

    git_repository* repo = get_repo(handle);

    // Convert commit OID
    git_oid commit_oid = to_oid(commit);

    // Lookup the commit to apply
    git_commit* target_commit = nullptr;
    int error = git_commit_lookup(&target_commit, repo, &commit_oid);

    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to find commit to apply"));
    }

    // RAII wrapper for commit
    struct CommitDeleter {
        git_commit* commit;
        ~CommitDeleter() {
            if (commit)
                git_commit_free(commit);
        }
    } commit_guard{target_commit};

    // Set cherry-pick options
    git_cherrypick_options opts{};
    git_cherrypick_options_init(&opts, GIT_CHERRYPICK_OPTIONS_VERSION);

    // Apply the commit to current branch
    error = git_cherrypick(repo, target_commit, &opts);

    if (error < 0) {
        if (error == GIT_ECONFLICT) {
            Error err;
            err.code = Error::Code::MergeConflict;
            err.message = "Conflicts occurred while applying commit";
            return std::unexpected(std::move(err));
        }
        if (error == GIT_EUNMERGED) {
            Error err;
            err.code = Error::Code::UnmergedEntries;
            err.message = "Cannot apply commit with unmerged changes";
            return std::unexpected(std::move(err));
        }
        return std::unexpected(make_libgit2_error(error, "Failed to apply commit"));
    }

    // If no_commit is false, create the commit automatically
    if (!no_commit) {
        // Get the index to check for conflicts
        git_index* index = nullptr;
        error = git_repository_index(&index, repo);

        if (error < 0) {
            return std::unexpected(make_libgit2_error(error, "Failed to get repository index"));
        }

        // RAII wrapper for index
        struct IndexDeleter {
            git_index* idx;
            ~IndexDeleter() {
                if (idx)
                    git_index_free(idx);
            }
        } index_guard{index};

        // Check if there are conflicts
        if (git_index_has_conflicts(index)) {
            Error err;
            err.code = Error::Code::MergeConflict;
            err.message = "Cannot commit changes with conflicts";
            return std::unexpected(std::move(err));
        }

        // Write the index as a tree
        git_oid tree_oid;
        error = git_index_write_tree(&tree_oid, index);

        if (error < 0) {
            return std::unexpected(make_libgit2_error(error, "Failed to write tree"));
        }

        // Lookup the tree
        git_tree* tree = nullptr;
        error = git_tree_lookup(&tree, repo, &tree_oid);

        if (error < 0) {
            return std::unexpected(make_libgit2_error(error, "Failed to lookup tree"));
        }

        // RAII wrapper for tree
        struct TreeDeleter {
            git_tree* t;
            ~TreeDeleter() {
                if (t)
                    git_tree_free(t);
            }
        } tree_guard{tree};

        // Get HEAD as parent
        git_reference* head_ref = nullptr;
        error = git_repository_head(&head_ref, repo);

        if (error < 0) {
            return std::unexpected(make_libgit2_error(error, "Failed to get HEAD"));
        }

        // RAII wrapper for reference
        struct RefDeleter {
            git_reference* ref;
            ~RefDeleter() {
                if (ref)
                    git_reference_free(ref);
            }
        } ref_guard{head_ref};

        const git_oid* head_oid = git_reference_target(head_ref);
        git_commit* parent_commit = nullptr;
        error = git_commit_lookup(&parent_commit, repo, head_oid);

        if (error < 0) {
            return std::unexpected(make_libgit2_error(error, "Failed to lookup parent commit"));
        }

        // RAII wrapper for parent commit
        struct ParentDeleter {
            git_commit* c;
            ~ParentDeleter() {
                if (c)
                    git_commit_free(c);
            }
        } parent_guard{parent_commit};

        // Get author and committer from the original commit
        const git_signature* author = git_commit_author(target_commit);
        const git_signature* committer = git_commit_committer(target_commit);

        // Get message from original commit
        const char* message = git_commit_message(target_commit);

        // Create the commit
        git_oid new_commit_oid;
        const git_commit* parents[] = {parent_commit};

        error = git_commit_create(&new_commit_oid, repo, "HEAD", author, committer,
                                  nullptr, // encoding
                                  message, tree,
                                  1, // parent count
                                  parents);

        if (error < 0) {
            return std::unexpected(make_libgit2_error(error, "Failed to create commit"));
        }

        // Cleanup operation state
        git_repository_state_cleanup(repo);
    }

    return {};
}

// ============================================================================
// Undo Commit Operations (Revert - create new commit that reverses changes)
// ============================================================================

auto LibGit2Backend::undo_commit(RepoHandle& handle, const domain::ObjectId& commit, bool no_commit)
    -> Status {

    git_repository* repo = get_repo(handle);

    // Convert commit OID
    git_oid commit_oid = to_oid(commit);

    // Lookup the commit to revert
    git_commit* revert_commit = nullptr;
    int error = git_commit_lookup(&revert_commit, repo, &commit_oid);

    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to find commit to undo"));
    }

    // RAII wrapper for commit
    struct CommitDeleter {
        git_commit* commit;
        ~CommitDeleter() {
            if (commit)
                git_commit_free(commit);
        }
    } commit_guard{revert_commit};

    // Set revert options
    git_revert_options opts{};
    git_revert_options_init(&opts, GIT_REVERT_OPTIONS_VERSION);

    // Try to auto-resolve conflicts by favoring the revert changes
    opts.merge_opts.file_favor = GIT_MERGE_FILE_FAVOR_NORMAL;

    // Apply the revert to current branch (updates index and working directory)
    error = git_revert(repo, revert_commit, &opts);

    if (error < 0) {
        if (error == GIT_ECONFLICT) {
            Error err;
            err.code = Error::Code::MergeConflict;
            err.message = "Conflicts occurred while undoing commit";
            return std::unexpected(std::move(err));
        }
        if (error == GIT_EUNMERGED) {
            Error err;
            err.code = Error::Code::UnmergedEntries;
            err.message = "Cannot undo commit with unmerged changes";
            return std::unexpected(std::move(err));
        }
        return std::unexpected(make_libgit2_error(error, "Failed to undo commit"));
    }

    // If no_commit is false, create the commit automatically
    if (!no_commit) {
        // Get the index to check for conflicts
        git_index* index = nullptr;
        error = git_repository_index(&index, repo);

        if (error < 0) {
            return std::unexpected(make_libgit2_error(error, "Failed to get repository index"));
        }

        // RAII wrapper for index
        struct IndexDeleter {
            git_index* idx;
            ~IndexDeleter() {
                if (idx)
                    git_index_free(idx);
            }
        } index_guard{index};

        // Check if there are conflicts
        if (git_index_has_conflicts(index)) {
            Error err;
            err.code = Error::Code::MergeConflict;
            err.message = "Cannot commit changes with conflicts";
            return std::unexpected(std::move(err));
        }

        // Write the index as a tree
        git_oid tree_oid;
        error = git_index_write_tree(&tree_oid, index);

        if (error < 0) {
            return std::unexpected(make_libgit2_error(error, "Failed to write tree"));
        }

        // Lookup the tree
        git_tree* tree = nullptr;
        error = git_tree_lookup(&tree, repo, &tree_oid);

        if (error < 0) {
            return std::unexpected(make_libgit2_error(error, "Failed to lookup tree"));
        }

        // RAII wrapper for tree
        struct TreeDeleter {
            git_tree* t;
            ~TreeDeleter() {
                if (t)
                    git_tree_free(t);
            }
        } tree_guard{tree};

        // Get HEAD as parent
        git_reference* head_ref = nullptr;
        error = git_repository_head(&head_ref, repo);

        if (error < 0) {
            return std::unexpected(make_libgit2_error(error, "Failed to get HEAD"));
        }

        // RAII wrapper for reference
        struct RefDeleter {
            git_reference* ref;
            ~RefDeleter() {
                if (ref)
                    git_reference_free(ref);
            }
        } ref_guard{head_ref};

        const git_oid* head_oid = git_reference_target(head_ref);
        git_commit* parent_commit = nullptr;
        error = git_commit_lookup(&parent_commit, repo, head_oid);

        if (error < 0) {
            return std::unexpected(make_libgit2_error(error, "Failed to lookup parent commit"));
        }

        // RAII wrapper for parent commit
        struct ParentDeleter {
            git_commit* c;
            ~ParentDeleter() {
                if (c)
                    git_commit_free(c);
            }
        } parent_guard{parent_commit};

        // Get default signature (or create a fallback if not configured)
        git_signature* sig = nullptr;
        error = git_signature_default(&sig, repo);

        if (error < 0) {
            // Fallback: create a signature manually
            error = git_signature_now(&sig, "Unknown", "unknown@example.com");
            if (error < 0) {
                return std::unexpected(make_libgit2_error(error, "Failed to create signature"));
            }
        }

        // RAII wrapper for signature
        struct SigDeleter {
            git_signature* s;
            ~SigDeleter() {
                if (s)
                    git_signature_free(s);
            }
        } sig_guard{sig};

        // Create commit message
        const char* original_message = git_commit_message(revert_commit);
        std::string msg_str = original_message ? original_message : "";

        // Remove trailing newlines
        while (!msg_str.empty() && (msg_str.back() == '\n' || msg_str.back() == '\r')) {
            msg_str.pop_back();
        }

        std::string commit_message = std::string("Revert \"") + msg_str + "\"\n";

        // Create the commit
        git_oid new_commit_oid;
        const git_commit* parents[] = {parent_commit};

        error = git_commit_create(&new_commit_oid, repo, "HEAD", sig, sig,
                                  nullptr, // encoding
                                  commit_message.c_str(), tree,
                                  1, // parent count
                                  parents);

        if (error < 0) {
            return std::unexpected(make_libgit2_error(error, "Failed to create commit"));
        }

        // Cleanup operation state
        git_repository_state_cleanup(repo);
    }

    return {};
}

auto LibGit2Backend::merge(RepoHandle& repo, const std::string& source, MergeStrategy strategy,
                           bool commit, const std::string& message) -> Result<MergeStatus> {
    auto* git_repo = get_repo(repo);

    // Resolve the source reference
    git_annotated_commit* their_head = nullptr;
    git_reference* their_ref = nullptr;

    // Try to resolve as a reference first
    if (git_reference_dwim(&their_ref, git_repo, source.c_str()) == 0) {
        git_annotated_commit_from_ref(&their_head, git_repo, their_ref);
        git_reference_free(their_ref);
    } else {
        // Try as OID
        git_oid oid;
        if (git_oid_fromstr(&oid, source.c_str()) == 0) {
            git_annotated_commit_lookup(&their_head, git_repo, &oid);
        }
    }

    if (!their_head) {
        return std::unexpected(
            make_error(Error::Code::InvalidReference, "Cannot resolve merge source: " + source));
    }

    // Perform merge analysis
    git_merge_analysis_t analysis;
    git_merge_preference_t preference = GIT_MERGE_PREFERENCE_NONE;
    const git_annotated_commit* their_heads[] = {their_head};

    int error = git_merge_analysis(&analysis, &preference, git_repo, their_heads, 1);
    if (error < 0) {
        git_annotated_commit_free(their_head);
        return std::unexpected(make_libgit2_error(error, "Merge analysis failed"));
    }

    MergeStatus result;

    // Check if already up to date
    if (analysis & GIT_MERGE_ANALYSIS_UP_TO_DATE) {
        git_annotated_commit_free(their_head);
        result.type = MergeStatus::Type::UpToDate;
        return result;
    }

    // Check for fast-forward
    if (analysis & GIT_MERGE_ANALYSIS_FASTFORWARD) {
        if (strategy != MergeStrategy::NoFastForward) {
            // Perform fast-forward
            git_reference* head_ref = nullptr;
            error = git_repository_head(&head_ref, git_repo);
            if (error < 0) {
                git_annotated_commit_free(their_head);
                return std::unexpected(make_libgit2_error(error, "Cannot get HEAD reference"));
            }

            const git_oid* target_oid = git_annotated_commit_id(their_head);
            git_reference* new_ref = nullptr;
            error = git_reference_set_target(&new_ref, head_ref, target_oid, "merge: Fast-forward");

            git_reference_free(head_ref);
            git_reference_free(new_ref);

            if (error < 0) {
                git_annotated_commit_free(their_head);
                return std::unexpected(make_libgit2_error(error, "Fast-forward failed"));
            }

            // Update working directory
            git_checkout_options checkout_opts;
            git_checkout_options_init(&checkout_opts, GIT_CHECKOUT_OPTIONS_VERSION);
            checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE | GIT_CHECKOUT_RECREATE_MISSING;
            error = git_checkout_head(git_repo, &checkout_opts);
            if (error < 0) {
                git_annotated_commit_free(their_head);
                return std::unexpected(
                    make_libgit2_error(error, "Checkout failed during fast-forward"));
            }

            result.type = MergeStatus::Type::FastForward;
            char oid_str[GIT_OID_SHA1_HEXSIZE + 1];
            git_oid_tostr(oid_str, sizeof(oid_str), target_oid);
            result.commit_id = oid_str;

            git_annotated_commit_free(their_head);
            return result;
        }
    }

    // Check if fast-forward only mode
    if (strategy == MergeStrategy::FastForwardOnly) {
        git_annotated_commit_free(their_head);
        return std::unexpected(
            make_error(Error::Code::MergeError,
                       "Fast-forward not possible and fast-forward-only mode requested"));
    }

    // Perform actual merge
    git_merge_options merge_opts;
    git_merge_options_init(&merge_opts, GIT_MERGE_OPTIONS_VERSION);

    git_checkout_options checkout_opts;
    git_checkout_options_init(&checkout_opts, GIT_CHECKOUT_OPTIONS_VERSION);
    checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE | GIT_CHECKOUT_ALLOW_CONFLICTS;

    error = git_merge(git_repo, their_heads, 1, &merge_opts, &checkout_opts);
    if (error < 0) {
        git_annotated_commit_free(their_head);
        return std::unexpected(make_libgit2_error(error, "Merge failed"));
    }

    // Check for conflicts
    git_index* index = nullptr;
    git_repository_index(&index, git_repo);

    if (git_index_has_conflicts(index)) {
        // Get conflict list
        git_index_conflict_iterator* conflicts = nullptr;
        git_index_conflict_iterator_new(&conflicts, index);

        const git_index_entry* ancestor = nullptr;
        const git_index_entry* our = nullptr;
        const git_index_entry* their = nullptr;

        while (git_index_conflict_next(&ancestor, &our, &their, conflicts) == 0) {
            const char* path = our ? our->path : (their ? their->path : ancestor->path);
            if (path) {
                result.conflicts.push_back(path);
            }
        }

        git_index_conflict_iterator_free(conflicts);
        git_index_free(index);
        git_annotated_commit_free(their_head);

        result.type = MergeStatus::Type::Conflicts;
        return result;
    }

    // No conflicts
    if (!commit) {
        git_index_free(index);
        git_annotated_commit_free(their_head);
        result.type = MergeStatus::Type::Staged;
        return result;
    }

    // Create merge commit
    git_oid tree_oid;
    error = git_index_write_tree(&tree_oid, index);
    git_index_free(index);

    if (error < 0) {
        git_annotated_commit_free(their_head);
        return std::unexpected(make_libgit2_error(error, "Failed to write tree"));
    }

    git_tree* tree = nullptr;
    error = git_tree_lookup(&tree, git_repo, &tree_oid);
    if (error < 0) {
        git_annotated_commit_free(their_head);
        return std::unexpected(make_libgit2_error(error, "Failed to lookup tree"));
    }

    // Get parents
    git_commit* head_commit = nullptr;
    git_reference* head = nullptr;
    error = git_repository_head(&head, git_repo);
    if (error < 0) {
        git_tree_free(tree);
        git_annotated_commit_free(their_head);
        return std::unexpected(make_libgit2_error(error, "Failed to get HEAD"));
    }

    error = git_reference_peel((git_object**)&head_commit, head, GIT_OBJECT_COMMIT);
    if (error < 0) {
        git_reference_free(head);
        git_tree_free(tree);
        git_annotated_commit_free(their_head);
        return std::unexpected(make_libgit2_error(error, "Failed to peel HEAD"));
    }

    git_commit* their_commit = nullptr;
    error = git_commit_lookup(&their_commit, git_repo, git_annotated_commit_id(their_head));
    if (error < 0) {
        git_commit_free(head_commit);
        git_reference_free(head);
        git_tree_free(tree);
        git_annotated_commit_free(their_head);
        return std::unexpected(make_libgit2_error(error, "Failed to lookup their commit"));
    }

    // Create merge commit
    git_signature* sig = nullptr;
    error = git_signature_default(&sig, git_repo);
    if (error < 0) {
        // Fallback: create a signature manually
        error = git_signature_now(&sig, "Unknown", "unknown@example.com");
        if (error < 0) {
            git_commit_free(their_commit);
            git_commit_free(head_commit);
            git_reference_free(head);
            git_tree_free(tree);
            git_annotated_commit_free(their_head);
            return std::unexpected(make_libgit2_error(error, "Failed to create signature"));
        }
    }

    std::string msg = message.empty() ? ("Merge " + source) : message;

    const git_commit* parents[] = {head_commit, their_commit};
    git_oid merge_commit_oid;

    error = git_commit_create(&merge_commit_oid, git_repo, "HEAD", sig, sig, nullptr, msg.c_str(),
                              tree, 2, parents);

    git_signature_free(sig);
    git_tree_free(tree);
    git_commit_free(head_commit);
    git_commit_free(their_commit);
    git_reference_free(head);
    git_annotated_commit_free(their_head);

    // Clean up merge state
    git_repository_state_cleanup(git_repo);

    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to create merge commit"));
    }

    result.type = MergeStatus::Type::MergeCommit;
    char oid_str[GIT_OID_SHA1_HEXSIZE + 1];
    git_oid_tostr(oid_str, sizeof(oid_str), &merge_commit_oid);
    result.commit_id = oid_str;

    return result;
}

auto LibGit2Backend::rebase(RepoHandle& handle, const std::string& onto) -> Result<RebaseStats> {
    git_repository* repo = get_repo(handle);

    // Resolve the onto reference
    git_annotated_commit* onto_commit = nullptr;
    int error = git_annotated_commit_from_revspec(&onto_commit, repo, onto.c_str());
    if (error < 0) {
        return std::unexpected(
            make_libgit2_error(error, "Failed to resolve rebase target: " + onto));
    }

    struct AnnotatedCommitDeleter {
        void operator()(git_annotated_commit* c) const {
            if (c)
                git_annotated_commit_free(c);
        }
    };
    std::unique_ptr<git_annotated_commit, AnnotatedCommitDeleter> onto_guard(onto_commit);

    // Get current HEAD as the branch to rebase
    git_reference* head_ref = nullptr;
    error = git_repository_head(&head_ref, repo);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to get HEAD"));
    }

    struct RefDeleter {
        void operator()(git_reference* r) const {
            if (r)
                git_reference_free(r);
        }
    };
    std::unique_ptr<git_reference, RefDeleter> head_guard(head_ref);

    git_annotated_commit* branch_commit = nullptr;
    error = git_annotated_commit_from_ref(&branch_commit, repo, head_ref);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to get branch commit"));
    }
    std::unique_ptr<git_annotated_commit, AnnotatedCommitDeleter> branch_guard(branch_commit);

    // Initialize rebase
    git_rebase* rebase_handle = nullptr;
    git_rebase_options rebase_opts;
    git_rebase_options_init(&rebase_opts, GIT_REBASE_OPTIONS_VERSION);

    error =
        git_rebase_init(&rebase_handle, repo, branch_commit, nullptr, onto_commit, &rebase_opts);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to initialize rebase"));
    }

    struct RebaseDeleter {
        void operator()(git_rebase* r) const {
            if (r)
                git_rebase_free(r);
        }
    };
    std::unique_ptr<git_rebase, RebaseDeleter> rebase_guard(rebase_handle);

    // Track progress
    size_t commits_replayed = 0;
    std::vector<std::string> conflicts;

    // Get signature for committing (used throughout rebase)
    git_signature* sig = nullptr;
    error = git_signature_default(&sig, repo);
    if (error < 0) {
        // Try to create a fallback signature
        error = git_signature_now(&sig, "Rebase", "rebase@example.com");
        if (error < 0) {
            return std::unexpected(make_libgit2_error(error, "Failed to create signature"));
        }
    }

    struct SigDeleter {
        void operator()(git_signature* s) const {
            if (s)
                git_signature_free(s);
        }
    };
    std::unique_ptr<git_signature, SigDeleter> sig_guard(sig);

    // Process each rebase operation
    while (git_rebase_next(nullptr, rebase_handle) == 0) {
        // Commit the current operation
        git_oid commit_oid;
        error = git_rebase_commit(&commit_oid, rebase_handle, sig, sig, nullptr, nullptr);

        if (error == GIT_EUNMERGED) {
            // Conflicts detected
            git_index* index = nullptr;
            if (git_repository_index(&index, repo) == 0) {
                git_index_conflict_iterator* conflict_iter = nullptr;
                if (git_index_conflict_iterator_new(&conflict_iter, index) == 0) {
                    const git_index_entry *ancestor, *our, *their;
                    while (git_index_conflict_next(&ancestor, &our, &their, conflict_iter) == 0) {
                        if (our && our->path) {
                            conflicts.push_back(our->path);
                        } else if (their && their->path) {
                            conflicts.push_back(their->path);
                        }
                    }
                    git_index_conflict_iterator_free(conflict_iter);
                }
                git_index_free(index);
            }

            // Abort the rebase due to conflicts
            git_rebase_abort(rebase_handle);

            RebaseStats result;
            result.commits_replayed = commits_replayed;
            result.conflicts = std::move(conflicts);
            return result;
        } else if (error < 0) {
            git_rebase_abort(rebase_handle);
            return std::unexpected(make_libgit2_error(error, "Failed to commit rebase operation"));
        }

        commits_replayed++;
    }

    // Finish the rebase
    error = git_rebase_finish(rebase_handle, sig);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to finish rebase"));
    }

    // Get the new HEAD
    git_reference* new_head_ref = nullptr;
    error = git_repository_head(&new_head_ref, repo);
    if (error < 0) {
        return std::unexpected(make_libgit2_error(error, "Failed to get new HEAD after rebase"));
    }
    std::unique_ptr<git_reference, RefDeleter> new_head_guard(new_head_ref);

    const git_oid* new_head_oid = git_reference_target(new_head_ref);
    if (!new_head_oid) {
        Error err;
        err.code = Error::Code::ReferenceNotFound;
        err.message = "New HEAD has no target";
        return std::unexpected(std::move(err));
    }

    RebaseStats result;
    result.new_head = from_oid(*new_head_oid);
    result.commits_replayed = commits_replayed;
    result.conflicts = std::move(conflicts);

    return result;
}

} // namespace repo::backend
