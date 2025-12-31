#pragma once

#include <string>
#include <vector>

#include "../domain/remote.hpp"
#include "../result.hpp"

namespace repo {
class Repository;

namespace ops {

struct ListRemotesResult {
    std::vector<domain::Remote> remotes;
};

struct AddRemoteParams {
    std::string name;
    std::string url;
};

struct RemoveRemoteParams {
    std::string name;
};

struct FetchParams {
    std::string remote;  // Remote name (default: "origin")
    std::string refspec; // Optional refspec to fetch specific refs
    bool prune = false;  // Remove remote-tracking refs that don't exist on remote
    bool tags = true;    // Fetch tags
};

struct FetchResult {
    size_t received_objects = 0;
    size_t indexed_objects = 0;
    size_t received_bytes = 0;
    std::vector<std::string> updated_refs; // List of updated references
};

struct PushParams {
    std::string remote;        // Remote name (default: "origin")
    std::string refspec;       // Optional refspec to push specific refs (default: current branch)
    bool force = false;        // Allow non-fast-forward updates
    bool set_upstream = false; // Set upstream tracking for current branch
};

struct PushResult {
    size_t sent_objects = 0;
    size_t sent_bytes = 0;
    std::vector<std::string> updated_refs; // List of updated references
};

struct PullParams {
    std::string remote;  // Remote name (default: "origin")
    bool rebase = false; // Use rebase instead of merge
    bool prune = false;  // Remove remote-tracking refs that don't exist on remote
    bool tags = true;    // Fetch tags
};

struct PullResult {
    FetchResult fetch_result;
    bool updated = false;   // Whether local branch was updated
    std::string merge_type; // "fast-forward", "merge", "rebase", "up-to-date"
};

auto list_remotes(Repository& repo) -> Result<ListRemotesResult>;
auto add_remote(Repository& repo, AddRemoteParams params) -> Status;
auto remove_remote(Repository& repo, RemoveRemoteParams params) -> Status;
auto fetch(Repository& repo, FetchParams params) -> Result<FetchResult>;
auto push(Repository& repo, PushParams params) -> Result<PushResult>;
auto pull(Repository& repo, PullParams params) -> Result<PullResult>;

} // namespace ops
} // namespace repo
