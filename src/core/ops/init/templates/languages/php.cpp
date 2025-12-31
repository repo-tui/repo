#include <string_view>

namespace repo::ops::templates_internal {

constexpr std::string_view GITIGNORE_PHP = R"(# ===================================
# PHP
# ===================================

# Composer
/vendor/
composer.lock
composer.phar

# Laravel specific
/bootstrap/compiled.php
/storage/
/public/hot
/public/storage

# Symfony specific
/app/cache/
/app/logs/
/var/cache/
/var/logs/

# PHPUnit
/phpunit.xml
.phpunit.result.cache
)";

auto get_php_template() -> std::string_view {
    return GITIGNORE_PHP;
}

} // namespace repo::ops::templates_internal
