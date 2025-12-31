#pragma once

#include <chrono>
#include <string>

#include "object_id.hpp"
#include "signature.hpp"

namespace repo::domain {

/// Stash entry
struct Stash {
    size_t index;                               // Stash index (0 = most recent)
    ObjectId commit_id;                         // OID of stash commit
    std::string message;                        // Stash message
    Signature author;                           // Who created the stash
    std::chrono::system_clock::time_point when; // When created

    auto operator==(const Stash&) const -> bool = default;
};

} // namespace repo::domain
