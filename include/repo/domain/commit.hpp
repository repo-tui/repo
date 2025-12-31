#pragma once

#include <string>
#include <vector>

#include "object_id.hpp"
#include "signature.hpp"

namespace repo::domain {

/// Git commit object
struct Commit {
    ObjectId id;
    ObjectId tree_id;
    std::vector<ObjectId> parent_ids;
    Signature author;
    Signature committer;
    std::string message;

    /// Get first line of commit message
    [[nodiscard]] auto summary() const -> std::string;

    /// Get commit message body (everything after first line)
    [[nodiscard]] auto body() const -> std::string;

    /// Check if this is a merge commit
    [[nodiscard]] auto is_merge() const -> bool { return parent_ids.size() > 1; }

    /// Check if this is a root commit (no parents)
    [[nodiscard]] auto is_root() const -> bool { return parent_ids.empty(); }

    auto operator==(const Commit&) const -> bool = default;
};

} // namespace repo::domain
