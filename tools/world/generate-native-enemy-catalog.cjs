const fs = require("node:fs");
const path = require("node:path");

const root = path.resolve(__dirname, "../..");
const lua = fs.readFileSync(path.join(root, "QDCoop-Direct/mod/Scripts/enemy_catalog.lua"), "utf8");
const match = lua.match(/^return \{classes=\{([^}]*)\},meshes=\{([^}]*)\},materials=\{/);
if (!match) throw new Error("Invalid generated enemy catalog");

const lines = [
    "// Generated from the preview3 enemy_catalog.lua. IDs retain Lua indexing.",
    "#pragma once",
    "#include <string_view>",
    "namespace qdcoop::enemy::catalog {"
];
const sizes = [];
for (const [index, name] of ["classes", "meshes"].entries()) {
    const paths = JSON.parse(`[${match[index + 1]}]`);
    if (!paths.length || paths.some(value => typeof value !== "string" || !/^\/Game\/[^\r\n]+\.[^\r\n.]+$/.test(value)) ||
        new Set(paths.map(value => value.toLowerCase())).size !== paths.length || paths.length >= 65536) {
        throw new Error(`Invalid catalog ${name}`);
    }
    lines.push(`inline constexpr std::string_view ${name}[] = {{},`);
    for (const asset of paths) lines.push(`    ${JSON.stringify(asset)},`);
    lines.push("};");
    sizes.push(paths.length);
}
lines.push("}");

const target = path.join(root, "native/include/qdcoop/enemy_catalog.hpp");
const content = lines.join("\n") + "\n";
if (process.argv.includes("--check")) {
    if (fs.readFileSync(target, "utf8").replace(/\r\n/g, "\n") !== content) {
        throw new Error("Regenerate enemy_catalog.hpp from the preview3 Lua catalog");
    }
} else {
    fs.writeFileSync(target, content);
}
console.log(`Native enemy catalog: ${sizes[0]} classes, ${sizes[1]} meshes`);
