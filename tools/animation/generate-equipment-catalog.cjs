const fs = require("node:fs");
const path = require("node:path");
const root = path.resolve(__dirname, "../..");
const script = path.join(root, "QDCoop-Direct/mod/Scripts/weapon_catalog.lua");
const lua = fs.readFileSync(script, "utf8");
const sections = lua.match(/return \{\["weapons"\]=\{(.*?)\},\["meshes"\]=\{(.*?)\},\["materials"\]=\{(.*?)\}\}/);
if (!sections) throw new Error("Unexpected preview3 equipment catalog");
const weapons = [...sections[1].matchAll(
    /\{\["class"\]="([^"]+)",\["name"\]="[^"]+",\["item"\]="([^"]+)",\["visual"\]="[^"]*"\}/g
)];
const meshes = [...sections[2].matchAll(
    /\{\["path"\]="([^"]+)",\["kind"\]="(static|skeletal)",\["bones"\]=\{([^}]*)\}\}/g
)];
const materials = [...sections[3].matchAll(/"([^"]+)"/g)];
for (const [index, rows] of [weapons, meshes, materials].entries()) {
    if (rows.map(row => row[0]).join(",") !== sections[index + 1] ||
        new Set(rows.map(row => row[1])).size !== rows.length || rows.length > 65534) {
        throw new Error("Incomplete equipment catalog section " + index);
    }
}

const quote = JSON.stringify;
const boneNames = [];
let largest = 0;
const meshRows = meshes.map(([, asset, kind, body]) => {
    const bones = [...body.matchAll(/"([^"]+)"/g)];
    if (bones.map(row => row[0]).join(",") !== body ||
        new Set(bones.map(row => row[1])).size !== bones.length ||
        (kind === "skeletal") !== (bones.length > 0)) {
        throw new Error("Unexpected skeleton layout for " + asset);
    }
    const first = boneNames.length;
    largest = Math.max(largest, bones.length);
    boneNames.push(...bones.map(row => row[1]));
    return `    {${quote(asset)},${kind === "skeletal"},${first},${bones.length}},`;
});
if (largest > 28 || boneNames.length > 65535) throw new Error("Skeleton catalog exceeds the pose wire limit");
const rows = [
    "// Generated from preview3 weapon_catalog.lua, preserving all numeric IDs.",
    "#pragma once", "#include <string_view>",
    "namespace qdcoop::player {",
    "struct WeaponEntry { std::string_view path,item; };",
    "struct EquipmentMesh { std::string_view path; bool skeletal; unsigned first_bone,bone_count; };",
    `inline constexpr unsigned max_equipment_bones=${largest};`,
    "inline constexpr WeaponEntry weapons[] = {{},",
    ...weapons.map(([, asset, item]) => `    {${quote(asset)},${quote(item)}},`),
    "};",
    "inline constexpr EquipmentMesh equipment_meshes[] = {{},",
    ...meshRows,
    "};",
    "inline constexpr std::string_view equipment_bones[] = {",
    ...boneNames.map(name => "    " + quote(name) + ","),
    "};",
    "inline constexpr std::string_view equipment_materials[] = {\"\",",
    ...materials.map(([, asset]) => "    " + quote(asset) + ","),
    "};",
];
for (const [name, values] of Object.entries({weapons, equipment_meshes: meshes, equipment_materials: materials})) {
    const order = values.map(([, asset], index) => ({ asset, id: index + 1 }));
    order.sort((a, b) => a.asset < b.asset ? -1 : a.asset > b.asset ? 1 : 0);
    rows.push(`inline constexpr unsigned ${name}_by_path[]{`,
        "    " + order.map(entry => entry.id).join(","), "};");
}
rows.push("}", "");
const output = path.join(root, "native/include/qdcoop/equipment_catalog.hpp");
const text = rows.join("\n");
if (process.argv.includes("--check")) {
    if (fs.readFileSync(output, "utf8").replace(/\r\n/g, "\n") !== text) throw new Error("Regenerate " + output);
} else {
    fs.writeFileSync(output, text);
}
console.log(`${weapons.length} weapon classes, ${meshes.length} meshes, ${materials.length} materials, ${boneNames.length} ordered bones`);
