#pragma once

#include <chrono>
#include <string>

namespace repo::domain {

/// Git signature (author or committer)
struct Signature {
    std::string name;
    std::string email;
    std::chrono::system_clock::time_point when;
    std::chrono::minutes tz_offset{0};

    /// Format as "Name <email> timestamp tz"
    [[nodiscard]] auto format() const -> std::string;

    /// Format just the "Name <email>" part
    [[nodiscard]] auto format_name_email() const -> std::string;

    /// Format timestamp with timezone
    [[nodiscard]] auto format_time() const -> std::string;

    auto operator==(const Signature&) const -> bool = default;
};

} // namespace repo::domain
