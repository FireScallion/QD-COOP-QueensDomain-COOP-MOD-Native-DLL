local M=assert(M1)
local function valid(o)return o and o:IsValid()end
local function find(path)local o=StaticFindObject(path);assert(valid(o),path);return o end
local function fields(c)local r={};while valid(c)do c:ForEachProperty(function(p)local n=p:GetFName():ToString();if not r[n]then r[n]=p end end);c=c:GetSuperStruct()end;return r end
local save=FindFirstOf('FS_GameInstance_C').ActiveSaveSlot
local soft=find('/Script/Engine.KismetSystemLibrary:Conv_ObjectToSoftObjectReference')
local spawnPath='/Game/Blueprints/ItemSystem/SpawnItemDrop.SpawnItemDrop_C:TrySpawningItem'
local meta={find('/Game/Blueprints/ItemSystem/SpawnItemDrop.Default__SpawnItemDrop_C'),find(spawnPath),find('/Script/Engine.Default__KismetSystemLibrary'),soft,find('/Script/Engine.Default__KismetStringLibrary'),find('/Script/Engine.KismetStringLibrary:Conv_StringToName'),find('/Script/QueensDomain.FS_Item'),find('/Game/Enemies/EnemyBase.EnemyBase_C:SetUpDropFog'),find('/Script/Engine.BlueprintMapLibrary:Map_Add'),assert(fields(save:GetClass()).EnemyDropDataA),assert(fields(save:GetClass()).EnemyDropDataB),assert(fields(soft).ReturnValue)}
for i,o in ipairs(meta)do meta[i]=o:GetAddress()end
assert(M.native('l '..table.concat(meta,' '))==1,'native drop metadata rejected')
local keys={'Chance_6_F7D575EC47153AD85A40D99F7290916A','PityModifier_27_B8F9C135469490AF613E2CBEFE28E997','Item_10_B0EB20114658902BDFBB5C9B0C9C4024','CoinCount_23_28CF6E804686C7685DDD0B9549FC3CDB','Chance2_16_57FB355047EE0AD89E8390BC9CD4BB85','PityModifier2_29_C2C42D7C4CDA82A5D6710C9748127E89','Item2_15_C69280724CF002967E616FB459A2A5CA','CoinCount2_24_1BDFAFF14AB42929887CE8910E3E4003'}
M.dropKeys=keys
local function command(e)
 local values={e.location.X,e.location.Y,e.location.Z,e.rotation.Pitch,e.rotation.Yaw,e.rotation.Roll,e.force.X,e.force.Y,e.force.Z,e.override.X,e.override.Y,e.override.Z,e.extra,e.pity}
 local body=string.pack('<I8I4I4I4I4i4',1,3,e.id,e.generation,e.flags,0)
 for _,n in ipairs(values)do body=body..string.pack('<f',n)end
 for _,n in ipairs(e.chances)do body=body..string.pack('<f',n)end
 for _,n in ipairs(e.counts)do body=body..string.pack('<i4',n)end
 for _,s in ipairs(e.text)do body=body..string.pack('<I2',#s)..s end
 local bytes=string.pack('<I4I4I8I8I8I4I4',0x45344e51,1,3819373611,1729814417,M.world,6,#body)..body
 return 'd '..bytes:gsub('.',function(c)return string.format('%02x',string.byte(c))end)
end
M.dropCommand=command
if M.role=='host'and not M.dropHook then
 M.dropHook=true;M.dropSourceCount=0
 RegisterHook(spawnPath,function(_,drop,loc,rot,nav,dropper,bounce,surface,gravity,extra,source,pity,force,skip,override,overridePos,context,first,second)
  local actor=dropper:get();if not valid(actor)or not actor:IsA('/Game/Enemies/EnemyBase.EnemyBase_C')then return end
  local id,generation=0,0;for i,r in pairs(M.entities)do if r.e:GetAddress()==actor:GetAddress()then id=i;generation=M.generation;break end end
  local asset=actor.Data:GetFullName():match(' (/Game/[%w_/%.-]+)$');if not asset then return end
  local d=drop:get();local ks=find('/Script/Engine.Default__KismetSystemLibrary')
  local flags=(nav:get()and 1 or 0)+(bounce:get()and 2 or 0)+(surface:get()and 4 or 0)+(gravity:get()and 8 or 0)+(skip:get()and 16 or 0)+(override:get()and 32 or 0)
  local e={id=id,generation=generation,flags=flags,location=loc:get(),rotation=rot:get(),force=force:get(),override=overridePos:get(),extra=extra:get(),pity=pity:get(),chances={d[keys[1]],d[keys[2]],d[keys[5]],d[keys[6]]},counts={d[keys[4]],d[keys[8]]},text={asset,source:get():ToString(),ks:Conv_SoftObjectReferenceToString(d[keys[3]]):ToString(),ks:Conv_SoftObjectReferenceToString(d[keys[7]]):ToString()}}
  local cmd=command(e);M.rewardPending[#M.rewardPending+1]=cmd;M.dropSourceCount=M.dropSourceCount+1;M.lastDropCommand=cmd
 end)
end
return 'native personal-drop metadata ready'
