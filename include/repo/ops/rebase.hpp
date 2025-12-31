#pragma once

#include <string>
#include <vector>

#include "../domain/object_id.hpp"
#include "../result.hpp"

namespace repo {
class Repository;

namespace ops {

struct RebaseParams {
    std::string onto;         // Branch or commit to rebase onto
    bool interactive = false; // Interactive rebase (not yet implemented)
};

struct RebaseResult {
    domain::ObjectId new_head;          // OID of the new HEAD after rebase
    size_t commits_replayed;            // Number of commits replayed
    std::vector<std::string> conflicts; // Paths with conflicts (if any)

    [[nodiscard]] auto has_conflicts() const -> bool { return !conflicts.empty(); }
};

auto rebase(Repository& repo, RebaseParams params) -> Result<RebaseResult>;

} // namespace ops
} // namespace repo
