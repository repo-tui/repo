#pragma once

#include <optional>
#include <string>

#include "../domain/commit.hpp"
#include "../domain/signature.hpp"
#include "../result.hpp"

namespace repo {
class Repository;

namespace ops {

struct CommitParams {
    std::string message;
    std::optional<domain::Signature> author;
    std::optional<domain::Signature> committer;
};

struct CommitResult {
    domain::Commit commit;
    size_t files_changed;
    size_t insertions;
    size_t deletions;
};

struct ShowCommitParams {
    std::string ref; // Commit ID, branch name, or ref (e.g., "HEAD", "main", "abc123")
};

struct ShowCommitResult {
    domain::Commit commit;
};

struct AmendCommitParams {
    std::optional<std::string> message;         // New message (empty = keep existing)
    std::optional<domain::Signature> author;    // New author (empty = keep existing)
    std::optional<domain::Signature> committer; // New committer (empty = use default)
};

struct AmendCommitResult {
    domain::Commit commit;
};

auto commit(Repository& repo, CommitParams params) -> Result<CommitResult>;
auto show_commit(Repository& repo, ShowCommitParams params) -> Result<ShowCommitResult>;
auto amend_commit(Repository& repo, AmendCommitParams params) -> Result<AmendCommitResult>;

} // namespace ops
} // namespace repo
