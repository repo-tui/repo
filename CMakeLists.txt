cmake_minimum_required(VERSION 3.25)
project(repo
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "Modern Git interface with intuitive commands and integrated TUI"
)

# C++23 required
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(REPO_BUILD_TESTS "Build tests" ON)

# Find dependencies
find_package(libgit2 CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)

if(REPO_BUILD_TESTS)
    find_package(Catch2 3 CONFIG REQUIRED)
    enable_testing()
endif()

# CLI and TUI dependencies (always enabled)
find_package(CLI11 CONFIG REQUIRED)
find_package(ftxui CONFIG REQUIRED)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Core library
add_library(librepo
    # Domain objects
    src/core/domain/object_id.cpp
    src/core/domain/signature.cpp
    src/core/domain/commit.cpp
    src/core/domain/branch.cpp
    src/core/domain/reference.cpp
    src/core/domain/file_status.cpp
    src/core/domain/diff.cpp
    src/core/domain/stash.cpp
    src/core/domain/remote.cpp
    src/core/domain/tag.cpp
    src/core/domain/index.cpp

    # Error handling
    src/core/error.cpp

    # Repository
    src/core/repository.cpp

    # Backend abstraction
    src/core/backend/git_backend.cpp
    src/core/backend/libgit2_backend.cpp
    src/core/backend/credential_helper.cpp
    src/core/backend/subprocess_utils.cpp
    src/core/backend/interactive_prompt.cpp
    src/core/backend/ssh_key_discovery.cpp
    src/core/backend/oauth_device_flow.cpp
    src/core/backend/auth_error_formatter.cpp

    # Operations
    src/core/ops/status.cpp
    src/core/ops/stage.cpp
    src/core/ops/commit.cpp
    src/core/ops/diff.cpp
    src/core/ops/branch.cpp
    src/core/ops/switch.cpp
    src/core/ops/restore.cpp
    src/core/ops/merge.cpp
    src/core/ops/list_commits.cpp
    src/core/ops/remote.cpp
    src/core/ops/stash.cpp
    src/core/ops/tag.cpp
    src/core/ops/rollback.cpp
    src/core/ops/select_commit.cpp
    src/core/ops/undo_commit.cpp
    src/core/ops/clean.cpp
    src/core/ops/rebase.cpp
    src/core/ops/init.cpp

    # Init templates - Languages
    src/core/ops/init/templates/languages/cpp.cpp
    src/core/ops/init/templates/languages/python.cpp
    src/core/ops/init/templates/languages/javascript.cpp
    src/core/ops/init/templates/languages/java.cpp
    src/core/ops/init/templates/languages/go.cpp
    src/core/ops/init/templates/languages/rust.cpp
    src/core/ops/init/templates/languages/csharp.cpp
    src/core/ops/init/templates/languages/ruby.cpp
    src/core/ops/init/templates/languages/php.cpp
    src/core/ops/init/templates/languages/swift.cpp
    src/core/ops/init/templates/languages/kotlin.cpp

    # Init templates - Build Systems
    src/core/ops/init/templates/build_systems/cmake.cpp
    src/core/ops/init/templates/build_systems/make.cpp
    src/core/ops/init/templates/build_systems/vcpkg.cpp
    src/core/ops/init/templates/build_systems/conan.cpp
    src/core/ops/init/templates/build_systems/gradle.cpp
    src/core/ops/init/templates/build_systems/maven.cpp

    # Init templates - Platforms
    src/core/ops/init/templates/platforms/macos.cpp
    src/core/ops/init/templates/platforms/linux.cpp
    src/core/ops/init/templates/platforms/windows.cpp

    # Init templates - Editors
    src/core/ops/init/templates/editors/vscode.cpp
    src/core/ops/init/templates/editors/jetbrains.cpp
    src/core/ops/init/templates/editors/vim.cpp
)

target_include_directories(librepo
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(librepo
    PUBLIC
        libgit2::libgit2package
        fmt::fmt
        tomlplusplus::tomlplusplus
)

# Set output name to just "repo" (not "librepo")
set_target_properties(librepo PROPERTIES OUTPUT_NAME repo)

# Tests
if(REPO_BUILD_TESTS)
    add_executable(repo_tests
        tests/test_main.cpp
        tests/test_utils.cpp

        # Unit tests
        tests/unit/domain/object_id_test.cpp
        tests/unit/domain/signature_test.cpp
        tests/unit/domain/commit_test.cpp
        tests/unit/domain/branch_test.cpp
        tests/unit/domain/file_status_test.cpp
        tests/unit/domain/diff_test.cpp

        tests/unit/error_test.cpp
        tests/unit/repository_test.cpp

        tests/unit/ops/status_test.cpp
        tests/unit/ops/stage_test.cpp
        tests/unit/ops/commit_test.cpp
        tests/unit/ops/diff_test.cpp
        tests/unit/ops/branch_test.cpp
        tests/unit/ops/merge_test.cpp

        # Backend tests
        tests/unit/backend/credential_helper_test.cpp
        tests/unit/backend/ssh_key_discovery_test.cpp
        tests/unit/backend/oauth_device_flow_test.cpp

        # Integration tests
        tests/integration/status_test.cpp
        tests/integration/diff_test.cpp
        tests/integration/switch_test.cpp
        tests/integration/restore_test.cpp
        tests/integration/remote_test.cpp
        tests/integration/tag_test.cpp
        tests/integration/stash_test.cpp
        tests/integration/rollback_test.cpp
        tests/integration/select_commit_test.cpp
        tests/integration/undo_commit_test.cpp
        tests/integration/branch_ops_test.cpp
        tests/integration/list_commits_test.cpp
        tests/integration/workflow_test.cpp
        tests/integration/branch_workflow_test.cpp
        tests/integration/merge_workflow_test.cpp
    )

    target_link_libraries(repo_tests
        PRIVATE
            librepo
            Catch2::Catch2WithMain
    )

    # Register tests with CTest
    include(CTest)
    include(Catch)
    catch_discover_tests(repo_tests)
endif()

# Main executable (CLI + TUI in single binary)
add_executable(repo-cli
    src/cli/main.cpp
    # src/tui/tui.cpp  # Old TUI implementation (replaced by tui_tea.cpp)
    src/tui/tui_tea.cpp  # New TEA-based TUI implementation

    # TEA infrastructure
    src/tui/tea/program.cpp

    # Rendering infrastructure
    src/tui/render/style.cpp
    src/tui/render/primitives.cpp

    # Models
    src/tui/models/status.cpp
    src/tui/models/log.cpp
    src/tui/models/diff.cpp
    src/tui/models/branch.cpp
    src/tui/models/remote.cpp
    src/tui/models/stash.cpp
    src/tui/models/command.cpp
    src/tui/models/app.cpp

    # Views
    src/tui/views/status.cpp
    src/tui/views/log.cpp
    src/tui/views/diff.cpp
    src/tui/views/branch.cpp
    src/tui/views/remote.cpp
    src/tui/views/stash.cpp
    src/tui/views/command.cpp
    src/tui/views/app.cpp
)

target_link_libraries(repo-cli
    PRIVATE
        librepo
        CLI11::CLI11
        ftxui::screen
        ftxui::dom
        ftxui::component
)

# Set output name to just "repo"
set_target_properties(repo-cli PROPERTIES OUTPUT_NAME repo)

# Installation
install(TARGETS librepo
    EXPORT repoTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(TARGETS repo-cli
    RUNTIME DESTINATION bin
    COMPONENT runtime
)

install(DIRECTORY include/repo
    DESTINATION include
)
