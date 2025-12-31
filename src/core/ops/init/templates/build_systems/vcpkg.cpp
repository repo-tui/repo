#include <string_view>

namespace repo::ops::templates_internal {

constexpr std::string_view GITIGNORE_VCPKG = R"(# ===================================
# vcpkg (includes CMake patterns)
# ===================================

# vcpkg
vcpkg_installed/
.vcpkg-root
vcpkg/

# CMake (commonly used with vcpkg)
CMakeCache.txt
CMakeFiles/
cmake_install.cmake
install_manifest.txt
compile_commands.json
CTestTestfile.cmake
cmake-build-*/
build/
)";

auto get_vcpkg_template() -> std::string_view {
    return GITIGNORE_VCPKG;
}

} // namespace repo::ops::templates_internal
