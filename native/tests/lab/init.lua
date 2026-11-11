local root=_G.M1Root or assert(debug.getinfo(1,'S').source:gsub('^@',''):match('^(.*[/\\])'))
local native=assert(package.loadlib(root..'bin/QDCoopNativeM1.dll','qdenemy'))
local function valid(o)return o and o:IsValid()end
local function find(path)local o=StaticFindObject(path);assert(valid(o),path);return o end
local function fields(o)local out={};while valid(o)do o:ForEachProperty(function(p)local name=p:GetFName():ToString();if not out[name]then out[name]=p end end);o=o:GetSuperStruct()end;return out end
local function configureMetadata()
local base=find('/Game/Enemies/EnemyBase.EnemyBase_C');local mesh=find('/Script/Engine.SkeletalMeshComponent');local f=fields(base)
local args={base:GetAddress(),mesh:GetAddress(),assert(f.HP):GetAddress(),assert(f.Mesh):GetAddress()}
for _,name in ipairs({'bIsAttacking','bInRecoveryTime','bIsChasing','bHidden'})do args[#args+1]=assert(f[name]):GetAddress()end
for _,path in ipairs({'/Script/Engine.Actor:K2_GetActorLocation','/Script/Engine.Actor:K2_GetActorRotation','/Script/Engine.Actor:GetVelocity','/Script/Engine.Actor:IsActorBeingDestroyed','/Script/Engine.SceneComponent:K2_GetComponentToWorld','/Script/Engine.SceneComponent:IsVisible'})do
 local fn=find(path);args[#args+1]=fn:GetAddress();args[#args+1]=assert(fields(fn).ReturnValue):GetAddress()
end
local transform=fields(find('/Script/CoreUObject.Transform'))
for _,name in ipairs({'Rotation','Translation','Scale3D'})do args[#args+1]=assert(transform[name]):GetAddress()end
local actor=find('/Script/Engine.Actor:K2_SetActorLocationAndRotation');local component=find('/Script/Engine.SceneComponent:K2_SetWorldTransform')
local x,y=fields(actor),fields(component)
local placement={actor:GetAddress(),find('/Script/Engine.Actor'):GetAddress(),x.NewLocation:GetAddress(),x.NewRotation:GetAddress(),x.bSweep:GetAddress(),x.SweepHitResult:GetAddress(),x.bTeleport:GetAddress(),x.ReturnValue:GetAddress(),component:GetAddress(),find('/Script/Engine.SceneComponent'):GetAddress(),y.NewTransform:GetAddress(),y.bSweep:GetAddress(),y.SweepHitResult:GetAddress(),y.bTeleport:GetAddress(),transform.Rotation:GetAddress(),transform.Translation:GetAddress(),transform.Scale3D:GetAddress()}
for _,v in ipairs(placement)do args[#args+1]=v end
args[#args+1]=find('/Script/Engine.Actor:SetActorHiddenInGame'):GetAddress();args[#args+1]=find('/Script/Engine.SceneComponent:SetVisibility'):GetAddress()
assert(#args==42);assert(native('I'..table.concat(args,' '))==1,'native metadata validation failed')
end
configureMetadata()
if _G.M1 then M1.refreshMetadata=configureMetadata;return native('S')end
assert(native('H')==1,'native EngineTick registration failed')
local host=assert(_G.M1Role)=='host';local port=host and 48761 or 48762;local peer=host and 48762 or 48761
assert(native('O '..(host and 1 or 0)..' '..port..' 3819373611 1729814417 '..peer..' 0 0')==1,'native transport start failed')
_G.M1={native=native,role=_G.M1Role,root=root,entities={},generation=1,world=1,refreshMetadata=configureMetadata}
local context=M1
RegisterLoadMapPreHook(function()
 context.legacy=false;context.pattern=false;context.world=context.world+1
 assert(context.native('Z '..context.world)==1,'world epoch reset failed')
 context.mapReset=true
end)
return native('S')
