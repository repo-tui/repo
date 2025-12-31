#include <string_view>

namespace repo::ops::templates_internal {

constexpr std::string_view GITIGNORE_JAVASCRIPT = R"(# ===================================
# JavaScript/TypeScript (Node, React, Vue, Bun, Deno)
# ===================================

# Dependencies
node_modules/
jspm_packages/

# Bun
.bun/

# Production build files
dist/
build/
out/
.next/
.nuxt/
.cache/

# Testing
coverage/
.nyc_output/

# TypeScript cache
*.tsbuildinfo

# Optional npm cache directory
.npm

# Optional eslint cache
.eslintcache

# Optional stylelint cache
.stylelintcache

# Microbundle cache
.rpt2_cache/
.rts2_cache_cjs/
.rts2_cache_es/
.rts2_cache_umd/

# Optional REPL history
.node_repl_history

# Output of 'npm pack'
*.tgz

# Yarn
.yarn/cache
.yarn/unplugged
.yarn/build-state.yml
.yarn/install-state.gz
.pnp.*

# parcel-bundler cache
.parcel-cache

# Next.js
.next/
out/

# Nuxt.js
.nuxt/
dist/

# Gatsby files
.cache/
public/

# vuepress build output
.vuepress/dist

# Serverless directories
.serverless/

# FuseBox cache
.fusebox/

# DynamoDB Local files
.dynamodb/

# TernJS port file
.tern-port

# Stores VSCode versions used for testing VSCode extensions
.vscode-test

# Temporary folders
tmp/
temp/

# Deno
deno.lock
)";

auto get_javascript_template() -> std::string_view {
    return GITIGNORE_JAVASCRIPT;
}

} // namespace repo::ops::templates_internal
