#pragma once

#include <memory>
#include <optional>
#include <source_location>
#include <string>

namespace repo {

/// Error codes categorized by domain
struct Error {
    enum class Code {
        // Generic errors (0x0000)
        Unknown = 0x0000,
        InvalidArgument,
        NotImplemented,

        // Repository errors (0x1000)
        NotARepository = 0x1000,
        RepositoryCorrupted,
        BareRepository,
        AlreadyExists,

        // Reference errors (0x2000)
        ReferenceNotFound = 0x2000,
        ReferenceAlreadyExists,
        InvalidReference,
        DetachedHead,

        // Index errors (0x3000)
        IndexLocked = 0x3000,
        ConflictPresent,
        IndexCorrupted,

        // Network errors (0x4000)
        NetworkUnreachable = 0x4000,
        AuthenticationFailed,
        RemoteNotFound,
        ServerError,

        // Object errors (0x5000)
        ObjectNotFound = 0x5000,
        ObjectCorrupted,
        InvalidObjectType,

        // Operation errors (0x6000)
        MergeConflict = 0x6000,
        MergeError,
        RebaseConflict,
        DirtyWorkTree,
        NothingToCommit,
        UnmergedEntries,
        NotOnBranch,
        CommitError,
        ReferenceError,

        // File system errors (0x7000)
        FileNotFound = 0x7000,
        PermissionDenied,
        PathAlreadyExists,
    };

    Code code;
    std::string message;
    std::optional<std::string> detail;
    std::source_location location;

    // Error chaining for root cause analysis
    std::unique_ptr<Error> cause;

    // Custom copy constructor for deep copying the error chain
    Error(const Error& other)
        : code(other.code), message(other.message), detail(other.detail), location(other.location),
          cause(other.cause ? std::make_unique<Error>(*other.cause) : nullptr) {}

    // Custom copy assignment
    auto operator=(const Error& other) -> Error& {
        if (this != &other) {
            code = other.code;
            message = other.message;
            detail = other.detail;
            location = other.location;
            cause = other.cause ? std::make_unique<Error>(*other.cause) : nullptr;
        }
        return *this;
    }

    // Default move operations
    Error(Error&&) noexcept = default;
    auto operator=(Error&&) noexcept -> Error& = default;

    // Default constructor
    Error() = default;

    /// Format error as single line
    [[nodiscard]] auto format() const -> std::string;

    /// Format error with full chain
    [[nodiscard]] auto format_chain() const -> std::string;

    /// Get category name from code
    [[nodiscard]] static auto category_name(Code code) -> std::string_view;

    /// Get code name
    [[nodiscard]] static auto code_name(Code code) -> std::string_view;
};

/// Helper to create error with source location
[[nodiscard]] inline auto
make_error(Error::Code code, std::string message,
           std::source_location location = std::source_location::current()) -> Error {
    Error err;
    err.code = code;
    err.message = std::move(message);
    err.location = location;
    return err;
}

/// Helper to create error with detail
[[nodiscard]] inline auto
make_error(Error::Code code, std::string message, std::string detail,
           std::source_location location = std::source_location::current()) -> Error {
    Error err;
    err.code = code;
    err.message = std::move(message);
    err.detail = std::move(detail);
    err.location = location;
    return err;
}

/// Helper to chain errors
[[nodiscard]] inline auto
make_error(Error::Code code, std::string message, Error cause,
           std::source_location location = std::source_location::current()) -> Error {
    Error err;
    err.code = code;
    err.message = std::move(message);
    err.cause = std::make_unique<Error>(std::move(cause));
    err.location = location;
    return err;
}

} // namespace repo
