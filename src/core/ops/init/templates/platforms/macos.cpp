#include <string_view>

namespace repo::ops::templates_internal {

constexpr std::string_view GITIGNORE_MACOS = R"(# ===================================
# macOS
# ===================================

# General
.DS_Store
.AppleDouble
.LSOverride

# Icon must end with two \r
Icon

# Thumbnails
._*

# Files that might appear in the root of a volume
.DocumentRevisions-V100
.fseventsd
.Spotlight-V100
.TemporaryItems
.Trashes
.VolumeIcon.icns
.com.apple.timemachine.donotpresent

# Directories potentially created on remote AFP share
.AppleDB
.AppleDesktop
Network Trash Folder
Temporary Items
.apdisk
)";

auto get_macos_template() -> std::string_view {
    return GITIGNORE_MACOS;
}

} // namespace repo::ops::templates_internal
