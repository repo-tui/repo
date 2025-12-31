#include <string_view>

namespace repo::ops::templates_internal {

constexpr std::string_view GITIGNORE_GRADLE = R"(# ===================================
# Gradle
# ===================================

.gradle/
build/
!gradle/wrapper/gradle-wrapper.jar
!**/src/main/**/build/
!**/src/test/**/build/

# Gradle cache
.gradle/
gradle-app.setting
!gradle-wrapper.jar
.gradletasknamecache
)";

auto get_gradle_template() -> std::string_view {
    return GITIGNORE_GRADLE;
}

} // namespace repo::ops::templates_internal
