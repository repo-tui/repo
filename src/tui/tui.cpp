#include <repo/ops/branch.hpp>
#include <repo/ops/commit.hpp>
#include <repo/ops/diff.hpp>
#include <repo/ops/list_commits.hpp>
#include <repo/ops/stage.hpp>
#include <repo/ops/status.hpp>
#include <repo/ops/switch.hpp>
#include <repo/repository.hpp>
#include <repo/tui/tui.hpp>

#include <fmt/core.h>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace repo::tui {

namespace {

using namespace ftxui;

// Find repository starting from current directory
auto find_repo() -> Result<Repository> {
    auto current_path = std::filesystem::current_path();

    while (true) {
        auto git_dir = current_path / ".git";
        if (std::filesystem::exists(git_dir)) {
            return Repository::open(current_path);
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

// File entry for display
struct FileEntry {
    std::filesystem::path path;
    std::string prefix;
    Color color;
    bool is_staged;
};

// Branch entry for display
struct BranchEntry {
    std::string name;
    bool is_current;
};

// App state
struct AppState {
    Repository* repo;
    int tab_selected = 0;
    int status_file_selected = 0;
    int branch_selected = 0;
    std::vector<FileEntry> files;
    std::vector<BranchEntry> branches;
    std::string error_message;
    bool show_commit_dialog = false;
    bool show_diff_view = false;
    bool show_help = false;
    std::string commit_message;
    std::string status_message;
    std::string diff_content;

    // Refresh file list from repo
    void refresh_files() {
        files.clear();
        error_message.clear();

        auto status_result = ops::status(*repo);
        if (!status_result.has_value()) {
            error_message = status_result.error().message;
            return;
        }

        auto& status = *status_result;

        // Staged files
        for (const auto& file : status.staged()) {
            std::string prefix = "?";
            switch (file.index_status) {
                case domain::FileStatus::State::Added:
                    prefix = "A";
                    break;
                case domain::FileStatus::State::Modified:
                    prefix = "M";
                    break;
                case domain::FileStatus::State::Deleted:
                    prefix = "D";
                    break;
                case domain::FileStatus::State::Renamed:
                    prefix = "R";
                    break;
                default:
                    break;
            }
            files.push_back({file.path, prefix, Color::Green, true});
        }

        // Unstaged files
        for (const auto& file : status.unstaged()) {
            std::string prefix = "?";
            switch (file.worktree_status) {
                case domain::FileStatus::State::Modified:
                    prefix = "M";
                    break;
                case domain::FileStatus::State::Deleted:
                    prefix = "D";
                    break;
                default:
                    break;
            }
            files.push_back({file.path, prefix, Color::Yellow, false});
        }

        // Untracked files
        for (const auto& file : status.untracked()) {
            files.push_back({file.path, "?", Color::White, false});
        }

        // Keep selection in bounds
        if (status_file_selected >= static_cast<int>(files.size())) {
            status_file_selected = std::max(0, static_cast<int>(files.size()) - 1);
        }
    }

    // Refresh branch list
    void refresh_branches() {
        branches.clear();
        error_message.clear();

        auto branches_result = ops::list_branches(*repo);
        if (!branches_result.has_value()) {
            error_message = branches_result.error().message;
            return;
        }

        for (const auto& branch : branches_result->branches) {
            branches.push_back({branch.name, branch.is_head});
        }

        // Keep selection in bounds
        if (branch_selected >= static_cast<int>(branches.size())) {
            branch_selected = std::max(0, static_cast<int>(branches.size()) - 1);
        }
    }

    // Toggle stage/unstage for selected file
    void toggle_stage() {
        if (status_file_selected < 0 || status_file_selected >= static_cast<int>(files.size())) {
            return;
        }

        auto& file = files[status_file_selected];

        if (file.is_staged) {
            // Unstage
            auto result = ops::unstage(*repo, {.paths = {file.path}});
            if (!result.has_value()) {
                status_message = "Error: " + result.error().message;
            } else {
                status_message = "Unstaged: " + file.path.string();
                refresh_files();
            }
        } else {
            // Stage
            auto result = ops::stage(*repo, {.paths = {file.path}});
            if (!result.has_value()) {
                status_message = "Error: " + result.error().message;
            } else {
                status_message = "Staged: " + file.path.string();
                refresh_files();
            }
        }
    }

    // Show diff for selected file
    void show_diff() {
        if (status_file_selected < 0 || status_file_selected >= static_cast<int>(files.size())) {
            return;
        }

        auto& file = files[status_file_selected];
        diff_content.clear();

        // Get diff - use mode based on file staging status
        ops::DiffParams::Mode mode =
            file.is_staged ? ops::DiffParams::Mode::Staged : ops::DiffParams::Mode::Unstaged;

        auto diff_result = ops::diff(*repo, {.mode = mode});
        if (!diff_result.has_value()) {
            diff_content = "Error: " + diff_result.error().message;
            show_diff_view = true;
            return;
        }

        // Find the selected file in the diff results
        for (const auto& file_diff : diff_result->diffs) {
            if (file_diff.path == file.path ||
                (file_diff.old_path && *file_diff.old_path == file.path)) {

                std::string old_path_str =
                    file_diff.old_path ? file_diff.old_path->string() : file_diff.path.string();

                diff_content += "--- " + old_path_str + "\n";
                diff_content += "+++ " + file_diff.path.string() + "\n";

                for (const auto& hunk : file_diff.hunks) {
                    diff_content += "@@ -" + std::to_string(hunk.old_start) + "," +
                                    std::to_string(hunk.old_lines) + " +" +
                                    std::to_string(hunk.new_start) + "," +
                                    std::to_string(hunk.new_lines) + " @@\n";

                    // Format each line
                    for (const auto& line : hunk.lines) {
                        switch (line.origin) {
                            case domain::DiffLine::Origin::Addition:
                                diff_content += "+" + line.content + "\n";
                                break;
                            case domain::DiffLine::Origin::Deletion:
                                diff_content += "-" + line.content + "\n";
                                break;
                            case domain::DiffLine::Origin::Context:
                                diff_content += " " + line.content + "\n";
                                break;
                        }
                    }
                }
                break;
            }
        }

        if (diff_content.empty()) {
            diff_content = "No changes to display for this file";
        }

        show_diff_view = true;
    }

    // Switch to selected branch
    void switch_branch() {
        if (branch_selected < 0 || branch_selected >= static_cast<int>(branches.size())) {
            return;
        }

        auto& branch = branches[branch_selected];
        if (branch.is_current) {
            status_message = "Already on branch: " + branch.name;
            return;
        }

        auto result = ops::switch_branch(*repo, {.branch_name = branch.name});
        if (!result.has_value()) {
            status_message = "Error: " + result.error().message;
        } else {
            status_message = "Switched to branch: " + branch.name;
            refresh_branches();
            refresh_files();
        }
    }

    // Create commit
    void create_commit() {
        if (commit_message.empty()) {
            status_message = "Error: Commit message cannot be empty";
            return;
        }

        auto result = ops::commit(*repo, {.message = commit_message});
        if (!result.has_value()) {
            status_message = "Error: " + result.error().message;
        } else {
            status_message = "Created commit: " + result->commit.id.to_short();
            commit_message.clear();
            show_commit_dialog = false;
            refresh_files();
        }
    }
};

// Render status view
auto render_status(AppState& state) -> Element {
    if (!state.error_message.empty()) {
        return vbox({text("Error: " + state.error_message) | color(Color::Red)});
    }

    if (state.files.empty()) {
        return vbox({text("✓ Working tree clean") | color(Color::Green)});
    }

    Elements elements;

    // File list
    for (size_t i = 0; i < state.files.size(); ++i) {
        const auto& file = state.files[i];
        auto line = hbox(
            {text(file.prefix) | color(file.color) | bold, text(" "), text(file.path.string())});

        if (static_cast<int>(i) == state.status_file_selected) {
            line = line | inverted | bold;
        }

        elements.push_back(line);
    }

    return vbox(std::move(elements));
}

// Render log view
auto render_log(Repository& repo) -> Element {
    auto commits_result = ops::list_commits(repo, {.max_count = 50});
    if (!commits_result.has_value()) {
        return vbox({text("Error: " + commits_result.error().message) | color(Color::Red)});
    }

    Elements elements;
    for (const auto& commit : commits_result->commits) {
        elements.push_back(hbox({text(commit.id.to_short()) | color(Color::Yellow) | bold,
                                 text(" "), text(commit.message)}));
        elements.push_back(text("  " + commit.author.name + " <" + commit.author.email + ">") |
                           dim);
        elements.push_back(text(""));
    }

    return vbox(std::move(elements));
}

// Render branches view
auto render_branches(AppState& state) -> Element {
    if (!state.error_message.empty()) {
        return vbox({text("Error: " + state.error_message) | color(Color::Red)});
    }

    Elements elements;
    for (size_t i = 0; i < state.branches.size(); ++i) {
        const auto& branch = state.branches[i];

        auto line = hbox({text(branch.is_current ? "* " : "  "), text(branch.name)});

        if (branch.is_current) {
            line = line | color(Color::Green) | bold;
        }

        if (static_cast<int>(i) == state.branch_selected) {
            line = line | inverted;
        }

        elements.push_back(line);
    }

    return vbox(std::move(elements));
}

// Render commit dialog
auto render_commit_dialog(AppState& state) -> Element {
    return vbox({text("Create Commit") | bold | center, separator(), text("Message:"),
                 hbox({text(state.commit_message.empty() ? "" : state.commit_message),
                       text("_") | blink}) |
                     border,
                 separator(), text("Type message | Enter: Commit | Esc: Cancel") | dim}) |
           border | size(WIDTH, GREATER_THAN, 50) | size(HEIGHT, GREATER_THAN, 8) | center;
}

// Render diff view
auto render_diff_view(AppState& state) -> Element {
    auto lines = Elements{};

    // Split diff content by lines and colorize
    std::string line;
    std::istringstream stream(state.diff_content);
    while (std::getline(stream, line)) {
        Element line_elem;
        if (line.empty()) {
            line_elem = text("");
        } else if (line[0] == '+' && line.size() > 1 && line[1] != '+') {
            line_elem = text(line) | color(Color::Green);
        } else if (line[0] == '-' && line.size() > 1 && line[1] != '-') {
            line_elem = text(line) | color(Color::Red);
        } else if (line[0] == '@') {
            line_elem = text(line) | color(Color::Cyan) | bold;
        } else {
            line_elem = text(line);
        }
        lines.push_back(line_elem);
    }

    return vbox({text("Diff View") | bold | center, separator(),
                 vbox(std::move(lines)) | flex | vscroll_indicator | frame, separator(),
                 text("Esc: Close") | dim | center}) |
           border | flex;
}

// Render help screen
auto render_help() -> Element {
    return vbox({text("Keyboard Shortcuts") | bold | center,
                 separator(),
                 text(""),
                 text("GLOBAL") | bold,
                 text("  1/2/3     Switch to Status/Log/Branches tab"),
                 text("  r         Refresh current view"),
                 text("  ?         Show this help"),
                 text("  q         Quit"),
                 text(""),
                 text("STATUS VIEW") | bold,
                 text("  ↑/↓       Select file"),
                 text("  Space     Stage/unstage selected file"),
                 text("  d         Show diff for selected file"),
                 text("  c         Create commit (if files are staged)"),
                 text(""),
                 text("BRANCHES VIEW") | bold,
                 text("  ↑/↓       Select branch"),
                 text("  Enter     Switch to selected branch"),
                 text(""),
                 text("COMMIT DIALOG") | bold,
                 text("  Type      Enter commit message"),
                 text("  Enter     Create commit"),
                 text("  Esc       Cancel"),
                 text(""),
                 text("DIFF VIEW") | bold,
                 text("  Esc       Close diff view"),
                 separator(),
                 text("Press any key to close") | dim | center}) |
           border | center;
}

} // anonymous namespace

auto run() -> int {
    // Find repository
    auto repo_result = find_repo();
    if (!repo_result.has_value()) {
        fmt::print(stderr, "Error: {}\n", repo_result.error().message);
        return 1;
    }

    auto& repo = *repo_result;

    // App state
    AppState state;
    state.repo = &repo;
    state.refresh_files();
    state.refresh_branches();

    // Tab state
    std::vector<std::string> tab_entries = {"Status [1]", "Log [2]", "Branches [3]"};

    // Main component
    auto main_component = Renderer([&] {
        Element tab_content;

        // Tab bar
        Elements tab_elements;
        for (size_t i = 0; i < tab_entries.size(); ++i) {
            auto tab = text(tab_entries[i]);
            if (static_cast<int>(i) == state.tab_selected) {
                tab = tab | bold | inverted;
            }
            tab_elements.push_back(tab);
            if (i < tab_entries.size() - 1) {
                tab_elements.push_back(text(" | "));
            }
        }

        // Render content based on selected tab
        switch (state.tab_selected) {
            case 0:
                tab_content = render_status(state);
                break;
            case 1:
                tab_content = render_log(repo);
                break;
            case 2:
                tab_content = render_branches(state);
                break;
            default:
                tab_content = text("Unknown tab");
        }

        // Help text based on tab and state
        std::string help_text;
        if (state.tab_selected == 0 && !state.files.empty()) {
            help_text = "↑/↓: Select | Space: Stage/Unstage | d: Diff | c: Commit | r: Refresh | "
                        "?: Help | q: Quit";
        } else if (state.tab_selected == 2 && !state.branches.empty()) {
            help_text = "↑/↓: Select | Enter: Switch branch | r: Refresh | ?: Help | q: Quit";
        } else {
            help_text = "1/2/3: Switch tabs | r: Refresh | ?: Help | q: Quit";
        }

        // Status bar
        Element status_bar;
        if (!state.status_message.empty()) {
            status_bar = text(state.status_message) | color(Color::Cyan);
        } else {
            status_bar = text(help_text) | dim;
        }

        // Build final layout
        auto main_layout =
            vbox({text("Repo - Modern Git Interface") | bold | center, separator(),
                  hbox(std::move(tab_elements)), separator(),
                  tab_content | flex | vscroll_indicator | frame, separator(), status_bar});

        // Show overlays
        if (state.show_help) {
            main_layout = dbox({main_layout | dim, render_help() | clear_under | center});
        } else if (state.show_diff_view) {
            main_layout = dbox({main_layout | dim, render_diff_view(state) | clear_under});
        } else if (state.show_commit_dialog) {
            main_layout =
                dbox({main_layout | dim, render_commit_dialog(state) | clear_under | center});
        }

        return main_layout;
    });

    // Handle keyboard events
    auto event_handler = CatchEvent(main_component, [&](Event event) {
        // Help screen
        if (state.show_help) {
            state.show_help = false;
            return true;
        }

        // Diff view
        if (state.show_diff_view) {
            if (event == Event::Escape) {
                state.show_diff_view = false;
                return true;
            }
            return false;
        }

        // Global shortcuts (unless in commit dialog)
        if (!state.show_commit_dialog) {
            if (event == Event::Character('q') || event == Event::Character('Q')) {
                return true; // Will exit via screen.Exit()
            }

            if (event == Event::Character('?')) {
                state.show_help = true;
                return true;
            }

            if (event == Event::Character('1')) {
                state.tab_selected = 0;
                state.status_message.clear();
                state.refresh_files();
                return true;
            }
            if (event == Event::Character('2')) {
                state.tab_selected = 1;
                state.status_message.clear();
                return true;
            }
            if (event == Event::Character('3')) {
                state.tab_selected = 2;
                state.status_message.clear();
                state.refresh_branches();
                return true;
            }

            if (event == Event::Character('r') || event == Event::Character('R')) {
                if (state.tab_selected == 0) {
                    state.refresh_files();
                } else if (state.tab_selected == 2) {
                    state.refresh_branches();
                }
                state.status_message = "Refreshed";
                return true;
            }
        }

        // Status view shortcuts
        if (state.tab_selected == 0 && !state.show_commit_dialog) {
            if (event == Event::ArrowUp) {
                if (state.status_file_selected > 0) {
                    state.status_file_selected--;
                }
                state.status_message.clear();
                return true;
            }
            if (event == Event::ArrowDown) {
                if (state.status_file_selected < static_cast<int>(state.files.size()) - 1) {
                    state.status_file_selected++;
                }
                state.status_message.clear();
                return true;
            }
            if (event == Event::Character(' ')) {
                state.toggle_stage();
                return true;
            }
            if (event == Event::Character('d') || event == Event::Character('D')) {
                state.show_diff();
                return true;
            }
            if (event == Event::Character('c') || event == Event::Character('C')) {
                // Check if there are staged files
                bool has_staged = false;
                for (const auto& file : state.files) {
                    if (file.is_staged) {
                        has_staged = true;
                        break;
                    }
                }

                if (!has_staged) {
                    state.status_message = "Error: No staged files to commit";
                } else {
                    state.show_commit_dialog = true;
                    state.status_message.clear();
                }
                return true;
            }
        }

        // Branches view shortcuts
        if (state.tab_selected == 2 && !state.show_commit_dialog) {
            if (event == Event::ArrowUp) {
                if (state.branch_selected > 0) {
                    state.branch_selected--;
                }
                state.status_message.clear();
                return true;
            }
            if (event == Event::ArrowDown) {
                if (state.branch_selected < static_cast<int>(state.branches.size()) - 1) {
                    state.branch_selected++;
                }
                state.status_message.clear();
                return true;
            }
            if (event == Event::Return) {
                state.switch_branch();
                return true;
            }
        }

        // Commit dialog shortcuts
        if (state.show_commit_dialog) {
            if (event == Event::Escape) {
                state.show_commit_dialog = false;
                state.commit_message.clear();
                state.status_message.clear();
                return true;
            }
            if (event == Event::Return) {
                if (!state.commit_message.empty()) {
                    state.create_commit();
                    return true;
                }
                return true;
            }
            if (event == Event::Backspace) {
                if (!state.commit_message.empty()) {
                    state.commit_message.pop_back();
                }
                return true;
            }
            if (event.is_character() && !event.character().empty()) {
                state.commit_message += event.character();
                return true;
            }
        }

        return false;
    });

    // Run the UI
    auto screen = ScreenInteractive::Fullscreen();

    // Handle quit with 'q'
    auto final_component = CatchEvent(event_handler, [&](Event event) {
        if ((event == Event::Character('q') || event == Event::Character('Q')) &&
            !state.show_commit_dialog && !state.show_diff_view && !state.show_help) {
            screen.Exit();
            return true;
        }
        return false;
    });

    screen.Loop(final_component);

    return 0;
}

} // namespace repo::tui
