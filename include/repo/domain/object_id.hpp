#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "../result.hpp"

namespace repo::domain {

/// Git object ID (SHA-1 hash)
struct ObjectId {
    static constexpr size_t SIZE = 20;      // SHA-1 is 20 bytes
    static constexpr size_t HEX_SIZE = 40;  // 40 hex characters
    static constexpr size_t SHORT_SIZE = 7; // Abbreviated form

    std::array<std::byte, SIZE> bytes{};

    /// Convert to full 40-character hex string
    [[nodiscard]] auto to_string() const -> std::string;

    /// Convert to abbreviated hex string (default 7 chars)
    [[nodiscard]] auto to_short(size_t length = SHORT_SIZE) const -> std::string;

    /// Parse from hex string (full or abbreviated)
    [[nodiscard]] static auto from_string(std::string_view hex) -> Result<ObjectId>;

    /// Create zero (null) OID
    [[nodiscard]] static auto zero() -> ObjectId;

    /// Check if this is zero OID
    [[nodiscard]] auto is_zero() const -> bool;

    /// Equality comparison
    auto operator==(const ObjectId&) const -> bool = default;
    auto operator<=>(const ObjectId&) const = default;
};

} // namespace repo::domain

/// Hash support for std::unordered_map
template <> struct std::hash<repo::domain::ObjectId> {
    auto operator()(const repo::domain::ObjectId& oid) const noexcept -> size_t {
        // Hash first 8 bytes
        size_t result = 0;
        for (size_t i = 0; i < 8 && i < oid.bytes.size(); ++i) {
            result = (result << 8) | static_cast<uint8_t>(oid.bytes[i]);
        }
        return result;
    }
};
