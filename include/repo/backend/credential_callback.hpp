#pragma once

#include "credential.hpp"
#include "../result.hpp"

#include <functional>
#include <memory>
#include <unordered_map>

namespace repo::backend {

/// Credential acquisition callback
/// Called by backend when authentication is required
/// Returns credential to use, or error if authentication should fail
using CredentialCallback = std::function<Result<Credential>(const AuthenticationContext& context)>;

/// Manages authentication attempt tracking to prevent infinite loops
class AuthenticationTracker {
  public:
    /// Maximum authentication attempts per URL before giving up
    static constexpr int MAX_ATTEMPTS = 3;

    /// Record an authentication attempt for a URL
    auto record_attempt(const std::string& url) -> int {
        return ++attempt_counts_[url];
    }

    /// Get current attempt count for a URL
    [[nodiscard]] auto get_attempt_count(const std::string& url) const -> int {
        auto it = attempt_counts_.find(url);
        return it != attempt_counts_.end() ? it->second : 0;
    }

    /// Reset attempt count for a URL (call on successful authentication)
    auto reset(const std::string& url) -> void {
        attempt_counts_.erase(url);
    }

    /// Clear all tracked attempts
    auto clear() -> void {
        attempt_counts_.clear();
    }

    /// Check if maximum attempts reached for a URL
    [[nodiscard]] auto is_max_attempts_reached(const std::string& url) const -> bool {
        return get_attempt_count(url) >= MAX_ATTEMPTS;
    }

  private:
    std::unordered_map<std::string, int> attempt_counts_;
};

/// Base class for authentication strategies
class AuthenticationStrategy {
  public:
    virtual ~AuthenticationStrategy() = default;

    /// Attempt to acquire credentials using this strategy
    /// Returns credential on success, or error if strategy cannot provide credentials
    [[nodiscard]] virtual auto acquire(const AuthenticationContext& context)
        -> Result<Credential> = 0;

    /// Get the name of this strategy (for logging/debugging)
    [[nodiscard]] virtual auto name() const -> std::string = 0;

    /// Check if this strategy can handle the given authentication context
    [[nodiscard]] virtual auto can_handle(const AuthenticationContext& context) const -> bool = 0;
};

} // namespace repo::backend
