#include <string_view>

namespace repo::ops::templates_internal {

constexpr std::string_view GITIGNORE_LINUX = R"(# ===================================
# Linux
# ===================================

*~

# temporary files which can be created if a process still has a handle open of a deleted file
.fuse_hidden*

# KDE directory preferences
.directory

# Linux trash folder which might appear on any partition or disk
.Trash-*

# .nfs files are created when an open file is removed but is still being accessed
.nfs*
)";

auto get_linux_template() -> std::string_view {
    return GITIGNORE_LINUX;
}

} // namespace repo::ops::templates_internal
