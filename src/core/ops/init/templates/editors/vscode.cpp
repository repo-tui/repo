#include <string_view>

namespace repo::ops::templates_internal {

constexpr std::string_view GITIGNORE_VSCODE = R"(# ===================================
# VSCode
# ===================================

.vscode/*
!.vscode/settings.json
!.vscode/tasks.json
!.vscode/launch.json
!.vscode/extensions.json
!.vscode/*.code-snippets

# Local History for Visual Studio Code
.history/

# Built Visual Studio Code Extensions
*.vsix
)";

auto get_vscode_template() -> std::string_view {
    return GITIGNORE_VSCODE;
}

} // namespace repo::ops::templates_internal
