#include <repo/domain/object_id.hpp>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace repo::domain {

namespace {
// Convert hex character to value
auto hex_to_byte(char c) -> int {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

// Check if character is valid hex
auto is_hex(char c) -> bool {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
} // namespace

auto ObjectId::to_string() const -> std::string {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (const auto byte : bytes) {
        oss << std::setw(2) << static_cast<unsigned>(byte);
    }
    return oss.str();
}

auto ObjectId::to_short(size_t length) const -> std::string {
    auto full = to_string();
    return full.substr(0, std::min(length, full.size()));
}

auto ObjectId::from_string(std::string_view hex) -> Result<ObjectId> {
    // Validate length (either full 40 chars or abbreviated >= 7 chars)
    static constexpr size_t MIN_HEX_SIZE = 7; // Minimum for abbreviated OID (SHORT_SIZE)

    if (hex.empty()) {
        return std::unexpected(
            make_error(Error::Code::InvalidArgument, "ObjectId hex string cannot be empty"));
    }

    if (hex.size() < MIN_HEX_SIZE) {
        return std::unexpected(make_error(Error::Code::InvalidArgument,
                                          "ObjectId hex string too short",
                                          "Expected at least " + std::to_string(MIN_HEX_SIZE) +
                                              " characters, got " + std::to_string(hex.size())));
    }

    if (hex.size() > HEX_SIZE) {
        return std::unexpected(make_error(Error::Code::InvalidArgument,
                                          "ObjectId hex string too long",
                                          "Expected at most " + std::to_string(HEX_SIZE) +
                                              " characters, got " + std::to_string(hex.size())));
    }

    // Validate all characters are hex
    if (!std::all_of(hex.begin(), hex.end(), is_hex)) {
        return std::unexpected(make_error(Error::Code::InvalidArgument,
                                          "ObjectId hex string contains invalid characters",
                                          "Only characters 0-9, a-f, A-F are allowed"));
    }

    ObjectId oid;

    // Parse hex string into bytes
    size_t hex_idx = 0;
    for (size_t byte_idx = 0; byte_idx < SIZE && hex_idx < hex.size(); ++byte_idx) {
        int high = hex_to_byte(hex[hex_idx++]);
        int low = 0;

        if (hex_idx < hex.size()) {
            low = hex_to_byte(hex[hex_idx++]);
        }

        oid.bytes[byte_idx] = static_cast<std::byte>((high << 4) | low);
    }

    // If abbreviated, remaining bytes are already zero

    return oid;
}

auto ObjectId::zero() -> ObjectId {
    ObjectId oid;
    // bytes are already zero-initialized
    return oid;
}

auto ObjectId::is_zero() const -> bool {
    return std::all_of(bytes.begin(), bytes.end(), [](std::byte b) { return b == std::byte{0}; });
}

} // namespace repo::domain
