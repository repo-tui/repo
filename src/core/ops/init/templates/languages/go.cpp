#include <string_view>

namespace repo::ops::templates_internal {

constexpr std::string_view GITIGNORE_GO = R"(# ===================================
# Go
# ===================================

# Binaries for programs and plugins
*.exe
*.exe~
*.dll
*.so
*.dylib

# Test binary, built with `go test -c`
*.test

# Output of the go coverage tool
*.out

# Dependency directories
vendor/

# Go workspace file
go.work
)";

auto get_go_template() -> std::string_view {
    return GITIGNORE_GO;
}

} // namespace repo::ops::templates_internal
