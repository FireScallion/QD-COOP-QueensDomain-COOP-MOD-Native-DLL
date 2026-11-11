// node generate-personal-pickups.cjs output.lua maps-json metadata-json [...]
// Inputs are CUE4Parse JSON exports, retaining QueensDomain/Content hierarchy.
const fs=require('fs'),path=require('path');
const [output,maps,...roots]=process.argv.slice(2);
if(!output||!maps||!roots.length)throw Error('Expected output.lua maps-json metadata-json [...]');
function files(d){return fs.readdirSync(d,{withFileTypes:true}).flatMap(e=>e.isDirectory()?files(path.join(d,e.name)):[path.join(d,e.name)]);}
function read(f){return JSON.parse(fs.readFileSync(f,'utf8').replace(/^\uFEFF/,''));}
function reference(r){if(!r?.ObjectPath)return null;return r.ObjectPath.replace(/\.\d+$/,'').replace('QueensDomain/Content','/Game')+'.'+r.ObjectName.match(/'([^']+)'/)?.[1];}
const parents=new Map();
for(const root of roots)for(const f of files(root).filter(f=>f.endsWith('.json'))){const exports=read(f);if(!Array.isArray(exports))continue;for(const o of exports)if(o.Type==='BlueprintGeneratedClass'){
 const pkg=path.relative(root,f).replaceAll('\\','/').replace(/\.json$/,'').replace('QueensDomain/Content','/Game');parents.set(pkg+'.'+o.Name,reference(o.Super));
}}
const missing=new Set(),ids=new Set(),classes=new Set();let actors=0;
function pickup(p,depth=0){if(p==='/Game/Blueprints/CoreItemPickupActor.CoreItemPickupActor_C')return true;if(p?.startsWith('/Game/')&&!parents.has(p))missing.add(p);return depth<30&&parents.has(p)&&pickup(parents.get(p),depth+1);}
for(const f of files(maps).filter(f=>f.endsWith('.json')))for(const o of read(f))if(o.Properties?.GUIDs){
 actors++;const cls=o.Class?.match(/'([^']+)'/)?.[1]?.replace('QueensDomain/Content','/Game');if(!pickup(cls))continue;classes.add(cls);
 for(const guid of o.Properties.GUIDs){const values=guid.split('-').map(v=>parseInt(v,16)|0);if(values.length!==4)throw Error('Invalid GUID '+guid);ids.add(values.join(','));}
}
if(missing.size)throw Error('Export metadata for unresolved classes before generating:\n'+[...missing].sort().join('\n'));
fs.writeFileSync(output,'-- Extracted map GUIDs whose actual Blueprint superclass is CoreItemPickupActor.\n-- Keeps unloaded personal pickups out of shared world state and saved overlays.\nreturn {\n'+[...ids].sort().map(id=>' ['+JSON.stringify(id)+']=true,').join('\n')+'\n}\n');
console.log(JSON.stringify({actors,pickupClasses:classes.size,pickupGUIDs:ids.size}));
