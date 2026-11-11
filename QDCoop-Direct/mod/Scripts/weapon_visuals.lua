return function(dir,Q,valid,full,config)
 config=config or{}
 local C=dofile(dir..'weapon_catalog.lua');local rig=dofile(dir..'spear_rig.lua')
 local M={parts={},source={},fingerprint='',unknown={}};local weaponIds,meshIds,materialIds={},{},{}
 local armNames={};for i,m in ipairs(rig.mapping)do armNames[i]=m.source end
 local armReader=dofile(dir..'socket_batch.lua')(dir,armNames,valid,config.NativeBodyRead~=false)
 for i,v in ipairs(C.weapons)do weaponIds[v.class]=i end
 for i,v in ipairs(C.meshes)do meshIds[v.path]=i end
 for i,v in ipairs(C.materials)do materialIds[v]=i end
 local function asset(o)return valid(o) and full(o):match(' (/[^ ]+)$')or nil end
 local function nums(t)local out={t.Translation.X,t.Translation.Y,t.Translation.Z,t.Rotation.X,t.Rotation.Y,t.Rotation.Z,t.Rotation.W,t.Scale3D.X,t.Scale3D.Y,t.Scale3D.Z};return out end
 local function line(kind,slot,id,t)local p={kind,tostring(slot),tostring(id)};for _,n in ipairs(t)do p[#p+1]=string.format('%.5f',n)end;return table.concat(p,'|')end
 local function side(c,arms)
  local p=c;local hand=1
  for i=1,10 do
   if not valid(p)then return nil end
   if full(p)==full(arms)then return hand end
   local socket=p:GetAttachSocketName():ToString();if socket=='Weapon_L' or socket=='Hand_L'then hand=2 end
   p=p:GetAttachParent()
  end
 end
 local function material(c,index)
  local m=c:GetMaterial(index)
  for i=1,5 do
   if not valid(m)then return nil end
   local p=asset(m);if p and materialIds[p]then return materialIds[p]end
   local ok,parent=pcall(function()return m.Parent end);if not ok then return nil end;m=parent
  end
 end
 function M.capture(player,nativeBody)
  local arms=player.Arms;if not valid(arms)then return {}end
  arms.VisibilityBasedAnimTickOption=0
  local w=player.CurrentInHandWeapon;local id=valid(w) and weaponIds[asset(w:GetClass())] or 0
  local visual=valid(w) and w.CurrentVisuals or nil
  local off=player.Offhand_FlyingSword_WeaponScript;local offVisual=valid(off) and off.CurrentVisuals or nil
  local fingerprint=full(arms)..'|'..full(visual)..'|'..full(offVisual)
  if fingerprint~=M.fingerprint then
   M.fingerprint=fingerprint;M.source={};local seen={}
   local function add(c)
    if valid(c) and not seen[full(c)] and (c:IsA(StaticFindObject('/Script/Engine.StaticMeshComponent')) or c:IsA(StaticFindObject('/Script/Engine.SkeletalMeshComponent'))) and side(c,arms) then
     seen[full(c)]=true;M.source[#M.source+1]=c
    end
   end
   local visuals={visual,offVisual};for i=1,2 do local v=visuals[i];if valid(v)then
    v.VisualComps:ForEach(function(_,c)add(c:get())end)
    for _,c in ipairs(v:K2_GetComponentsByClass(StaticFindObject('/Script/Engine.MeshComponent')))do add(c:get())end
   end end
   table.sort(M.source,function(a,b)return full(a)<full(b)end)
  end
  local lines={'W|'..tostring(id or 0)};local maths=StaticFindObject('/Script/Engine.Default__KismetMathLibrary')
  local armsWorld=arms:K2_GetComponentToWorld();local hands={arms:GetSocketTransform(FName('Hand_R'),2),arms:GetSocketTransform(FName('Hand_L'),2)}
  local primary={0,0,0,0,0,0,1};local slot=0
  for _,c in ipairs(M.source)do if valid(c) and slot<4 then
   local hand=side(c,arms)
   if hand then
    local skeletal=c:IsA(StaticFindObject('/Script/Engine.SkeletalMeshComponent'))
    local mesh=skeletal and c.SkeletalMesh or c.StaticMesh;local meshPath=asset(mesh);local meshId=meshPath and meshIds[meshPath]
    if meshId then
     slot=slot+1;local t=nums(maths:MakeRelativeTransform(c:K2_GetComponentToWorld(),armsWorld));local h=hands[hand].Translation
     t[1]=t[1]-h.X;t[2]=t[2]-h.Y;t[3]=t[3]-h.Z
     local visible=c:IsVisible() and not c.bHiddenInGame and 1 or 0
     lines[#lines+1]='V|'..slot..'|'..meshId..'|'..hand..'|'..visible..'|'..line('',0,0,t):match('^|0|0|(.*)$')
     if slot==1 then for i=1,7 do primary[i]=t[i]end end
     if skeletal then
      c.VisibilityBasedAnimTickOption=0
      for i,name in ipairs(C.meshes[meshId].bones)do
       if c:GetBoneIndex(FName(name))>=0 then lines[#lines+1]=line('K',slot,i,nums(c:GetSocketTransform(FName(name),2)))end
      end
     end
     for i=0,math.min(c:GetNumMaterials(),8)-1 do local mid=material(c,i);if mid then lines[#lines+1]='D|'..slot..'|'..i..'|'..mid end end
    elseif meshPath then M.unknown[meshPath]=true end
   end
  end end
  if not nativeBody then
  local armData=config.NativeBodyRead~=false and armReader.read(arms)or nil
  local b={'B'};for index,mapping in ipairs(rig.mapping)do
   local q;if armData then local x,y,z,w=string.unpack('<ffff',armData,(index-1)*40+13);q={x,y,z,w}else q=Q.quat(arms:GetSocketTransform(FName(mapping.source),2).Rotation)end
   for _,n in ipairs(q)do b[#b+1]=string.format('%.5f',n)end
  end
  -- Keep the primary grip fields from the 0.4 pose record for diagnostics.
  for _,n in ipairs(primary)do b[#b+1]=string.format('%.5f',n)end;lines[#lines+1]=table.concat(b,'|')
  end
  M.localId=id or 0;M.localParts=slot;return lines
 end
 function M.effectTarget(path,hand)
  for _,part in pairs(M.parts)do if part.hand==hand and C.meshes[part.id].path==path and valid(part.c)then return part.c end end
 end
 function M.clearPresentation()
  for _,old in pairs(M.parts)do if valid(old.c)then old.c:SetVisibility(false,false);old.c:K2_DestroyComponent(old.c:GetOwner())end end
  M.parts={};M.actor=nil
 end
 function M.reset()M.parts={};M.actor=nil;M.source={};M.fingerprint=''end
 function M.apply(actor,pose,positions,view)
  if M.actor~=full(actor)then M.parts={};M.actor=full(actor)end
  local active={};local visuals=view and view.parts or {}
  for slot,v in pairs(visuals)do
   active[slot]=true;local old=M.parts[slot];local info=C.meshes[v.mesh]
   if old and old.id~=v.mesh then if valid(old.c)then old.c:SetVisibility(false,false);old.c:K2_DestroyComponent(actor)end;M.parts[slot]=nil;old=nil end
   if not old or not valid(old.c)then
    local kind=info.kind=='skeletal' and 'PoseableMeshComponent' or 'StaticMeshComponent'
    local c=actor:AddComponentByClass(StaticFindObject('/Script/Engine.'..kind),false,Q.transform({0,0,0},{0,0,0,1}),false)
    if info.kind=='skeletal'then c:SetSkeletalMesh(LoadAsset(info.path),true)else c:SetStaticMesh((LoadAsset(info.path)))end
    c:SetCollisionEnabled(0);c:K2_AttachToComponent(pose,FName('None'),0,0,0,false)
    old={id=v.mesh,c=c,materials={}};M.parts[slot]=old
   end
   old.hand=v.hand;local c=old.c;local t=v.transform;local hand=positions[v.hand==2 and 'hand_l' or 'hand_r']
   c:K2_SetRelativeTransform(Q.transform(Q.add(hand,{t[1],t[2],t[3]}),{t[4],t[5],t[6],t[7]},{t[8],t[9],t[10]}),false,{},true)
   c:SetVisibility(v.visible==1,false)
   -- The catalogue retains parent-before-child skeleton order.
   for i,name in ipairs(info.bones)do local bt=v.bones and v.bones[i];if bt then c:SetBoneTransformByName(FName(name),Q.transform({bt[1],bt[2],bt[3]},{bt[4],bt[5],bt[6],bt[7]},{bt[8],bt[9],bt[10]}),1)end end
   for index,id in pairs(v.materials or {})do if old.materials[index]~=id then c:SetMaterial(index,(LoadAsset(C.materials[id])));old.materials[index]=id end end
  end
  for slot,old in pairs(M.parts)do if not active[slot]then if valid(old.c)then old.c:SetVisibility(false,false);old.c:K2_DestroyComponent(actor)end;M.parts[slot]=nil end end
  M.remoteId=view and view.weapon or 0
 end
 function M.status()local n=0;for _ in pairs(M.unknown)do n=n+1 end;return string.format('localWeapon=%d localParts=%d remoteWeapon=%d unknownMeshes=%d',M.localId or 0,M.localParts or 0,M.remoteId or 0,n)..' '..armReader.status('armsRead')end
 return M
end
