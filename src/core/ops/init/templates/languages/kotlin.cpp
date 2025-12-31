#include <string_view>

namespace repo::ops::templates_internal {

constexpr std::string_view GITIGNORE_KOTLIN = R"(# ===================================
# Kotlin
# ===================================

# Compiled class file
*.class

# Log file
*.log

# Package Files
*.jar
*.war
*.nar
*.ear
*.zip
*.tar.gz
*.rar

# virtual machine crash logs
hs_err_pid*

# Gradle
.gradle/
build/

# IntelliJ
.idea/
*.iml
*.iws
*.ipr
out/
)";

auto get_kotlin_template() -> std::string_view {
    return GITIGNORE_KOTLIN;
}

} // namespace repo::ops::templates_internal
