#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "msg.hpp"

namespace repo::tui::tea {

// Command - represents a deferred effect that will produce a message
// This is move-only since it wraps a function
using Cmd = std::function<std::optional<Msg>()>;

// Batch of commands
using CmdBatch = std::vector<Cmd>;

// Helper: Create a command that produces no message
inline auto none() -> CmdBatch {
    return {};
}

// Helper: Create a single command
inline auto cmd(Cmd c) -> CmdBatch {
    CmdBatch batch;
    batch.push_back(std::move(c));
    return batch;
}

// Helper: Create a batch of commands
inline auto batch(CmdBatch cmds) -> CmdBatch {
    return cmds;
}

// Helper: Create a command from a message (immediate)
inline auto just(Msg msg) -> CmdBatch {
    return cmd([m = std::move(msg)]() -> std::optional<Msg> { return m; });
}

// Helper: Create a command from an async operation
template <typename F> auto async(F&& f) -> CmdBatch {
    return cmd([func = std::forward<F>(f)]() -> std::optional<Msg> { return func(); });
}

// Helper: Combine multiple command batches
inline auto combine(CmdBatch a, CmdBatch b) -> CmdBatch {
    CmdBatch result = std::move(a);
    result.insert(result.end(), std::make_move_iterator(b.begin()),
                  std::make_move_iterator(b.end()));
    return result;
}

// Helper: Combine multiple command batches (variadic)
template <typename... Batches> auto combine(CmdBatch first, Batches&&... rest) -> CmdBatch {
    return combine(std::move(first), combine(std::forward<Batches>(rest)...));
}

} // namespace repo::tui::tea
