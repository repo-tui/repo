#pragma once

#include <string>
#include <vector>

#include "../domain/object_id.hpp"
#include "../domain/signature.hpp"
#include "../domain/stash.hpp"
#include "../repository.hpp"
#include "../result.hpp"

namespace repo::ops {

struct ListStashesResult {
    std::vector<domain::Stash> stashes;
};

struct CreateStashParams {
    std::string message;
    domain::Signature stasher;
    bool include_untracked = false;
    bool keep_index = false;
};

struct CreateStashResult {
    domain::ObjectId stash_id;
};

struct ApplyStashParams {
    size_t index = 0; // 0 = most recent
    bool reinstate_index = false;
};

struct PopStashParams {
    size_t index = 0; // 0 = most recent
    bool reinstate_index = false;
};

struct DropStashParams {
    size_t index = 0; // 0 = most recent
};

/// List all stashes
auto list_stashes(Repository& repo) -> Result<ListStashesResult>;

/// Create a new stash
auto create_stash(Repository& repo, CreateStashParams params) -> Result<CreateStashResult>;

/// Apply a stash without removing it
auto apply_stash(Repository& repo, ApplyStashParams params) -> Status;

/// Apply and remove a stash
auto pop_stash(Repository& repo, PopStashParams params) -> Status;

/// Remove a stash without applying it
auto drop_stash(Repository& repo, DropStashParams params) -> Status;

} // namespace repo::ops
