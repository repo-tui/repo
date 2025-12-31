#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../domain/commit.hpp"
#include "../domain/object_id.hpp"
#include "../result.hpp"

namespace repo {
class Repository;

namespace ops {

struct ListCommitsParams {
    std::optional<std::string> ref_name;   // Start from this ref (default: HEAD)
    std::optional<size_t> max_count;       // Limit number of commits
    std::optional<domain::ObjectId> since; // Only commits newer than this
    std::optional<domain::ObjectId> until; // Only commits older than this
};

struct ListCommitsResult {
    std::vector<domain::Commit> commits;

    [[nodiscard]] auto first() const -> const domain::Commit*;
    [[nodiscard]] auto last() const -> const domain::Commit*;
};

auto list_commits(Repository& repo, ListCommitsParams params = {}) -> Result<ListCommitsResult>;

} // namespace ops
} // namespace repo
