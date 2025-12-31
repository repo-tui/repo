#pragma once

#include <repo/error.hpp>
#include <repo/result.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace repo::ops {

// Project types for .gitignore template generation
enum class ProjectType {
    // Languages
    Cpp,
    Python,
    JavaScript, // Covers Node, React, Vue, Bun, Deno, TypeScript
    Java,
    Go,
    Rust,
    CSharp,
    Ruby,
    PHP,
    Swift,
    Kotlin,

    // Build Systems (C++)
    CMake,
    Make,
    Vcpkg,
    Conan,

    // Build Systems (Java)
    Gradle,
    Maven,

    // Platforms
    macOS,
    Linux,
    Windows,

    // Editors
    VSCode,
    JetBrains,
    Vim
};

// Parameters for repository initialization
struct InitParams {
    std::filesystem::path path = ".";
    bool bare = false;
    bool interactive = false;
    std::vector<ProjectType> project_types;
    std::optional<std::string> initial_branch_name;
};

// Result of repository initialization
struct InitResult {
    std::filesystem::path git_dir;
    bool created_gitignore = false;
    std::vector<ProjectType> applied_templates;
};

// Initialize a new Git repository
auto init(const InitParams& params) -> Result<InitResult>;

// Helper: Get human-readable name for project type
auto project_type_name(ProjectType type) -> std::string;

// Helper: Get project type from string (for CLI parsing)
auto parse_project_type(const std::string& str) -> std::optional<ProjectType>;

// Helper: Get all available project types grouped by category
struct ProjectTypeCategory {
    std::string name;
    std::vector<ProjectType> types;
};

auto get_project_type_categories() -> std::vector<ProjectTypeCategory>;

// Helper: Check if build system is relevant for given languages
auto is_build_system_relevant(ProjectType build_system, const std::vector<ProjectType>& languages)
    -> bool;

} // namespace repo::ops
