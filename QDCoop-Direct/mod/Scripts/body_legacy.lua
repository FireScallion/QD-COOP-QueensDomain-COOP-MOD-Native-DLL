return function(dir,valid,full,C,sharedWeapons)
 C=C or{}
 local skin=dofile(dir..'skin_visual.lua')(dir,valid)
 local Q=dofile(dir..'pose_math.lua');local rig=dofile(dir..'spear_rig.lua');local motionRig=dofile(dir..'motion_rig.lua')
 local Locomotion=dofile(dir..'locomotion.lua')(Q);local weapons=sharedWeapons or dofile(dir..'weapon_visuals.lua')(dir,Q,valid,full,C)
 local committed=dofile(dir..'pose_commit.lua')();local boneNames,driverNames={},{}
 for i,b in ipairs(rig.target)do boneNames[i]=FName(b.name)end
 for _,b in ipairs(motionRig.mapping)do driverNames[b.name]=FName(b.name)end
 local rootName=FName('root_x')
 local socketNames,rootSlot={},nil;for i,b in ipairs(motionRig.mapping)do socketNames[i]=b.name;if b.name=='root_x'then rootSlot=i end end
 if not rootSlot then socketNames[#socketNames+1]='root_x';rootSlot=#socketNames end
 local reader=dofile(dir..'socket_batch.lua')(dir,socketNames,valid,C.NativeBodyRead~=false)
 local batch=dofile(dir..'pose_batch.lua')(dir,rig,valid,C)
 local M={};local referencePositions,referenceRotations={},{};local names,changed,armOffsets={},{},{}
 for i,b in ipairs(rig.target)do
  names[b.name]=i;local parent=b.parent
  referenceRotations[i]=parent>0 and Q.mul(referenceRotations[parent],b.q)or b.q
  referencePositions[i]=parent>0 and Q.add(referencePositions[parent],Q.rot(referenceRotations[parent],b.p))or b.p
 end
 for i,b in ipairs(rig.mapping)do armOffsets[i]=Q.mul(Q.inv(b.bind),rig.target[b.target].cq);changed[b.target]=true end
 for _,b in ipairs(motionRig.mapping)do changed[b.target]=true end;changed[names.head_x]=true
 local builder=dofile(dir..'pose_builder.lua')(rig,names.root_x)
 local prone=dofile(dir..'downed_pose.lua')(Q,rig,referencePositions,names)
 local function spawnComponent(actor,kind)return actor:AddComponentByClass(StaticFindObject('/Script/Engine.'..kind),false,Q.transform({0,0,0},{0,0,0,1}),false)end
 function M.reset()
  committed.reset();M.boneWrites=0;M.boneSkips=0;M.lastGroundOffset=nil
  M.actor=nil;M.pose=nil;M.driver=nil;M.clip=nil;M.lastTime=nil;M.motion=Locomotion.new();M.overrides={};M.pelvis=nil;M.groundOffset=0;M.downId=0;M.fallAt=nil;M.riseAt=nil;M.wasProne=false;weapons.reset()
 end
 M.reset()
 function M.releasePresentation()
  skin.reset();M.skin=nil
  for _,key in ipairs({'driver','pose'})do local c=M[key];if valid(c)then c:SetVisibility(false,false);c:SetComponentTickEnabled(false);c:K2_DestroyComponent(M.actor)end;M[key]=nil end
  M.actor=nil;weapons.clearPresentation()
 end
 function M.capture(player,nativeBody)
  local lines=weapons.capture(player,nativeBody);local controller=player:GetController();local pitch=valid(controller) and controller:GetControlRotation().Pitch or 0
  lines[#lines+1]=string.format('L|%.4f|%d|%.4f',Q.pitch(pitch),player.Running and 1 or 0,tonumber(player.InternalDefaultMaxWalkSpeed)or 400)
  return lines
 end
 function M.apply(actor,base,snapshot,now,downId)
  downId=downId or 0
  local selected=C.SkinCatalog.get(snapshot.view and snapshot.view.skin).id
  if not valid(M.actor) or full(M.actor)~=full(actor)or M.skin~=selected then
   M.releasePresentation()
   M.reset();M.actor=actor
   M.pose=spawnComponent(actor,'PoseableMeshComponent');skin.apply(M.pose,selected);M.skin=selected;M.pose:SetCollisionEnabled(0)
   M.driver=spawnComponent(actor,'SkeletalMeshComponent');M.driver:SetSkeletalMesh(LoadAsset(motionRig.mesh),true);M.driver:SetCollisionEnabled(0)
   M.driver:SetVisibility(false,false);M.driver.CastShadow=false;M.driver.VisibilityBasedAnimTickOption=0
   base:SetVisibility(false,false);base:SetComponentTickEnabled(false)
  end
  local clock=snapshot.sampleTime or now
  local dt=M.lastTime and Q.clamp(clock-M.lastTime,0,.1)or .016;M.lastTime=clock
  local state=M.motion:update(snapshot.motion,snapshot.view,dt,snapshot.yaw);M.state=state.state
  if M.clip~=state.clip then M.driver:PlayAnimation(LoadAsset(motionRig.clips[state.clip]),false);M.driver:SetPlayRate(0);M.clip=state.clip end
  -- Sampling native assets through a cosmetic component keeps ACL decoding in
  -- the game. Zero play rate plus SetPosition(false) suppresses animation notifies.
  M.driver:SetPosition(state.time,false)
  local sockets=C.NativeBodyRead~=false and reader.read(M.driver)or nil
  local alpha=1-math.exp(-20*dt);local overrides={}
  local turn=Q.axis(0,0,1,state.yaw)
  for index,b in ipairs(motionRig.mapping)do
   local source
   if sockets then local x,y,z,w=string.unpack('<ffff',sockets,(index-1)*40+13);source={x,y,z,w}else source=Q.quat(M.driver:GetSocketTransform(driverNames[b.name],2).Rotation)end
   local q=Q.mul(source,b.offset)
   q=Q.mix(rig.target[b.target].cq,q,state.weight)
   if b.name=='root_x' or b.name:match('^thigh')or b.name:match('^leg')or b.name:match('^foot')or b.name:match('^toes')then q=Q.mul(turn,q)end
   q=Q.mix(M.overrides[b.target]or rig.target[b.target].cq,q,alpha);M.overrides[b.target]=q;overrides[b.target]=q
  end
  local sourcePelvis;if sockets then local x,y,z=string.unpack('<fff',sockets,(rootSlot-1)*40+1);sourcePelvis={X=x,Y=y,Z=z}else sourcePelvis=M.driver:GetSocketTransform(rootName,2).Translation end
  local pelvisIndex=names.root_x;local desired=Q.add(referencePositions[pelvisIndex],Q.rot(turn,Q.sub(Q.vec(sourcePelvis),motionRig.pelvisBind)))
  for i=1,3 do desired[i]=referencePositions[pelvisIndex][i]+(desired[i]-referencePositions[pelvisIndex][i])*motionRig.pelvisScale*state.weight end
  if not M.pelvis then M.pelvis=referencePositions[pelvisIndex]end
  local pelvis={};for i=1,3 do pelvis[i]=M.pelvis[i]+(desired[i]-M.pelvis[i])*alpha end;M.pelvis=pelvis
  local bones=snapshot.bones
  if bones then for i,b in ipairs(rig.mapping)do local k=(i-1)*4+1;overrides[b.target]=Q.mul({bones[k],bones[k+1],bones[k+2],bones[k+3]},armOffsets[i])end end
  M.pitch=Q.pitch(snapshot.view and snapshot.view.pitch or 0)
  -- Absolute component orientation prevents torso lean from exceeding the
  -- requested +/-45 degree head limit. Descendant eyes/hair follow the head.
  overrides[names.head_x]=Q.mul(Q.axis(1,0,0,M.pitch),rig.target[names.head_x].cq)
  local positions,rotations,byName
  if C.ReuseBodyPose~=false then positions,rotations,byName=builder.build(overrides,pelvis)
  else positions,rotations,byName={},{},{}
  for i,b in ipairs(rig.target)do
   local parent=b.parent;local q=overrides[i]or(parent>0 and Q.mul(rotations[parent],b.q)or b.q)
   local p=i==pelvisIndex and pelvis or(parent>0 and Q.add(positions[parent],Q.rot(rotations[parent],b.p))or b.p)
   positions[i]=p;rotations[i]=q;byName[b.name]=p
  end end
  if downId~=M.downId then
   if downId>0 then M.fallAt=now;M.riseAt=nil else M.riseAt=now end
   M.downId=downId
  end
  local fallen=downId>0 or (M.riseAt and now-M.riseAt<.48)
  if fallen then
   local elapsed=downId>0 and now-M.fallAt or .72*(1-(now-M.riseAt)/.48)
   prone(positions,rotations,elapsed)
   M.state=downId>0 and 'downed' or 'getting_up'
  end
  local parentChanged={};local pending={}
  for i,b in ipairs(rig.target)do
   parentChanged[i]=parentChanged[b.parent]or false
   if changed[i] or ((fallen or M.wasProne) and i>=pelvisIndex)then
    -- PoseableMesh stores local transforms. A changed ancestor invalidates an
    -- otherwise identical component-space child transform as well.
    if parentChanged[i]or committed.changed(i,positions[i],rotations[i])then
     pending[#pending+1]=i
     parentChanged[i]=true
    else M.boneSkips=M.boneSkips+1 end
   end
  end
  if #pending>0 then
   if not batch.apply(M.pose,pending,positions,rotations)then
    for _,i in ipairs(pending)do M.pose:SetBoneTransformByName(boneNames[i],Q.transform(positions[i],rotations[i]),1)end
   end
   for _,i in ipairs(pending)do committed.commit(i,positions[i],rotations[i])end
   M.boneWrites=M.boneWrites+#pending
  end
  M.wasProne=fallen
  local correction=0
  if state.grounded and not fallen then
   local minimum=math.huge
   for _,name in ipairs({'foot_l','foot_r','toes_01_l','toes_01_r'})do local i=names[name];minimum=math.min(minimum,positions[i][3]-referencePositions[i][3])end
   correction=Q.clamp(-minimum,-30,30)
  end
  M.groundOffset=M.groundOffset+(correction-M.groundOffset)*(1-math.exp(-35*dt))
  if not M.lastGroundOffset or math.abs(M.groundOffset-M.lastGroundOffset)>.0001 then
   M.pose:K2_SetRelativeTransform(Q.transform({0,0,M.groundOffset},Q.axis(0,0,1,-90)),false,{},true);M.lastGroundOffset=M.groundOffset
  end
  weapons.apply(actor,M.pose,byName,not fallen and snapshot.view or nil)
 end
 function M.status()return string.format('state=%s headPitch=%.3f nativeLocomotion=%s poseBatch=%s poseBatches=%d poseFallbacks=%d %s',M.state or 'none',M.pitch or 0,tostring(valid(M.driver)),tostring(batch.available),batch.calls,batch.fallbacks,weapons.status())..' poseInitError='..(batch.error and batch.error:gsub('[\r\n]',' '):sub(1,240)or'none')..' '..reader.status('bodyRead')..' poseReuse='..tostring(C.ReuseBodyPose~=false)end
 return M
end
