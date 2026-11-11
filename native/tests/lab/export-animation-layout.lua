local C=dofile(M1.root..'legacy/enemy_animation_catalog.lua')
local skeletons={};local nextId=0
local function skeleton(path)if not skeletons[path]then nextId=nextId+1;skeletons[path]=nextId end;return skeletons[path]end
local lines={'// Generated from the preview3 enemy_animation_catalog.lua; IDs retain Lua indexing.','#pragma once','#include <array>','#include <cstdint>','namespace qdcoop::enemy::animation {','struct Layout { unsigned skeleton,count; std::array<char,8> kinds; };','inline constexpr Layout layouts[] = {{0,0,{}},'}
for _,p in ipairs(C.profiles)do
 local kinds={};for _,input in ipairs(p.inputs)do for _=1,(input.kind=='r'or input.kind=='v')and 3 or 1 do kinds[#kinds+1]="'"..input.kind.."'"end end
 assert(#kinds==p.count and #kinds<=8)
 lines[#lines+1]='{'..skeleton(p.skeleton)..','..#kinds..',{'..table.concat(kinds,',')..'}},'
end
lines[#lines+1]='};'
for _,name in ipairs({'montages','sequences'})do
 local items={'0'};for _,a in ipairs(C[name])do items[#items+1]=tostring(skeleton(a.skeleton))end
 lines[#lines+1]='inline constexpr unsigned '..name..'[] = {'..table.concat(items,',')..'};'
end
local loops={Boner_Trap_Montage=.96666664,Boner_Trap_Heavy_Montage=3.3333335,Boner_CasketTrap_Montage=2.4583333}
lines[#lines+1]='inline double loop_end(unsigned id) { switch(id) {'
for i,m in ipairs(C.montages)do for name,t in pairs(loops)do if m.path=='/Game/Enemies/Boner_00/Animation/'..name..'.'..name then lines[#lines+1]='case '..i..': return '..string.format('%.9g',t)..';'end end end
lines[#lines+1]='default: return 0; } }'
lines[#lines+1]='}'
local file=assert(io.open(M1.root..'animation_layout.hpp','wb'));file:write(table.concat(lines,'\n')..'\n');file:close()
return 'exported '..#C.profiles..' profiles, '..#C.montages..' montages, '..#C.sequences..' sequences'
