return function(valid,full,gs,system)
 local M={};local base='/Game/QDCoop/QDCoopPlacedItem.QDCoopPlacedItem_C'
 local meshClass=StaticFindObject('/Script/Engine.StaticMeshComponent')
 local skeletalClass=StaticFindObject('/Script/Engine.SkeletalMeshComponent')
 local function path(o)return valid(o)and full(o):match(' (/Game/.*)$')or ''end
 -- Read component templates, never spawn a quest pedestal or an NPC pickup BP.
 function M.template(item)
  local classes={item['Drop BP / Fallback Visual BP'],item['Pickup BP']}
  local soft=system:Conv_SoftClassReferenceToString(item['Default Visual BP']):ToString()
  if soft~=''then classes[#classes+1]=system:LoadClassAsset_Blocking(system:Conv_SoftClassPathToSoftClassRef(system:MakeSoftClassPath(soft)))end
  for _,c in ipairs(classes)do
   if valid(c)then
    local candidates={}
    local function add(t)
     if valid(t)and((t:IsA(meshClass)and valid(t.StaticMesh))or(t:IsA(skeletalClass)and valid(t.SkeletalMesh)))then candidates[#candidates+1]=t end
    end
    local handler=c.InheritableComponentHandler
    if valid(handler)then handler.Records:ForEach(function(_,v)add(v:get().ComponentTemplate)end)end
    local scs=c.SimpleConstructionScript
    if valid(scs)then scs.AllNodes:ForEach(function(_,v)add(v:get().ComponentTemplate)end)end
    for _,t in ipairs(candidates)do if full(t):find('PickupGraphics',1,true)then return t end end
    if candidates[1]then return candidates[1]end
   end
  end
 end
 function M.create(player,r,item)
  local t=M.template(item);if not t then return end
  local transform={Translation={X=r.x,Y=r.y,Z=r.z},Rotation={X=0,Y=0,Z=0,W=1},Scale3D={X=1,Y=1,Z=1}}
  local cls=StaticFindObject(base);assert(valid(cls),'Placed item asset unavailable')
  local a=gs:BeginDeferredActorSpawnFromClass(player,cls,transform,1,nil)
  assert(valid(a),'Placed item spawn failed')
  a.TransientPickup=true;a.IsADrop=false;a.RequireActivation=false;a.GUIDs={};a.ItemsToGrant:Empty()
  gs:FinishSpawningActor(a,transform)
  a.ItemsToGrant:Empty();a.GUIDs={};a.ActorsToActivateOnPickup={};a.IsADrop=true
  local m=a.PickupGraphics
  if t:IsA(skeletalClass)then
   local identity={Translation={X=0,Y=0,Z=0},Rotation={X=0,Y=0,Z=0,W=1},Scale3D={X=1,Y=1,Z=1}}
   local sk=a:AddComponentByClass(skeletalClass,true,identity,true)
   assert(valid(sk),'Placed skeletal mesh creation failed')
   sk:SetSkeletalMesh(t.SkeletalMesh,true)
   assert(sk:K2_AttachToComponent(m,FName('None'),2,2,2,false))
   a:FinishAddComponent(sk,true,identity);m=sk
  else m:SetStaticMesh(t.StaticMesh)end
  for i=0,t:GetNumMaterials()-1 do local mat=t:GetMaterial(i);if valid(mat)then m:SetMaterial(i,mat)end end
  m:SetRelativeScale3D(t.RelativeScale3D)
  m:K2_SetRelativeRotation(t.RelativeRotation,false,{},true)
  m:SetCollisionEnabled(0)
  a.InteractCollision:SetCollisionEnabled(1);a.InteractCollision:SetSphereRadius(32,true)
  a.Tags={FName('QDCoopPlacedItem'),FName(r.id)}
  -- The loot-hint mod recognizes this native pickup base and may add its normal sparkle.
  return a
 end
 function M.destroy(a)if valid(a)then a:K2_DestroyActor()end end
 return M
end
