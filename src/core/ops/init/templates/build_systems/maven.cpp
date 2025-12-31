#include <string_view>

namespace repo::ops::templates_internal {

constexpr std::string_view GITIGNORE_MAVEN = R"(# ===================================
# Maven
# ===================================

target/
pom.xml.tag
pom.xml.releaseBackup
pom.xml.versionsBackup
pom.xml.next
release.properties
dependency-reduced-pom.xml
buildNumber.properties
.mvn/timing.properties
.mvn/wrapper/maven-wrapper.jar
)";

auto get_maven_template() -> std::string_view {
    return GITIGNORE_MAVEN;
}

} // namespace repo::ops::templates_internal
