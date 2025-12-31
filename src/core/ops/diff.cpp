#include <repo/ops/diff.hpp>
#include <repo/repository.hpp>

namespace repo::ops {

auto DiffResult::total_additions() const -> size_t {
    size_t total = 0;
    for (const auto& diff : diffs) {
        total += diff.additions;
    }
    return total;
}

auto DiffResult::total_deletions() const -> size_t {
    size_t total = 0;
    for (const auto& diff : diffs) {
        total += diff.deletions;
    }
    return total;
}

auto DiffResult::files_changed() const -> size_t {
    return diffs.size();
}

auto diff(Repository& repo, DiffParams params) -> Result<DiffResult> {
    std::vector<domain::FileDiff> all_diffs;

    if (params.mode == DiffParams::Mode::Unstaged || params.mode == DiffParams::Mode::All) {
        auto unstaged = repo.backend().diff_index_to_workdir(repo.repo_handle());
        if (!unstaged) {
            return std::unexpected(std::move(unstaged.error()));
        }
        all_diffs.insert(all_diffs.end(), std::make_move_iterator(unstaged->begin()),
                         std::make_move_iterator(unstaged->end()));
    }

    if (params.mode == DiffParams::Mode::Staged || params.mode == DiffParams::Mode::All) {
        auto staged = repo.backend().diff_tree_to_index(repo.repo_handle());
        if (!staged) {
            return std::unexpected(std::move(staged.error()));
        }
        all_diffs.insert(all_diffs.end(), std::make_move_iterator(staged->begin()),
                         std::make_move_iterator(staged->end()));
    }

    return DiffResult{.diffs = std::move(all_diffs)};
}

} // namespace repo::ops
