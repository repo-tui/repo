#pragma once

#include <fmt/format.h>

#include <optional>
#include <string>

#include "../error.hpp"
#include "../repository.hpp"

namespace repo::ops {

/// Check if repository has any commits
/// Returns error with helpful guidance if repository is empty
inline auto require_commits(Repository& repo, const std::string& operation_name)
    -> std::optional<Error> {

    auto head_result = repo.backend().get_head(repo.repo_handle());

    if (!head_result.has_value()) {
        auto code = head_result.error().code;

        // Empty repository detection (unborn branch)
        if (code == Error::Code::ReferenceNotFound || code == Error::Code::InvalidReference) {

            return make_error(
                Error::Code::InvalidReference,
                fmt::format("Cannot {} - repository has no commits yet", operation_name),
                "The repository is empty and has no commit history.\n\n"
                "Create your first commit:\n"
                "  1. repo stage .\n"
                "  2. repo commit create -m \"Initial commit\"\n\n"
                "Then try the operation again.");
        }

        // Some other error with HEAD - return it as-is
        return head_result.error();
    }

    // Repository has commits
    return std::nullopt;
}

} // namespace repo::ops
