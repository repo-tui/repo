#pragma once

#include <optional>
#include <string>
#include <vector>

namespace repo::domain {

/// Git remote
struct Remote {
    std::string name;                        // Remote name (e.g., "origin")
    std::string url;                         // Fetch URL
    std::optional<std::string> push_url;     // Push URL (if different from fetch)
    std::vector<std::string> fetch_refspecs; // Fetch refspecs
    std::vector<std::string> push_refspecs;  // Push refspecs

    auto operator==(const Remote&) const -> bool = default;
};

} // namespace repo::domain
