#include <repo/domain/commit.hpp>

#include <algorithm>

namespace repo::domain {

auto Commit::summary() const -> std::string {
    auto first_newline = message.find('\n');
    if (first_newline == std::string::npos) {
        return message;
    }
    return message.substr(0, first_newline);
}

auto Commit::body() const -> std::string {
    auto first_newline = message.find('\n');
    if (first_newline == std::string::npos) {
        return "";
    }

    // Skip the first line and any blank lines after it
    auto start = first_newline + 1;
    while (start < message.size() && (message[start] == '\n' || message[start] == '\r')) {
        ++start;
    }

    if (start >= message.size()) {
        return "";
    }

    return message.substr(start);
}

} // namespace repo::domain
