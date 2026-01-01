# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

**IMPORTANT Git Commit Policy**: NEVER add co-authorship or generation attribution to commit messages. Do NOT include:
- "🤖 Generated with [Claude Code](https://claude.com/claude-code)"
- "Co-Authored-By: Claude <noreply@anthropic.com>"
- Any similar attribution or signature in commit messages

Keep commit messages clean and professional without AI attribution.

**For detailed architecture documentation**, see `repo-architecture.md` which contains comprehensive information about:
- Complete domain model specifications
- Full operation catalog with all parameters
- Git backend abstraction details
- CLI/TUI implementation patterns
- Plugin architecture
- Packaging and distribution strategies
- Implementation roadmap

## Project Overview

Repo is a modern Git interface built in C++23 that provides both a CLI and TUI in a **single unified binary**. The project aims to replace Git's complex UX with an intuitive, resource-oriented command structure while maintaining full compatibility with existing Git repositories through libgit2.

**Core Architecture**: Single source of truth with one library (librepo) powering two interfaces:
- **CLI**: Resource-oriented commands (e.g., `repo commit list`, `repo branch create`)
- **TUI**: Interactive interface using FTXUI (launch with `repo -i` or `repo tui`)

## Implementation Status

**Current Phase**: Phase 1-4 ✅ Complete (117 tests, 100% passing)

### ✅ Fully Implemented (117 tests, 100% passing)

**Infrastructure:**
- ✅ CMake build system with vcpkg integration
- ✅ Makefile (install, build, test targets)
- ✅ libgit2 backend abstraction with RAII wrappers
- ✅ Domain models (ObjectId, Signature, Commit, Branch, Reference, FileStatus, FileDiff, Stash, Tag, Remote)
- ✅ Error handling with `std::expected<T, Error>`
- ✅ Test infrastructure (TempRepo, CommitBuilder utilities)

**Core Operations Implemented (Phase 2):**
1. ✅ **status** - Full working tree status (7 tests)
2. ✅ **stage/unstage** - Index management (integrated into tests)
3. ✅ **commit** - Create commits with trees/parents (integrated)
4. ✅ **list_commits** - Commit history walking (6 tests)
5. ✅ **branch** - List, create, delete (6 tests)
6. ✅ **switch** - Branch switching (7 tests)
7. ✅ **restore** - Discard changes (6 tests)
8. ✅ **diff** - Working tree and staged diffs (8 tests)
9. ✅ **stash** - Create, list, apply, pop, drop (11 tests)
10. ✅ **remote** - Add, remove, list (7 tests)
11. ✅ **tag** - Create, list, delete (9 tests)
12. ✅ **select_commit** - Cherry-pick commits (5 tests)
13. ✅ **undo_commit** - Revert commits (6 tests)
14. ✅ **rollback** - Reset to previous commits (6 tests)

**CLI Layer (Phase 3) - 30 Commands:**
- ✅ **status** - Show working tree status
- ✅ **commit** - 5 subcommands (create, list, select, undo, rollback)
- ✅ **branch** - 4 subcommands (list, create, delete, switch)
- ✅ **file** - 3 subcommands (stage, unstage, restore)
- ✅ **stash** - 5 subcommands (create, list, apply, pop, drop)
- ✅ **remote** - 3 subcommands (list, add, remove)
- ✅ **tag** - 2 subcommands (list, delete)
- ✅ **Top-level shortcuts** - `stage`, `switch`
- ✅ **Help system** - Every command has `--help` with examples
- ✅ **Colored output** - Green/yellow/red/gray for different states

**TUI Layer (Phase 4):**
- ✅ **Interactive terminal UI** - Built with FTXUI (immediate-mode)
- ✅ **Status view** - File selection, stage/unstage, commit creation
- ✅ **Log view** - Commit history browsing (50 commits)
- ✅ **Branches view** - Branch listing and switching
- ✅ **Diff view** - Syntax-highlighted diffs for selected files
- ✅ **Help screen** - Comprehensive keyboard shortcut reference
- ✅ **Keyboard shortcuts** - Vim-inspired navigation
- ✅ **Single binary** - CLI + TUI in one executable (~12MB)

### ❌ Not Yet Implemented

**Advanced Operations (Phase 5):**
- Merge operations
- Rebase operations
- Fetch, push, pull (network operations)
- Branch rename, tracking configuration
- Commit amend
- Blame, clean, bisect

## Build System

### Dependencies
- C++23 compiler (GCC 14+ or Clang 18+ or Apple Clang 15+)
- CMake 3.25+
- libgit2 1.7.0+
- fmt 9.0.0+
- tomlplusplus (config parsing)
- CLI11 (CLI argument parsing)
- FTXUI (TUI framework - immediate-mode)
- Catch2 3.0.0+ (testing)

Dependencies are managed via vcpkg (see `vcpkg.json`).

### Build Commands

```bash
# Configure with vcpkg
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build build

# Build with tests
cmake -S . -B build -DREPO_BUILD_TESTS=ON
cmake --build build

# Run all tests
cd build && ctest

# Run specific test
./build/repo_tests "[test name]"

# Install
cmake --install build --prefix /usr/local
```

