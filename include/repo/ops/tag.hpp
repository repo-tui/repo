#pragma once

#include <string>
#include <vector>

#include "../domain/object_id.hpp"
#include "../domain/signature.hpp"
#include "../domain/tag.hpp"
#include "../repository.hpp"
#include "../result.hpp"

namespace repo::ops {

struct ListTagsResult {
    std::vector<domain::Tag> tags;
};

struct CreateTagParams {
    std::string name;
    domain::ObjectId target;
    std::string message; // Empty for lightweight tag
    domain::Signature tagger;
    bool force = false;
};

struct DeleteTagParams {
    std::string name;
};

/// List all tags in the repository
auto list_tags(Repository& repo) -> Result<ListTagsResult>;

/// Create a tag (lightweight if message is empty, annotated otherwise)
auto create_tag(Repository& repo, CreateTagParams params) -> Status;

/// Delete a tag by name
auto delete_tag(Repository& repo, DeleteTagParams params) -> Status;

} // namespace repo::ops
