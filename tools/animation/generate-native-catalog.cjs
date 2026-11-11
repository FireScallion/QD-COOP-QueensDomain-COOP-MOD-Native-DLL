const fs = require("node:fs");
const path = require("node:path");

const root = path.resolve(__dirname, "../..");
const source = path.join(root, "QDCoop-Direct/mod/Scripts/enemy_animation_catalog.lua");
const output = path.join(root, "native/include/qdcoop");
const check = process.argv.includes("--check");
const lists = { profiles: [], montages: [], sequences: [] };
const skeletons = new Map();
let section;

for (const line of fs.readFileSync(source, "utf8").split(/\r?\n/)) {
    if (line === "},meshes={") break;
    const opening = line.match(/^(?:return \{|\},)(profiles|montages|sequences)=\{$/);
    if (opening) {
        section = opening[1];
        continue;
    }
    if (!section && (!line || line.startsWith("--"))) continue;
    const row = line.match(/^\{path="([^"\\]+)",skeleton="([^"\\]+)"(.*)\},?$/);
    if (!section || !row) throw new Error(`Unexpected catalog row: ${line}`);
    const [, objectPath, skeletonPath, extra] = row;
    if (!skeletons.has(skeletonPath)) skeletons.set(skeletonPath, skeletons.size + 1);
    const entry = { path: objectPath, skeleton: skeletons.get(skeletonPath), inputs: [], kinds: [] };
    if (section === "profiles") {
        const fields = extra.match(/^,count=(\d+),inputs=\{(.*)\}$/);
        if (!fields) throw new Error(`Missing profile fields for ${objectPath}`);
        if (fields[2]) {
            for (const input of fields[2].split(/,(?=\{)/)) {
                const parsed = input.match(/^\{name="([^"\\]+)",kind="([abfirv])"\}$/);
                if (!parsed) throw new Error(`Unexpected profile input: ${input}`);
                const [, name, kind] = parsed;
                entry.inputs.push(name);
                entry.kinds.push(...Array(kind === "r" || kind === "v" ? 3 : 1).fill(kind));
            }
        }
        if (entry.kinds.length !== Number(fields[1]) || entry.kinds.length > 8 ||
            new Set(entry.inputs).size !== entry.inputs.length) {
            throw new Error(`Invalid profile input layout for ${objectPath}`);
        }
    } else if (extra) {
        throw new Error(`Unexpected asset data for ${objectPath}`);
    }
    lists[section].push(entry);
}

for (const [name, entries] of Object.entries(lists)) {
    if (!entries.length || new Set(entries.map(entry => entry.path.toLowerCase())).size !== entries.length) {
        throw new Error(`Missing or duplicate ${name}`);
    }
}

const header = "// Generated from the preview3 enemy_animation_catalog.lua; IDs retain Lua indexing.";
const namespace = "namespace qdcoop::enemy::animation {";
const quote = JSON.stringify;
const layout = [header, "#pragma once", "#include <array>", "#include <cstdint>", namespace,
    "struct Layout { unsigned skeleton,count; std::array<char,8> kinds; };",
    "inline constexpr Layout layouts[] = {{0,0,{}},"
];
for (const profile of lists.profiles) {
    layout.push(`{${profile.skeleton},${profile.kinds.length},{${profile.kinds.map(kind => `'${kind}'`).join(",")}}},`);
}
layout.push("};");
for (const kind of ["montages", "sequences"]) {
    layout.push(`inline constexpr unsigned ${kind}[] = {0,${lists[kind].map(asset => asset.skeleton).join(",")}};`);
}
const loops = new Map([
    ["Boner_CasketTrap_Montage", "2.4583333"],
    ["Boner_Trap_Heavy_Montage", "3.3333335"],
    ["Boner_Trap_Montage", "0.96666664"]
]);
layout.push("inline double loop_end(unsigned id) { switch(id) {");
lists.montages.forEach(({ path: assetPath }, index) => {
    const leaf = assetPath.split(".").at(-1);
    if (loops.has(leaf) && assetPath === `/Game/Enemies/Boner_00/Animation/${leaf}.${leaf}`) {
        layout.push(`case ${index + 1}: return ${loops.get(leaf)};`);
        loops.delete(leaf);
    }
});
if (loops.size) throw new Error("Missing looping montage in catalog");
layout.push("default: return 0; } }", "}");

const catalog = [header, "#pragma once", '#include "animation_layout.hpp"', "#include <string_view>",
    namespace, "struct CatalogProfile { std::string_view path; std::array<std::string_view,8> fields; };",
    "inline constexpr CatalogProfile catalog_profiles[] = {{},"
];
for (const profile of lists.profiles) {
    catalog.push(`    {${quote(profile.path)}, {${profile.inputs.map(name => quote(name)).join(", ")}}},`);
}
catalog.push("};");
for (const [name, paths] of [
    ["catalog_montages", lists.montages.map(entry => entry.path)],
    ["catalog_sequences", lists.sequences.map(entry => entry.path)],
    ["catalog_skeletons", [...skeletons.keys()]]
]) {
    catalog.push(`inline constexpr std::string_view ${name}[] = {{},`);
    for (const item of paths) catalog.push(`    ${quote(item)},`);
    catalog.push("};");
    if (name === "catalog_skeletons") continue;
    const order = paths.map((asset, index) => ({ asset, id: index + 1 }))
        .sort((a, b) => a.asset < b.asset ? -1 : a.asset > b.asset ? 1 : 0);
    catalog.push(`inline constexpr std::uint16_t ${name}_by_path[] = {`);
    for (let i = 0; i < order.length; i += 20) {
        catalog.push(`    ${order.slice(i, i + 20).map(entry => entry.id).join(", ")},`);
    }
    catalog.push("};");
}
catalog.push("}");

for (const [name, lines] of [["animation_layout.hpp", layout], ["animation_catalog.hpp", catalog]]) {
    const target = path.join(output, name);
    const content = lines.join("\n") + "\n";
    if (check) {
        if (fs.readFileSync(target, "utf8").replace(/\r\n/g, "\n") !== content) {
            throw new Error(`Regenerate ${name} from the Lua catalog`);
        }
    } else {
        fs.writeFileSync(target, content);
    }
}
console.log(`Native catalog: ${lists.profiles.length} profiles, ${lists.montages.length} montages, ${lists.sequences.length} sequences`);