### Build Targets
- `librepo` - Core library (shared by CLI and TUI)
- `repo` - Single executable (contains both CLI and TUI)
- `repo_tests` - Test suite

## Code Architecture

### Layer Structure
The codebase follows a strict layered architecture:

```
┌─────────────────────────────────────┐
│   CLI Layer    │    TUI Layer       │  (User interfaces)
├────────────────┴────────────────────┤
│      Core Library (librepo)         │  (Business logic)
│  ┌───────────────────────────────┐  │
│  │    Operation Layer (ops::)    │  │  (Commands/Queries)
│  ├───────────────────────────────┤  │
│  │    Domain Model (domain::)    │  │  (Entities)
│  ├───────────────────────────────┤  │
│  │  Git Backend (backend::)      │  │  (libgit2 abstraction)
│  └───────────────────────────────┘  │
└─────────────────────────────────────┘
```

**Key Principle**: CLI and TUI are thin presentation layers. ALL business logic lives in `librepo`.

### Domain Model (`include/repo/domain/`)
Pure data structures representing Git concepts:
- `Commit`, `Branch`, `Remote`, `Tag` - Git objects
- `FileStatus`, `FileDiff`, `DiffHunk` - Working tree state
- `Index` - Staging area operations
- `ObjectId` - SHA-1 wrapper with parsing/formatting

Domain objects are immutable where possible and contain no libgit2 dependencies.

### Operation Layer (`include/repo/ops/`)
Stateless functions implementing Git workflows:
```cpp
namespace repo::ops {
    auto stage(Repository&, StageParams) -> Result<StageResult>;
    auto commit(Repository&, CommitParams) -> Result<CommitResult>;
    auto branch_create(Repository&, BranchCreateParams) -> Result<Branch>;
    // etc...
}
```

**Pattern**: All operations follow `auto operation(Repository&, Params) -> Result<Output>`.

Operations are the **only** place where Git backend interactions occur. They coordinate between domain objects and the backend abstraction.

### Git Backend Abstraction (`src/core/backend/`)
Wraps libgit2 behind the `GitBackend` interface. This allows:
- Unit testing with mock backends
- Potential future backends (dulwich, JGit, etc.)
- Isolation of libgit2 memory management

### Error Handling
Uses `std::expected<T, Error>` (C++23) for explicit error handling:
```cpp
template<typename T>
using Result = std::expected<T, Error>;

// Error codes are categorized (Repository, Reference, Network, etc.)
struct Error {
    Code code;
    std::string message;
    std::optional<std::string> detail;
    std::source_location location;
    std::unique_ptr<Error> cause;  // Error chaining
};
```

**NO exceptions in hot paths**. Only throw for truly exceptional cases (allocation failures, etc.).

## Command Taxonomy

Repo uses **resource-oriented** commands instead of Git's action-oriented verbs:

### Resource Domains
- `commit` - Commit operations (list, show, select, undo, reorder)
- `branch` - Branch management (create, delete, switch, merge, rebase)
- `file` - Working tree operations (stage, unstage, restore, diff, clean)
- `stash` - Stash operations (create, list, apply, pop, delete)
- `remote` - Remote operations (add, fetch, push, pull, sync)
- `tag` - Tag operations (create, list, delete, push)

### Top-Level Shortcuts
Daily workflow commands available without domain prefix:
- `repo status` - Working tree status
- `repo stage <files>` - Stage files (→ `repo file stage`)
- `repo commit` - Create commit
- `repo switch <branch>` - Switch branches (→ `repo branch switch`)
- `repo push/pull/fetch` - Remote operations (→ `repo remote push/pull/fetch`)

### Examples of Git vs Repo
```bash
# Git                          # Repo
git log                        repo commit list
git cherry-pick <hash>         repo commit select <hash>
git reset HEAD~1               repo commit undo --hard
git rebase -i HEAD~5           repo commit reorder HEAD~5
git checkout -b feat           repo branch create feat --switch
git branch -d old              repo branch delete old
```

## TUI Architecture

The TUI uses **FTXUI** (immediate-mode GUI pattern):
```
render() { /* rebuild UI every frame */ }
event_handler() { /* handle keyboard input */ }
```

### TUI Implementation (`src/tui/tui.cpp`)
Single-file TUI implementation (~700 lines) with:
- **AppState** - Central state container (files, branches, selections, etc.)
- **Render functions** - Pure functions: `AppState → Element`
  - `render_status()` - File list with staging states
  - `render_log()` - Commit history
  - `render_branches()` - Branch list
  - `render_diff_view()` - Syntax-highlighted diffs
  - `render_commit_dialog()` - Commit message input
  - `render_help()` - Keyboard shortcuts
- **Event handler** - Keyboard input routing
- **Main loop** - FTXUI screen loop with immediate-mode rendering

### Key Bindings
TUI operations map directly to core library operations:
- `Space` in Status → `ops::stage()` / `ops::unstage()`
- `c` in Status → `ops::commit()`
- `d` in Status → `ops::diff()`
- `Enter` in Branches → `ops::switch_branch()`
- `r` → Refresh (re-query operations)

