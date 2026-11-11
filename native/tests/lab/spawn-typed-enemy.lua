local M=assert(M1);local C=dofile(M.root..'legacy/enemy_animation_catalog.lua')
local profile=_G.M2SpawnProfile or 3;local desired=assert(C.profiles[profile])
local gs=StaticFindObject('/Script/Engine.Default__GameplayStatics');local loaded={}
gs:GetAllActorsOfClass(M.player,StaticFindObject('/Game/Enemies/EnemyBase.EnemyBase_C'),loaded)
local class
for _,item in ipairs(loaded)do local e=item:get();local ai=e.Mesh:GetAnimInstance();if ai:IsValid()and ai:GetClass():GetFullName():match(' (/[^ ]+)$')==desired.path then class=e:GetClass();break end end
if not class and _G.M2SpawnClass then class=LoadAsset(_G.M2SpawnClass) end
assert(class and class:IsValid(),'no loaded template for profile '..profile)
local t={Translation={X=17200,Y=-7000,Z=61},Rotation={X=0,Y=0,Z=0,W=1},Scale3D={X=1,Y=1,Z=1}}
local e=gs:BeginDeferredActorSpawnFromClass(M.player,class,t,1,nil);assert(e:IsValid());gs:FinishSpawningActor(e,t)
e:SetActorEnableCollision(false);e:SetActorTickEnabled(false);e.CharacterMovement:StopMovementImmediately();e.CharacterMovement:SetComponentTickEnabled(false)
local controller=e:GetController();if controller:IsValid()then controller:StopMovement();controller:SetActorTickEnabled(false);if controller.BrainComponent:IsValid()then controller.BrainComponent:StopLogic('QD M2 typed animation fixture')end end
local valid=function(o)return o and o:IsValid()end
local quiet=dofile(M.root..'legacy/enemy_quiescence.lua')(valid);local paused=quiet.pause(e)
e.CustomTimeDilation=0;e.HP=131;e.Mesh:SetComponentTickEnabled(false)
local id=#M.entities+1;local r={e=e,id=id,quiet=paused,profile=profile};M.entities[id]=r
assert(M.native('R',id,1,e:GetAddress(),e.Mesh:GetAddress())==1)
if M.role=='client'then
 e:SetActorHiddenInGame(true);e.Mesh:SetVisibility(false,false)
 local visual=gs:BeginDeferredActorSpawnFromClass(M.player,StaticFindObject('/Script/Engine.SkeletalMeshActor'),t,1,nil);gs:FinishSpawningActor(visual,t);visual:SetActorEnableCollision(false)
 local pose=visual:GetComponentByClass(StaticFindObject('/Script/Engine.SkeletalMeshComponent'));pose:SetSkeletalMesh(e.Mesh.SkeletalMesh,true);pose:SetCollisionEnabled(0);pose:SetGenerateOverlapEvents(false)
 for i=0,e.Mesh:GetNumMaterials()-1 do pose:SetMaterial(i,e.Mesh:GetMaterial(i))end
 pose:SetComponentTickEnabled(true);pose:SetVisibility(true,false);r.visual=visual;r.pose=pose
 assert(M.native('J',id,1,visual:GetAddress(),pose:GetAddress(),StaticFindObject('/Script/Engine.ActorComponent:GetOwner'):GetAddress())==1)
end
return 'spawned genuine profile '..profile..' entity '..id..' class '..class:GetFullName()

