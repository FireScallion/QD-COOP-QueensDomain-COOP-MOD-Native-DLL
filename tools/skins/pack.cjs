// node pack.cjs <cooked-QDCoopSkins-directory> <UnrealPak.exe> <output.pak>
const fs=require('fs'),path=require('path'),cp=require('child_process'),assert=require('assert');
const[baseInput,exeInput,outInput]=process.argv.slice(2);assert(baseInput&&exeInput&&outInput,'See usage comment');
const base=path.resolve(baseInput),exe=path.resolve(exeInput),out=path.resolve(outInput);
function files(d,p=''){return fs.readdirSync(d,{withFileTypes:true}).flatMap(e=>e.isDirectory()?files(path.join(d,e.name),p+e.name+'/'):[p+e.name]);}
const list=files(base);assert(list.length>0&&list.every(n=>/\.(uasset|uexp|ubulk)$/.test(n)),'Unexpected cook output');
fs.mkdirSync(path.dirname(out),{recursive:true});const response=out+'.response.txt';
fs.writeFileSync(response,list.map(n=>'"'+path.join(base,n)+'" "../../../QueensDomain/Content/QDCoopSkins/'+n+'"').join('\n'));
for(const args of [[out,'-Create='+response],[out,'-Test']]){const p=cp.spawnSync(exe,args,{windowsHide:true,stdio:'inherit'});assert.equal(p.status,0,'UnrealPak failed');}
