#include <string_view>

namespace repo::ops::templates_internal {

constexpr std::string_view GITIGNORE_WINDOWS = R"(# ===================================
# Windows
# ===================================

# Windows thumbnail cache files
Thumbs.db
Thumbs.db:encryptable
ehthumbs.db
ehthumbs_vista.db

# Dump file
*.stackdump

# Folder config file
[Dd]esktop.ini

# Recycle Bin used on file shares
$RECYCLE.BIN/

# Windows Installer files
*.cab
*.msi
*.msix
*.msm
*.msp

# Windows shortcuts
*.lnk
)";

auto get_windows_template() -> std::string_view {
    return GITIGNORE_WINDOWS;
}

} // namespace repo::ops::templates_internal
