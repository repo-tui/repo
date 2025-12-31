#include <string_view>

namespace repo::ops::templates_internal {

constexpr std::string_view GITIGNORE_MAKE = R"(# ===================================
# Make
# ===================================

# Object files
*.o
*.ko
*.obj
*.elf

# Linker output
*.ilk
*.map
*.exp

# Precompiled Headers
*.gch
*.pch

# Libraries
*.lib
*.a
*.la
*.lo

# Shared objects
*.dll
*.so
*.so.*
*.dylib

# Executables
*.exe
*.out
*.app
)";

auto get_make_template() -> std::string_view {
    return GITIGNORE_MAKE;
}

} // namespace repo::ops::templates_internal
