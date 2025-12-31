#include <repo/ops/clean.hpp>
#include <repo/ops/status.hpp>
#include <repo/repository.hpp>

#include <algorithm>
#include <filesystem>

namespace repo::ops {

auto clean(Repository& repo, CleanParams params) -> Result<CleanResult> {
    // Safety check: require force flag unless dry_run
    if (!params.dry_run && !params.force) {
        Error err;
        err.code = Error::Code::InvalidArgument;
        err.message =
            "Clean requires --force flag to actually delete files (use --dry-run to preview)";
        return std::unexpected(std::move(err));
    }

    // Get repository status to find untracked files
    auto status_result = status(repo);
    if (!status_result) {
        return std::unexpected(std::move(status_result.error()));
    }

    auto untracked = status_result->untracked();

    CleanResult result;

    // Get the working directory
    auto workdir = repo.backend().workdir(repo.repo_handle());

    // Collect files and directories to remove
    std::vector<std::filesystem::path> to_remove;
    for (const auto& file_status : untracked) {
        auto full_path = workdir / file_status.path;

        // Check if it's a directory
        if (std::filesystem::is_directory(full_path)) {
            if (params.include_directories) {
                to_remove.push_back(full_path);
            }
        } else {
            to_remove.push_back(full_path);
        }
    }

    // Sort paths by depth (deepest first) to safely remove directories
    std::sort(to_remove.begin(), to_remove.end(), [](const auto& a, const auto& b) {
        return std::distance(a.begin(), a.end()) > std::distance(b.begin(), b.end());
    });

    // Remove files and directories
    for (const auto& path : to_remove) {
        if (!params.dry_run) {
            try {
                if (std::filesystem::is_directory(path)) {
                    std::filesystem::remove_all(path);
                    result.removed_directories.push_back(std::filesystem::relative(path, workdir));
                } else {
                    std::filesystem::remove(path);
                    result.removed_files.push_back(std::filesystem::relative(path, workdir));
                }
            } catch (const std::filesystem::filesystem_error& e) {
                Error err;
                err.code = Error::Code::Unknown;
                err.message = std::string("Failed to remove: ") + e.what();
                return std::unexpected(std::move(err));
            }
        } else {
            // Dry run - just record what would be removed
            if (std::filesystem::is_directory(path)) {
                result.removed_directories.push_back(std::filesystem::relative(path, workdir));
            } else {
                result.removed_files.push_back(std::filesystem::relative(path, workdir));
            }
        }
    }

    return result;
}

} // namespace repo::ops
