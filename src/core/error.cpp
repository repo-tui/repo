#include <repo/error.hpp>

#include <fmt/format.h>

#include <sstream>

namespace repo {

auto Error::format() const -> std::string {
    std::string result = fmt::format("[{}] {}", code_name(code), message);
    if (detail) {
        result += fmt::format(": {}", *detail);
    }
    return result;
}

auto Error::format_chain() const -> std::string {
    std::ostringstream oss;
    oss << format();

    if (location.file_name() != nullptr) {
        oss << fmt::format("\n  at {}:{}:{}", location.file_name(), location.line(),
                           location.column());
    }

    if (cause) {
        oss << "\nCaused by:\n  " << cause->format_chain();
    }

    return oss.str();
}

auto Error::category_name(Code code) -> std::string_view {
    auto code_val = static_cast<int>(code);

    if (code_val < 0x1000)
        return "Generic";
    if (code_val < 0x2000)
        return "Repository";
    if (code_val < 0x3000)
        return "Reference";
    if (code_val < 0x4000)
        return "Index";
    if (code_val < 0x5000)
        return "Network";
    if (code_val < 0x6000)
        return "Object";
    if (code_val < 0x7000)
        return "Operation";
    if (code_val < 0x8000)
        return "FileSystem";

    return "Unknown";
}

auto Error::code_name(Code code) -> std::string_view {
    switch (code) {
        // Generic
        case Code::Unknown:
            return "Unknown";
        case Code::InvalidArgument:
            return "InvalidArgument";
        case Code::NotImplemented:
            return "NotImplemented";

        // Repository
        case Code::NotARepository:
            return "NotARepository";
        case Code::RepositoryCorrupted:
            return "RepositoryCorrupted";
        case Code::BareRepository:
            return "BareRepository";
        case Code::AlreadyExists:
            return "AlreadyExists";

        // Reference
        case Code::ReferenceNotFound:
            return "ReferenceNotFound";
        case Code::ReferenceAlreadyExists:
            return "ReferenceAlreadyExists";
        case Code::InvalidReference:
            return "InvalidReference";
        case Code::DetachedHead:
            return "DetachedHead";

        // Index
        case Code::IndexLocked:
            return "IndexLocked";
        case Code::ConflictPresent:
            return "ConflictPresent";
        case Code::IndexCorrupted:
            return "IndexCorrupted";

        // Network
        case Code::NetworkUnreachable:
            return "NetworkUnreachable";
        case Code::AuthenticationFailed:
            return "AuthenticationFailed";
        case Code::RemoteNotFound:
            return "RemoteNotFound";
        case Code::ServerError:
            return "ServerError";

        // Object
        case Code::ObjectNotFound:
            return "ObjectNotFound";
        case Code::ObjectCorrupted:
            return "ObjectCorrupted";
        case Code::InvalidObjectType:
            return "InvalidObjectType";

        // Operation
        case Code::MergeConflict:
            return "MergeConflict";
        case Code::RebaseConflict:
            return "RebaseConflict";
        case Code::DirtyWorkTree:
            return "DirtyWorkTree";
        case Code::NothingToCommit:
            return "NothingToCommit";
        case Code::UnmergedEntries:
            return "UnmergedEntries";
        case Code::NotOnBranch:
            return "NotOnBranch";

        // FileSystem
        case Code::FileNotFound:
            return "FileNotFound";
        case Code::PermissionDenied:
            return "PermissionDenied";
        case Code::PathAlreadyExists:
            return "PathAlreadyExists";

        default:
            return "Unknown";
    }
}

} // namespace repo
