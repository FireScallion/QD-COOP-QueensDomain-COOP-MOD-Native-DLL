return function(dir,valid,full)
 local rig=dofile(dir..'spear_rig.lua');local M={};local basis={0,0,0,1}
 local function mul(a,b)return {a[4]*b[1]+a[1]*b[4]+a[2]*b[3]-a[3]*b[2],a[4]*b[2]-a[1]*b[3]+a[2]*b[4]+a[3]*b[1],a[4]*b[3]+a[1]*b[2]-a[2]*b[1]+a[3]*b[4],a[4]*b[4]-a[1]*b[1]-a[2]*b[2]-a[3]*b[3]}end
 local function inv(q)return {-q[1],-q[2],-q[3],q[4]}end
 local function rot(q,v)local r=mul(mul(q,{v[1],v[2],v[3],0}),inv(q));return {r[1],r[2],r[3]}end
 local function add(a,b)return {a[1]+b[1],a[2]+b[2],a[3]+b[3]}end
 local function quat(q)return {q.X,q.Y,q.Z,q.W}end
 local function vec(v)return {v.X,v.Y,v.Z}end
 local function trans(p,q)return {Translation={X=p[1],Y=p[2],Z=p[3]},Rotation={X=q[1],Y=q[2],Z=q[3],W=q[4]},Scale3D={X=1,Y=1,Z=1}}end
 local identity=trans({0,0,0},{0,0,0,1})
 local offsets={};for i,m in ipairs(rig.mapping)do offsets[i]=mul(inv(mul(basis,m.bind)),rig.target[m.target].cq)end
 local function attached(mesh,arms)
  local parent=mesh
  for i=1,8 do if not valid(parent)then return false end;if full(parent)==full(arms)then return true end;parent=parent:GetAttachParent()end
  return false
 end
 function M.capture(player)
  M.hasLocal=false
  local w=player.CurrentInHandWeapon
  if not valid(w) or not full(w):find('BasicSpearEvolved_WeaponScript_C',1,true) then return nil end
  local arms=player.Arms;if not valid(arms)then return nil end
  arms.VisibilityBasedAnimTickOption=0
  local mesh=M.sourceMesh
  if not valid(mesh) or not attached(mesh,arms)then
   mesh=nil
   for _,v in ipairs(FindAllOf('BasicSpearEvolved_WeaponVisuals_C') or {})do
    if valid(v)then local candidate=v:GetComponentByClass(StaticFindObject('/Script/Engine.SkeletalMeshComponent'));if valid(candidate) and attached(candidate,arms)then mesh=candidate;break end end
   end
   M.sourceMesh=mesh
  end
  if not valid(mesh)then return nil end
  local mathlib=StaticFindObject('/Script/Engine.Default__KismetMathLibrary')
  local weapon=mathlib:MakeRelativeTransform(mesh:K2_GetComponentToWorld(),arms:K2_GetComponentToWorld())
  local hand=arms:GetSocketTransform(FName('Hand_R'),2);local delta={weapon.Translation.X-hand.Translation.X,weapon.Translation.Y-hand.Translation.Y,weapon.Translation.Z-hand.Translation.Z}
  local values={};for _,m in ipairs(rig.mapping)do local q=quat(arms:GetSocketTransform(FName(m.source),2).Rotation);for _,n in ipairs(q)do values[#values+1]=n end end
  for _,n in ipairs(delta)do values[#values+1]=n end;for _,n in ipairs(quat(weapon.Rotation))do values[#values+1]=n end
  local parts={'B'};for _,n in ipairs(values)do parts[#parts+1]=string.format('%.5f',n)end;M.hasLocal=true;return table.concat(parts,'|')
 end
 function M.reset()M.actor=nil;M.pose=nil;M.weapon=nil;M.sourceMesh=nil;M.hasRemote=false end
 function M.status()return 'localSpear='..tostring(M.hasLocal==true)..' remoteSpear='..tostring(M.hasRemote==true)..' mappedBones=36' end
 function M.apply(actor,base,values)
  if not values then
   M.hasRemote=false
   if valid(M.pose)then M.pose:SetVisibility(false,false)end;if valid(M.weapon)then M.weapon:SetVisibility(false,false)end
   base:SetVisibility(true,false);return
  end
  if not valid(M.actor) or full(M.actor)~=full(actor) then
   M.reset();M.actor=actor
   M.pose=actor:AddComponentByClass(StaticFindObject('/Script/Engine.PoseableMeshComponent'),false,identity,false)
   M.pose:SetSkeletalMesh(base.SkeletalMesh,true);M.pose:SetCollisionEnabled(0)
   M.pose:K2_SetRelativeRotation({Pitch=0,Yaw=-90,Roll=0},false,{},true)
   M.weapon=actor:AddComponentByClass(StaticFindObject('/Script/Engine.SkeletalMeshComponent'),false,identity,false)
   M.weapon:SetSkeletalMesh(LoadAsset('/Game/Items/Weapons/BasicSpearEvolved/BasicSpearEvolved_Mesh.BasicSpearEvolved_Mesh'),true);M.weapon:SetCollisionEnabled(0)
   M.weapon:K2_AttachToComponent(M.pose,FName('None'),0,0,0,false)
  end
  M.hasRemote=true
  base:SetVisibility(false,false);M.pose:SetVisibility(true,false);M.weapon:SetVisibility(true,false)
  local v=values
  local overrides={};for i,m in ipairs(rig.mapping)do local k=(i-1)*4+1;overrides[m.target]=mul(mul(basis,{v[k],v[k+1],v[k+2],v[k+3]}),offsets[i])end
  local positions,rotations={},{};local rightHand
  for i,b in ipairs(rig.target)do
   local parent=b.parent;local q=overrides[i]or(parent>0 and mul(rotations[parent],b.q)or b.q)
   local p=parent>0 and add(positions[parent],rot(rotations[parent],b.p))or b.p
   positions[i]=p;rotations[i]=q
   if i>=41 and i<=90 then M.pose:SetBoneTransformByName(FName(b.name),trans(p,q),1)end
   if b.name=='hand_r' then rightHand=p end
  end
  local wp=add(rightHand,rot(basis,{v[145],v[146],v[147]}));local wq=mul(basis,{v[148],v[149],v[150],v[151]})
  M.weapon:K2_SetRelativeTransform(trans(wp,wq),false,{},true)
 end
 return M
end
