#include <repo/backend/libgit2_backend.hpp>
#include <repo/repository.hpp>

namespace repo {

Repository::Repository(std::filesystem::path path, std::unique_ptr<backend::GitBackend> backend,
                       std::unique_ptr<backend::RepoHandle> handle)
    : path_(std::move(path)), backend_(std::move(backend)), repo_handle_(std::move(handle)) {}

auto Repository::open(const std::filesystem::path& path) -> Result<Repository> {
    // Create backend
    auto backend = std::make_unique<backend::LibGit2Backend>();

    // Open repository
    auto handle_result = backend->open(path);
    if (!handle_result) {
        return std::unexpected(std::move(handle_result.error()));
    }

    return Repository(path, std::move(backend), std::move(*handle_result));
}

auto Repository::init(const std::filesystem::path& path, InitOptions opts) -> Result<Repository> {
    // Create backend
    auto backend = std::make_unique<backend::LibGit2Backend>();

    // Initialize repository
    auto handle_result = backend->init(path, opts.bare);
    if (!handle_result) {
        return std::unexpected(std::move(handle_result.error()));
    }

    return Repository(path, std::move(backend), std::move(*handle_result));
}

auto Repository::path() const -> const std::filesystem::path& {
    return path_;
}

auto Repository::git_dir() const -> std::filesystem::path {
    return backend_->git_dir(*repo_handle_);
}

auto Repository::workdir() const -> std::filesystem::path {
    return backend_->workdir(*repo_handle_);
}

auto Repository::is_bare() const -> bool {
    return backend_->is_bare(*repo_handle_);
}

auto Repository::head() const -> Result<domain::Reference> {
    return backend_->get_head(*repo_handle_);
}

auto Repository::backend() -> backend::GitBackend& {
    return *backend_;
}

auto Repository::backend() const -> const backend::GitBackend& {
    return *backend_;
}

auto Repository::repo_handle() -> backend::RepoHandle& {
    return *repo_handle_;
}

auto Repository::repo_handle() const -> const backend::RepoHandle& {
    return *repo_handle_;
}

} // namespace repo
