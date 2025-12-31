#pragma once

#include <optional>
#include <string>

#include "object_id.hpp"

namespace repo::domain {

/// Git branch
struct Branch {
    std::string name;      // Short name (e.g., "main")
    std::string full_name; // Full ref name (e.g., "refs/heads/main")
    ObjectId target;       // Commit this branch points to
    bool is_remote;        // True if this is a remote branch
    bool is_head;          // True if this is the current HEAD

    // Tracking information
    std::optional<std::string> upstream; // Upstream branch name (e.g., "origin/main")

    /// Tracking status relative to upstream
    struct TrackingInfo {
        size_t ahead;  // Commits ahead of upstream
        size_t behind; // Commits behind upstream

        [[nodiscard]] auto is_up_to_date() const -> bool { return ahead == 0 && behind == 0; }

        auto operator==(const TrackingInfo&) const -> bool = default;
    };
    std::optional<TrackingInfo> tracking;

    auto operator==(const Branch&) const -> bool = default;
};

} // namespace repo::domain
