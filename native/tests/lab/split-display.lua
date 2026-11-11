local M=assert(M1);assert(M.role=='client')
local gs=StaticFindObject('/Script/Engine.Default__GameplayStatics')
local cls=StaticFindObject('/Script/Engine.SkeletalMeshActor')
local meshClass=StaticFindObject('/Script/Engine.SkeletalMeshComponent')
local owner=StaticFindObject('/Script/Engine.ActorComponent:GetOwner')
local quiet=dofile(M.root..'legacy/enemy_quiescence.lua')(function(o)return o and o:IsValid()end)
for i,r in ipairs(M.entities)do
 local e=r.e;assert(e:IsValid())
 r.quiet=quiet.pause(e)
 e.CustomTimeDilation=0;e:SetActorHiddenInGame(true);e:SetActorEnableCollision(false)
 e.Mesh:SetComponentTickEnabled(false);e.Mesh:SetVisibility(false,false)
 local t=e:GetTransform()
 local visual=gs:BeginDeferredActorSpawnFromClass(M.player,cls,t,1,nil);assert(visual:IsValid());gs:FinishSpawningActor(visual,t)
 visual:SetActorEnableCollision(false)
 local pose=visual:GetComponentByClass(meshClass);assert(pose:IsValid())
 pose:SetSkeletalMesh(e.Mesh.SkeletalMesh,true);pose:SetCollisionEnabled(0);pose:SetGenerateOverlapEvents(false)
 for j=0,e.Mesh:GetNumMaterials()-1 do pose:SetMaterial(j,e.Mesh:GetMaterial(j))end
 pose:SetComponentTickEnabled(true);pose:SetVisibility(true,false)
 assert(M.native('J',i,M.generation,visual:GetAddress(),pose:GetAddress(),owner:GetAddress())==1,'split registration '..i)
 r.visual=visual;r.pose=pose
end
return 'split registered '..#M.entities..' controlled gameplay actors and cosmetic displays'
