#pragma once

#include <repo/domain/branch.hpp>
#include <repo/domain/commit.hpp>
#include <repo/domain/diff.hpp>
#include <repo/domain/file_status.hpp>
#include <repo/domain/remote.hpp>
#include <repo/domain/stash.hpp>
#include <repo/error.hpp>

#include <chrono>
#include <filesystem>
#include <string>
#include <variant>
#include <vector>

namespace repo::tui::tea {

// Keyboard input message
struct KeyMsg {
    enum class Type {
        Character,
        ArrowUp,
        ArrowDown,
        ArrowLeft,
        ArrowRight,
        Enter,
        Escape,
        Tab,
        Backspace,
        Delete,
        Home,
        End,
        PageUp,
        PageDown,
        F1,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        F11,
        F12
    };

    Type type;
    char character = '\0'; // Only for Character type
    bool ctrl = false;
    bool alt = false;
    bool shift = false;

    static auto from_char(char c) -> KeyMsg { return KeyMsg{Type::Character, c}; }

    static auto arrow_up() -> KeyMsg { return KeyMsg{Type::ArrowUp}; }
    static auto arrow_down() -> KeyMsg { return KeyMsg{Type::ArrowDown}; }
    static auto arrow_left() -> KeyMsg { return KeyMsg{Type::ArrowLeft}; }
    static auto arrow_right() -> KeyMsg { return KeyMsg{Type::ArrowRight}; }
    static auto enter() -> KeyMsg { return KeyMsg{Type::Enter}; }
    static auto escape() -> KeyMsg { return KeyMsg{Type::Escape}; }
    static auto tab() -> KeyMsg { return KeyMsg{Type::Tab}; }
    static auto backspace() -> KeyMsg { return KeyMsg{Type::Backspace}; }
    static auto del() -> KeyMsg { return KeyMsg{Type::Delete}; }
};

// Mouse input message
struct MouseMsg {
    enum class Button { Left, Right, Middle, WheelUp, WheelDown };
    enum class Action { Press, Release, Move };

    Button button;
    Action action;
    int x, y;
};

// Window resize message
struct WindowSizeMsg {
    int width, height;
};

// Timer tick message
struct TickMsg {
    std::chrono::steady_clock::time_point time;
};

// Status operation results
struct StatusLoadedMsg {
    std::vector<domain::FileStatus> files;
};

struct StatusErrorMsg {
    Error error;
};

// Log operation results
struct LogLoadedMsg {
    std::vector<domain::Commit> commits;
    bool has_more;
};

struct LogErrorMsg {
    Error error;
};

// Diff operation results
struct DiffLoadedMsg {
    std::vector<domain::FileDiff> diffs;
};

struct DiffErrorMsg {
    Error error;
};

// Branch operation results
struct BranchesLoadedMsg {
    std::vector<domain::Branch> branches;
};

struct BranchCreatedMsg {
    domain::Branch branch;
};

struct BranchDeletedMsg {
    std::string name;
};

struct BranchSwitchedMsg {
    std::string name;
};

struct BranchErrorMsg {
    Error error;
};

// Remote operation results
struct RemotesLoadedMsg {
    std::vector<domain::Remote> remotes;
};

struct FetchProgressMsg {
    std::string remote;
    size_t received_objects;
    size_t total_objects;
    size_t received_bytes;
};

struct FetchCompletedMsg {
    std::string remote;
    std::vector<std::string> updated_refs;
};

struct PushProgressMsg {
    std::string remote;
    size_t sent_objects;
    size_t total_objects;
};

struct PushCompletedMsg {
    std::string remote;
    std::vector<std::string> updated_refs;
};

struct PullCompletedMsg {
    std::string remote_name;
    size_t received_objects;
    size_t received_bytes;
    std::string merge_type;
};

struct RemoteAddedMsg {
    std::string name;
};

struct RemoteRemovedMsg {
    std::string name;
};

struct RemoteErrorMsg {
    Error error;
};

// Stash operation results
struct StashesLoadedMsg {
    std::vector<domain::Stash> stashes;
};

struct StashCreatedMsg {
    domain::ObjectId stash_id;
};

struct StashAppliedMsg {
    size_t index;
};

struct StashPoppedMsg {
    size_t index;
};

struct StashDroppedMsg {
    size_t index;
};

struct StashErrorMsg {
    Error error;
};

// File operation results
struct FileStagedMsg {
    std::filesystem::path path;
};

struct FileUnstagedMsg {
    std::filesystem::path path;
};

struct CommitCreatedMsg {
    domain::ObjectId commit_id;
    std::string short_id;
};

struct FileOperationErrorMsg {
    Error error;
    std::filesystem::path path;
};

// Filesystem watcher
struct FileChangedMsg {
    std::filesystem::path path;
};

// Generic error
struct ErrorMsg {
    Error error;
    std::string context;
};

// Notification
struct NotificationMsg {
    std::string message;
    enum class Level { Info, Success, Warning, Error };
    Level level;
};

// Command executed
struct CommandExecutedMsg {
    std::string command;
    int exit_code;
    std::string output;
};

// The message sum type
using Msg = std::variant<
    // Input
    KeyMsg, MouseMsg, WindowSizeMsg, TickMsg,

    // Status operations
    StatusLoadedMsg, StatusErrorMsg,

    // Log operations
    LogLoadedMsg, LogErrorMsg,

    // Diff operations
    DiffLoadedMsg, DiffErrorMsg,

    // Branch operations
    BranchesLoadedMsg, BranchCreatedMsg, BranchDeletedMsg, BranchSwitchedMsg, BranchErrorMsg,

    // Remote operations
    RemotesLoadedMsg, RemoteAddedMsg, RemoteRemovedMsg, FetchProgressMsg, FetchCompletedMsg,
    PushProgressMsg, PushCompletedMsg, PullCompletedMsg, RemoteErrorMsg,

    // Stash operations
    StashesLoadedMsg, StashCreatedMsg, StashAppliedMsg, StashPoppedMsg, StashDroppedMsg,
    StashErrorMsg,

    // File operations
    FileStagedMsg, FileUnstagedMsg, CommitCreatedMsg, FileOperationErrorMsg,

    // Filesystem
    FileChangedMsg,

    // Generic
    ErrorMsg, NotificationMsg, CommandExecutedMsg>;

} // namespace repo::tui::tea
