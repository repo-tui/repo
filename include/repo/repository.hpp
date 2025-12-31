#pragma once

#include <filesystem>
#include <memory>

#include "backend/git_backend.hpp"
#include "domain/reference.hpp"
#include "result.hpp"

namespace repo {

/// Options for initializing a repository
struct InitOptions {
    bool bare = false;
};

/// Main repository class
class Repository {
  public:
    /// Open an existing repository
    [[nodiscard]] static auto open(const std::filesystem::path& path) -> Result<Repository>;

    /// Initialize a new repository
    [[nodiscard]] static auto init(const std::filesystem::path& path, InitOptions opts = {})
        -> Result<Repository>;

    // Repository state queries
    [[nodiscard]] auto path() const -> const std::filesystem::path&;
    [[nodiscard]] auto git_dir() const -> std::filesystem::path;
    [[nodiscard]] auto workdir() const -> std::filesystem::path;
    [[nodiscard]] auto is_bare() const -> bool;

    // Reference operations
    [[nodiscard]] auto head() const -> Result<domain::Reference>;

    // Access to backend (for operations layer)
    [[nodiscard]] auto backend() -> backend::GitBackend&;
    [[nodiscard]] auto backend() const -> const backend::GitBackend&;
    [[nodiscard]] auto repo_handle() -> backend::RepoHandle&;
    [[nodiscard]] auto repo_handle() const -> const backend::RepoHandle&;

    // Movable but not copyable
    Repository(Repository&&) noexcept = default;
    auto operator=(Repository&&) noexcept -> Repository& = default;
    Repository(const Repository&) = delete;
    auto operator=(const Repository&) -> Repository& = delete;

  private:
    Repository(std::filesystem::path path, std::unique_ptr<backend::GitBackend> backend,
               std::unique_ptr<backend::RepoHandle> handle);

    std::filesystem::path path_;
    std::unique_ptr<backend::GitBackend> backend_;
    std::unique_ptr<backend::RepoHandle> repo_handle_;
};

} // namespace repo
