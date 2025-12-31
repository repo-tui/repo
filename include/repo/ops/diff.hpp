#pragma once

#include <vector>

#include "../domain/diff.hpp"
#include "../result.hpp"

namespace repo {
class Repository;

namespace ops {

struct DiffParams {
    enum class Mode {
        Unstaged, // Working tree vs index
        Staged,   // Index vs HEAD
        All       // All changes (staged + unstaged)
    };

    Mode mode = Mode::Unstaged;
};

struct DiffResult {
    std::vector<domain::FileDiff> diffs;

    [[nodiscard]] auto total_additions() const -> size_t;
    [[nodiscard]] auto total_deletions() const -> size_t;
    [[nodiscard]] auto files_changed() const -> size_t;
};

auto diff(Repository& repo, DiffParams params = {}) -> Result<DiffResult>;

} // namespace ops
} // namespace repo
