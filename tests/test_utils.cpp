#include "test_utils.hpp"

#include <repo/ops/commit.hpp>
#include <repo/ops/stage.hpp>

#include <cstdlib>
#include <fstream>
#include <random>
#include <sstream>

namespace repo::test {

namespace {
auto generate_temp_name() -> std::string {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);
    return "repo_test_" + std::to_string(dis(gen));
}
} // namespace

// TempDir implementation

TempDir::TempDir() {
    auto temp_base = std::filesystem::temp_directory_path();
    path_ = temp_base / generate_temp_name();
    std::filesystem::create_directories(path_);
}

TempDir::~TempDir() {
    if (!path_.empty() && std::filesystem::exists(path_)) {
        std::filesystem::remove_all(path_);
    }
}

auto TempDir::path() const -> const std::filesystem::path& {
    return path_;
}

// TempRepo implementation

TempRepo::TempRepo() : TempRepo(false) {}

TempRepo::TempRepo(bool bare) {
    // Initialize repository
    auto result = Repository::init(temp_dir_.path(), {.bare = bare});
    if (!result) {
        throw std::runtime_error("Failed to init test repo: " + result.error().format());
    }
    repo_ = std::move(*result);
}

auto TempRepo::path() const -> const std::filesystem::path& {
    return temp_dir_.path();
}

auto TempRepo::repo() -> Repository& {
    if (!repo_) {
        throw std::runtime_error("TempRepo repository not initialized");
    }
    return *repo_;
}

auto TempRepo::write_file(const std::filesystem::path& relative_path, std::string_view content)
    -> void {
    auto full_path = temp_dir_.path() / relative_path;

    // Create parent directories if needed
    if (auto parent = full_path.parent_path(); !parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    std::ofstream file(full_path);
    if (!file) {
        throw std::runtime_error("Failed to write file: " + full_path.string());
    }
    file << content;
}

auto TempRepo::read_file(const std::filesystem::path& relative_path) -> std::string {
    auto full_path = temp_dir_.path() / relative_path;
    std::ifstream file(full_path);
    if (!file) {
        throw std::runtime_error("Failed to read file: " + full_path.string());
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

auto TempRepo::delete_file(const std::filesystem::path& relative_path) -> void {
    auto full_path = temp_dir_.path() / relative_path;
    std::filesystem::remove(full_path);
}

auto TempRepo::file_exists(const std::filesystem::path& relative_path) -> bool {
    auto full_path = temp_dir_.path() / relative_path;
    return std::filesystem::exists(full_path);
}

auto TempRepo::run_git(const std::vector<std::string>& args) -> std::string {
    std::string cmd = "git -C \"" + temp_dir_.path().string() + "\"";
    for (const auto& arg : args) {
        cmd += " \"" + arg + "\"";
    }
    cmd += " 2>&1"; // Capture stderr too

    // NOLINTNEXTLINE(cert-env33-c)
    using FileDeleter = int (*)(FILE*);
    std::unique_ptr<FILE, FileDeleter> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("Failed to run git command");
    }

    std::string result;
    std::array<char, 128> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return result;
}

// CommitBuilder implementation

auto CommitBuilder::with_file(const std::filesystem::path& path, std::string_view content)
    -> CommitBuilder& {
    files_.emplace_back(path, content);
    return *this;
}

auto CommitBuilder::with_message(std::string_view msg) -> CommitBuilder& {
    message_ = msg;
    return *this;
}

auto CommitBuilder::create() -> domain::ObjectId {
    // Write files
    for (const auto& [path, content] : files_) {
        repo_.write_file(path, content);
    }

    // Stage files
    std::vector<std::filesystem::path> paths;
    paths.reserve(files_.size());
    for (const auto& [path, _] : files_) {
        paths.push_back(path);
    }

    auto stage_result = ops::stage(repo_.repo(), {.paths = paths});
    if (!stage_result) {
        throw std::runtime_error("Failed to stage files: " + stage_result.error().format());
    }

    // Commit
    auto commit_result = ops::commit(repo_.repo(), {
        .message = message_,
        .author = std::nullopt,
        .committer = std::nullopt
    });
    if (!commit_result) {
        throw std::runtime_error("Failed to commit: " + commit_result.error().format());
    }

    return commit_result->commit.id;
}

} // namespace repo::test
