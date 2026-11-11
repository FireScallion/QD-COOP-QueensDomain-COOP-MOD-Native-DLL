local M=assert(M1);local C=dofile(M.root..'legacy/enemy_animation_catalog.lua')
local function valid(o)return o and o:IsValid()end
local function find(p)local o=StaticFindObject(p);assert(valid(o),p);return o end
local function fields(c)local out={};while valid(c)do c:ForEachProperty(function(p)local n=p:GetFName():ToString();if not out[n]then out[n]=p end end);c=c:GetSuperStruct()end;return out end
local fns={'/Script/Engine.SkeletalMeshComponent:GetAnimInstance'}
for _,name in ipairs({'GetCurrentActiveMontage','Montage_Play','Montage_Stop','Montage_GetPosition','Montage_GetPlayRate','Montage_SetPlayRate','Montage_SetPosition','Montage_IsPlaying'})do fns[#fns+1]='/Script/Engine.AnimInstance:'..name end
local args={};for _,name in ipairs(fns)do args[#args+1]=find(name):GetAddress()end
assert(M.native('G '..table.concat(args,' '))==1,'animation function layout')
local configured={};local profiles={};for id,p in ipairs(C.profiles)do profiles[p.path]=id end
for id,r in ipairs(M.entities)do
 local source=r.e.Mesh:GetAnimInstance();assert(valid(source),'source AnimInstance')
 local class=source:GetClass();local path=class:GetFullName():match(' (/[^ ]+)$');local profile=assert(profiles[path],path)
 if M.role=='client'then r.pose:SetAnimClass(class);r.pose.VisibilityBasedAnimTickOption=0 end
 if not configured[profile]then
  local props=fields(class);local a={profile,class:GetAddress()}
  for _,name in ipairs({'bIsMoving','bIsChasing','bIsFalling','bIsAggro','bIsWandering','CurrentAimAngle'})do a[#a+1]=assert(props[name],name):GetAddress()end
  for _,input in ipairs(C.profiles[profile].inputs)do a[#a+1]=assert(props[input.name],input.name):GetAddress()end
  assert(M.native('A '..table.concat(a,' '))==1,'animation field layout '..profile);configured[profile]=true
 end
 assert(M.native('T '..id..' '..profile)==1,'animation binding '..id)
end
M.registerAnimationAsset=function(kind,id)
 local entry=assert((kind==0 and C.montages or C.sequences)[id]);local asset=StaticFindObject(entry.path)
 if not valid(asset)then asset=LoadAsset(entry.path)end;assert(valid(asset),entry.path)
 assert(M.native('Y '..kind..' '..id..' '..asset:GetAddress()..' '..asset.SequenceLength)==1,'animation asset')
 return asset
end
return 'native animation functions and profile fields registered for '..#M.entities..' enemies'
