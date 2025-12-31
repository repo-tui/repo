#pragma once

#include <string>
#include <variant>

#include "object_id.hpp"

namespace repo::domain {

/// Git reference (branch, tag, etc.)
struct Reference {
    std::string name; // Full name (e.g., "refs/heads/main")

    enum class Type {
        Direct,  // Points directly to an OID
        Symbolic // Points to another reference (e.g., HEAD -> refs/heads/main)
    };
    Type type;

    /// Either an ObjectId (direct) or reference name (symbolic)
    std::variant<ObjectId, std::string> target;

    /// Check if this is a symbolic reference
    [[nodiscard]] auto is_symbolic() const -> bool { return type == Type::Symbolic; }

    /// Check if this is a direct reference
    [[nodiscard]] auto is_direct() const -> bool { return type == Type::Direct; }

    auto operator==(const Reference&) const -> bool = default;
};

} // namespace repo::domain
