#include <string_view>

namespace repo::ops::templates_internal {

constexpr std::string_view GITIGNORE_CPP = R"(# ===================================
# C++
# ===================================

# Prerequisites
*.d

# Compiled Object files
*.slo
*.lo
*.o
*.obj

# Precompiled Headers
*.gch
*.pch

# Compiled Dynamic libraries
*.so
*.dylib
*.dll

# Fortran module files
*.mod
*.smod

# Compiled Static libraries
*.lai
*.la
*.a
*.lib

# Executables
*.exe
*.out
*.app
)";

auto get_cpp_template() -> std::string_view {
    return GITIGNORE_CPP;
}

} // namespace repo::ops::templates_internal
