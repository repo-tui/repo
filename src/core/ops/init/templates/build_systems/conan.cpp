#include <string_view>

namespace repo::ops::templates_internal {

constexpr std::string_view GITIGNORE_CONAN = R"(# ===================================
# Conan
# ===================================

# Conan cache
conan/
.conan/
conanbuildinfo.*
conaninfo.txt
conan.lock
graph_info.json
)";

auto get_conan_template() -> std::string_view {
    return GITIGNORE_CONAN;
}

} // namespace repo::ops::templates_internal
