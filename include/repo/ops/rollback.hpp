#pragma once

#include <string>

#include "../backend/git_backend.hpp"
#include "../domain/object_id.hpp"
#include "../repository.hpp"
#include "../result.hpp"

namespace repo::ops {

using RollbackMode = backend::GitBackend::ResetMode;

struct RollbackParams {
    domain::ObjectId target;
    RollbackMode mode = RollbackMode::Mixed; // Default to mixed mode
};

/// Rollback current branch to the specified commit
/// - Soft: Move branch pointer only, keep index and working directory
/// - Mixed: Move branch and update index, keep working directory (default)
/// - Hard: Move branch and update both index and working directory
auto rollback(Repository& repo, RollbackParams params) -> Status;

} // namespace repo::ops
