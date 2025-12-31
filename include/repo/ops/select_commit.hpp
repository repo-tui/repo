#pragma once

#include <string>

#include "../domain/object_id.hpp"
#include "../repository.hpp"
#include "../result.hpp"

namespace repo::ops {

struct SelectCommitParams {
    domain::ObjectId commit;
    bool no_commit = false; // If true, apply changes without committing
};

/// Select and apply a commit onto the current branch
/// Applies the changes from the specified commit onto HEAD
auto select_commit(Repository& repo, SelectCommitParams params) -> Status;

} // namespace repo::ops
