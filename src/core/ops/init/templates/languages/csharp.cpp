#include <string_view>

namespace repo::ops::templates_internal {

constexpr std::string_view GITIGNORE_CSHARP = R"(# ===================================
# C#
# ===================================

# Build results
[Dd]ebug/
[Rr]elease/
x64/
x86/
[Ww][Ii][Nn]32/
[Aa][Rr][Mm]/
[Aa][Rr][Mm]64/
bld/
[Bb]in/
[Oo]bj/
[Ll]og/
[Ll]ogs/

# Visual Studio cache/options
.vs/
*.suo
*.user
*.userosscache
*.sln.docstates

# User-specific files
*.rsuser

# .NET Core
project.lock.json
project.fragment.lock.json
artifacts/

# ASP.NET Scaffolding
ScaffoldingReadMe.txt
)";

auto get_csharp_template() -> std::string_view {
    return GITIGNORE_CSHARP;
}

} // namespace repo::ops::templates_internal
