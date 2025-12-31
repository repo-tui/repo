#include <string_view>

namespace repo::ops::templates_internal {

constexpr std::string_view GITIGNORE_SWIFT = R"(# ===================================
# Swift
# ===================================

# Xcode
xcuserdata/
*.xcscmblueprint
*.xccheckout

# Build generated
/build/
/DerivedData/

# Carthage
Carthage/Build/

# CocoaPods
Pods/

# Swift Package Manager
.build/
.swiftpm/

# Fastlane
fastlane/report.xml
fastlane/Preview.html
fastlane/screenshots/**/*.png
fastlane/test_output
)";

auto get_swift_template() -> std::string_view {
    return GITIGNORE_SWIFT;
}

} // namespace repo::ops::templates_internal
