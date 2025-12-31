#include <repo/repository.hpp>
#include <repo/tui/models/app.hpp>
#include <repo/tui/tea/program.hpp>
#include <repo/tui/tui.hpp>
#include <repo/tui/views/app.hpp>

#include <fmt/core.h>

#include <filesystem>

namespace repo::tui {

namespace {

// Find repository starting from current directory
auto find_repo() -> Result<std::string> {
    auto current_path = std::filesystem::current_path();

    while (true) {
        auto git_dir = current_path / ".git";
        if (std::filesystem::exists(git_dir)) {
            return current_path.string();
        }

        auto parent = current_path.parent_path();
        if (parent == current_path) {
            Error err;
            err.code = Error::Code::NotARepository;
            err.message = "Not a git repository (or any of the parent directories)";
            return std::unexpected(err);
        }
        current_path = parent;
    }
}

} // anonymous namespace

auto run() -> int {
    // Find repository
    auto repo_path_result = find_repo();
    if (!repo_path_result.has_value()) {
        fmt::print(stderr, "Error: {}\n", repo_path_result.error().message);
        return 1;
    }

    auto repo_path = *repo_path_result;

    // Create TEA program configuration
    tea::ProgramConfig<models::AppModel> config{
        .init = [repo_path] { return models::init_app(repo_path); },
        .update =
            [](models::AppModel model, tea::Msg msg) {
                // Update model and get commands
                auto [new_model, cmds] = models::update_app(std::move(model), std::move(msg));

                // Note: quit signal (new_model.should_quit) is automatically detected
                // and handled by the Program class

                return std::pair{std::move(new_model), std::move(cmds)};
            },
        .view = [](const models::AppModel& model) { return views::render_app(model); }};

    // Create and run program
    try {
        auto program = tea::make_program(std::move(config));
        program.run();
        return 0;
    } catch (const std::exception& e) {
        fmt::print(stderr, "Fatal error: {}\n", e.what());
        return 1;
    }
}

} // namespace repo::tui
