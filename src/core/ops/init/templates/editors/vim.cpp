#include <string_view>

namespace repo::ops::templates_internal {

constexpr std::string_view GITIGNORE_VIM = R"(# ===================================
# Vim
# ===================================

# Swap
[._]*.s[a-v][a-z]
!*.svg  # comment out if you don't need vector files
[._]*.sw[a-p]
[._]s[a-rt-v][a-z]
[._]ss[a-gi-z]
[._]sw[a-p]

# Session
Session.vim
Sessionx.vim

# Temporary
.netrwhist
*~
# Auto-generated tag files
tags
# Persistent undo
[._]*.un~
)";

auto get_vim_template() -> std::string_view {
    return GITIGNORE_VIM;
}

} // namespace repo::ops::templates_internal
