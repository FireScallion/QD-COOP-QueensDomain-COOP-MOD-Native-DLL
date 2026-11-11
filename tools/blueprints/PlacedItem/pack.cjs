const fs=require('fs'),crypto=require('crypto'),path=require('path');
const input=process.argv[2]||'.',output=process.argv[3]||'QDCoop-PlacedItem.pak';
const u32=n=>{const b=Buffer.alloc(4);b.writeUInt32LE(n);return b},u64=n=>{const b=Buffer.alloc(8);b.writeBigUInt64LE(BigInt(n));return b},sha=b=>crypto.createHash('sha1').update(b).digest(),str=s=>Buffer.concat([u32(Buffer.byteLength(s)+1),Buffer.from(s+'\0')]);
// UE legacy v3, uncompressed, unencrypted. No original paths are overridden.
let offset=0;const data=[],idx=[];for(const ext of ['uasset','uexp']){const name='QueensDomain/Content/QDCoop/QDCoopPlacedItem.'+ext,b=fs.readFileSync(path.join(input,'QDCoopPlacedItem.'+ext));const entry=o=>Buffer.concat([u64(o),u64(b.length),u64(b.length),u32(0),sha(b),Buffer.alloc(5)]);const h=entry(0);data.push(h,b);idx.push(str(name),entry(offset));offset+=h.length+b.length}
const index=Buffer.concat([str('../../../'),u32(2),...idx]);const footer=Buffer.concat([u32(0x5A6F12E1),u32(3),u64(offset),u64(index.length),sha(index)]);
const out=output;fs.writeFileSync(out,Buffer.concat([...data,index,footer]));console.log(out);
