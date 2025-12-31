#include <string_view>

namespace repo::ops::templates_internal {

constexpr std::string_view GITIGNORE_CMAKE = R"(# ===================================
# CMake
# ===================================

CMakeLists.txt.user
CMakeCache.txt
CMakeFiles/
CMakeScripts/
Testing/
Makefile
cmake_install.cmake
install_manifest.txt
compile_commands.json
CTestTestfile.cmake
_deps
cmake-build-*/
build/
)";

auto get_cmake_template() -> std::string_view {
    return GITIGNORE_CMAKE;
}

} // namespace repo::ops::templates_internal
