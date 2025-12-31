#pragma once

#include <repo/repository.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace repo::test {

/// Temporary directory that cleans itself up
class TempDir {
  public:
    TempDir();
    ~TempDir();

    // Non-copyable, movable
    TempDir(const TempDir&) = delete;
    auto operator=(const TempDir&) -> TempDir& = delete;
    TempDir(TempDir&&) noexcept = default;
    auto operator=(TempDir&&) noexcept -> TempDir& = default;

    [[nodiscard]] auto path() const -> const std::filesystem::path&;

  private:
    std::filesystem::path path_;
};

/// Temporary Git repository for testing
class TempRepo {
  public:
    /// Create temp repo and initialize it
    TempRepo();

    /// Create temp repo with specific init options
    explicit TempRepo(bool bare);

    ~TempRepo() = default;

    // Non-copyable, movable
    TempRepo(const TempRepo&) = delete;
    auto operator=(const TempRepo&) -> TempRepo& = delete;
    TempRepo(TempRepo&&) noexcept = default;
    auto operator=(TempRepo&&) noexcept -> TempRepo& = default;

    [[nodiscard]] auto path() const -> const std::filesystem::path&;
    [[nodiscard]] auto repo() -> Repository&;

    // Helper methods
    auto write_file(const std::filesystem::path& relative_path, std::string_view content) -> void;
    auto read_file(const std::filesystem::path& relative_path) -> std::string;
    auto delete_file(const std::filesystem::path& relative_path) -> void;
    auto file_exists(const std::filesystem::path& relative_path) -> bool;

    /// Run git command (for setup/verification)
    auto run_git(const std::vector<std::string>& args) -> std::string;

  private:
    TempDir temp_dir_;
    std::optional<Repository> repo_;
};

/// Helper to create commits for testing
class CommitBuilder {
  public:
    explicit CommitBuilder(TempRepo& r) : repo_(r) {}

    auto with_file(const std::filesystem::path& path, std::string_view content) -> CommitBuilder&;
    auto with_message(std::string_view msg) -> CommitBuilder&;
    auto create() -> domain::ObjectId;

  private:
    TempRepo& repo_;
    std::vector<std::pair<std::filesystem::path, std::string>> files_;
    std::string message_ = "Test commit";
};

} // namespace repo::test
