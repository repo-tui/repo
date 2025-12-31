#pragma once

#include <optional>
#include <string>

#include "object_id.hpp"
#include "signature.hpp"

namespace repo::domain {

/// Git tag
struct Tag {
    std::string name; // Tag name (without refs/tags/ prefix)
    ObjectId target;  // OID of tagged object

    bool is_annotated; // True if annotated tag, false if lightweight

    // Only for annotated tags
    std::optional<ObjectId> tag_object_id; // OID of tag object itself
    std::optional<Signature> tagger;       // Who created the tag
    std::optional<std::string> message;    // Tag message

    auto operator==(const Tag&) const -> bool = default;
};

} // namespace repo::domain
