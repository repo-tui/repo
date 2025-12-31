#pragma once

#include <expected>

#include "error.hpp"

namespace repo {

/// Result type wrapping std::expected with our Error type
template <typename T> using Result = std::expected<T, Error>;

/// Result type for void operations
using Status = Result<void>;

} // namespace repo