This creates a **unified mental model** across CLI and TUI, as both call the same operation layer.

## Testing Strategy

### Test Categories
1. **Unit tests** (`tests/unit/`) - Domain objects, operation logic
2. **Integration tests** (`tests/integration/`) - Full workflows with real libgit2

All 117 tests focus on the core library (librepo). The CLI and TUI are thin presentation layers that call the tested operations.

### Test Utilities (`tests/test_utils.hpp`)
- `TempRepo` - Creates disposable Git repositories for testing
- `CommitBuilder` - Helper for creating test commits

### Running Tests
```bash
# All tests
ctest

# Specific test file
./build/repo_tests tests/unit/ops_test.cpp

# Specific test case
./build/repo_tests "[Stage operation]"

# With verbose output
./build/repo_tests -s
```

## Development Workflow

### Adding a New Operation
1. Define domain objects in `include/repo/domain/` if needed
2. Define operation signature in `include/repo/ops/<domain>.hpp`:
   ```cpp
   struct FooParams { /* ... */ };
   struct FooResult { /* ... */ };
   auto foo(Repository&, FooParams) -> Result<FooResult>;
   ```
3. Implement in `src/core/ops/<domain>.cpp` (coordinate domain + backend)
4. Add CLI command in `src/cli/commands/<domain>.cpp`
5. Wire up TUI keybinding if applicable in `src/tui/models/<view>.cpp`
6. Write tests in `tests/unit/ops_test.cpp` and `tests/integration/workflow_test.cpp`

### Adding a New CLI Command
1. Create command class inheriting from `repo::cli::Command`
2. Implement `name()`, `description()`, `setup()` (CLI11), `execute()`
3. Register in `src/cli/cli.cpp`
4. Add to command shortcuts if it's a top-level alias
5. Test in `tests/cli/command_test.cpp`

### Adding a TUI View
1. Define model struct in `src/tui/models/<view>.hpp` (TEA model)
2. Implement `view()` function (rendering) in `src/tui/render/<view>.cpp`
3. Implement `update()` function (state transitions) in `src/tui/models/<view>.cpp`
4. Wire into `AppModel` in `src/tui/app.cpp`
5. Add key bindings in `src/tui/program.cpp`
6. Test state transitions in `tests/tui/model_test.cpp`

## Directory Structure

```
repo/
├── include/repo/          # Public API headers
│   ├── domain/            # Domain objects
│   ├── ops/               # Operation signatures
│   └── config/            # Configuration
├── src/
│   ├── core/              # Core library implementation
│   │   ├── domain/        # Domain implementations
│   │   ├── ops/           # Operation implementations
│   │   └── backend/       # libgit2 abstraction
│   ├── cli/               # CLI layer
│   │   ├── commands/      # Command implementations
│   │   └── formatters/    # Output formatters (text/json/table)
│   ├── tui/               # TUI layer
│   │   ├── models/        # TEA models
│   │   └── render/        # View functions
│   └── main.cpp           # Entry point (dispatches to CLI or TUI)
└── tests/
    ├── unit/              # Unit tests
    ├── integration/       # Integration tests
    ├── cli/               # CLI tests
    └── tui/               # TUI tests
```

## Configuration System

Repo uses TOML for configuration with fallback to Git config:

**Priority order**:
1. `$REPO/.repo/config.toml` (repo-local)
2. `$XDG_CONFIG_HOME/repo/config.toml` (user)
3. `~/.config/repo/config.toml` (user fallback)
4. Git config `[repo]` section (integration)

**Key settings**:
- `ui.color` - Enable/disable colors
- `ui.tui.theme` - TUI color scheme (default, dracula, gruvbox)
- `defaults.pull_rebase` - Rebase by default on pull
- `defaults.push_autosetup_remote` - Auto-create tracking branches
- `aliases.*` - Command aliases

## Single Binary Philosophy

Unlike competitors (lazygit, gitui, tig), Repo ships **one binary** containing both CLI and TUI. Benefits:
- **One install** - Users get both interfaces
- **Shared code** - Smaller footprint, guaranteed consistency
- **Seamless transitions** - Launch TUI from CLI with `repo -i`
- **Same commands** - TUI command mode accepts CLI commands
- **Unified updates** - No version skew between interfaces

When working on this project, maintain parity between CLI and TUI wherever possible.

## Code Style

- Use C++23 features: `std::expected`, `std::span`, concepts, ranges
- Prefer `auto` for verbose types, explicit types for clarity
- Use trailing return types: `auto foo() -> Type`
- Domain objects are in `repo::domain` namespace
- Operations are in `repo::ops` namespace
- CLI is in `repo::cli` namespace
- TUI is in `repo::tui` namespace
- Test utilities are in `repo::test` namespace
- Mark non-throwing functions `noexcept` where appropriate
- Use `[[nodiscard]]` for functions returning results
- Prefer composition over inheritance
- Keep functions small and focused
- Document public APIs with Doxygen-style comments
