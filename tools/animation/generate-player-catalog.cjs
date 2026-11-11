const fs = require("node:fs");
const path = require("node:path");

const root = path.resolve(__dirname, "../..");
const catalog = fs.readFileSync(path.join(root, "QDCoop-Direct/mod/Scripts/body_catalog.lua"), "utf8");
const clips = [...catalog.matchAll(
    /\{\["id"\]=(\d+),\["name"\]="([^"]+)",\["path"\]="([^"]*)",\["montage"\]=(true|false),\["source"\]=([\d.]+),\["length"\]=([\d.]+),\["group"\]=([0-7]),\["sections"\]=\{(.*?)\},\["blendOut"\]=([\d.]+),\["autoBlendOut"\]=(true|false),\["additive"\]=(true|false)\}/g
)];
if (clips.length !== 185 || clips.filter(row => row[4] === "true").length !== 108) {
    throw new Error("Unexpected preview3 body catalog shape");
}

const float = value => Number.isInteger(value) ? value + ".0f" : value + "f";
const rows = [
    "// Generated from preview3 body_catalog.lua in its original array order.",
    "#pragma once",
    "#include <string_view>",
    "namespace qdcoop::player {",
    "struct Clip { std::string_view source,name; float length,cooked,blend; unsigned lane; };",
    "inline constexpr Clip clips[] = {{},"
];
const loops = [];
const byPath = [];
const names = new Set();
for (const [index, row] of clips.entries()) {
    const [, id, name, source, montage, length, cooked, lane, sectionRows, blend] = row;
    const ordinal = index + 1;
    if ((source ? Number(id) !== ordinal : id !== "0") || names.has(name) ||
        (montage === "true") !== (Number(lane) > 0)) throw new Error("Invalid clip " + name);
    names.add(name);
    rows.push(`    {${JSON.stringify(source)},${JSON.stringify(name)},${float(+length)},${float(+cooked)},${float(+blend)},${lane}},`);
    if (source) byPath.push({ source, ordinal });
    const sections = [...sectionRows.matchAll(/\{\["name"\]="([^"]+)",\["next"\]="([^"]+)",\["time"\]=([\d.]+)\}/g)];
    if (sections.map(x => x[0]).join(",") !== sectionRows) throw new Error("Invalid sections for " + name);
    for (const [sectionIndex, [, section, next, start]] of sections.entries()) {
        if (section !== next) continue;
        const end = sections[sectionIndex + 1]?.[3] ?? length;
        if (+end <= +start) throw new Error("Invalid loop for " + name);
        loops.push(`    {${ordinal},${float(+start)},${float(+end)}},`);
    }
}
if (new Set(byPath.map(row => row.source)).size !== byPath.length) throw new Error("Duplicate source path");
byPath.sort((a, b) => a.source < b.source ? -1 : a.source > b.source ? 1 : 0);
rows.push("};", "struct SectionLoop { unsigned clip; float start,end; };",
    "inline constexpr SectionLoop section_loops[]{", ...loops, "};",
    "inline constexpr unsigned clips_by_path[]{",
    "    " + byPath.map(row => row.ordinal).join(","),
    "};", "}", "");
const output = path.join(root, "native/include/qdcoop/player_catalog.hpp");
const text = rows.join("\n");
if (process.argv.includes("--check")) {
    if (fs.readFileSync(output, "utf8").replace(/\r\n/g, "\n") !== text) throw new Error("Regenerate " + output);
} else {
    fs.writeFileSync(output, text);
}
console.log(`${clips.length} player clips, ${loops.length} looping sections`);
