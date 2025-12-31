#include <string_view>

namespace repo::ops::templates_internal {

constexpr std::string_view GITIGNORE_JAVA = R"(# ===================================
# Java
# ===================================

# Compiled class file
*.class

# Log file
*.log

# BlueJ files
*.ctxt

# Mobile Tools for Java (J2ME)
.mtj.tmp/

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
replay_pid*
)";

auto get_java_template() -> std::string_view {
    return GITIGNORE_JAVA;
}

} // namespace repo::ops::templates_internal
