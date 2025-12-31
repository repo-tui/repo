#pragma once

#include <string>

#include "../domain/object_id.hpp"
#include "../repository.hpp"
#include "../result.hpp"

namespace repo::ops {

struct UndoCommitParams {
    domain::ObjectId commit; // Commit to undo
    bool no_commit = false;  // If true, apply changes without committing
};

/// Undo a commit by creating a new commit that reverses its changes
/// This is a safe operation that creates a new commit (unlike rollback which moves the branch
/// pointer)
auto undo_commit(Repository& repo, UndoCommitParams params) -> Status;

} // namespace repo::ops
