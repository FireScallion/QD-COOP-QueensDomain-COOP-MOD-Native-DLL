-- Budgets cosmetic work only; visible animation and gameplay keep their cadence.
return function(config,valid)
 local M={changes=0,pins=0,wakes=0,checks=0,sleeps=0,parked={}};local all,visible,montages
 local ok,err=pcall(function()
  local enum=StaticFindObject('/Script/Engine.EVisibilityBasedAnimTickOption');assert(valid(enum),'visibility enum absent')
  enum:ForEachName(function(name,value)
   local n=name:ToString();if n:match('::AlwaysTickPoseAndRefreshBones$')then all=value elseif n:match('::AlwaysTickPose$')then visible=value elseif n:match('::OnlyTickMontagesWhenNotRendered$')then montages=value end
  end)
  assert(all~=nil and visible~=nil and montages~=nil,'visibility modes unavailable')
 end)
 M.available=ok;M.error=not ok and tostring(err)or nil
 local function set(r,mode)
  if r.animRefreshMode~=mode then r.pose.VisibilityBasedAnimTickOption=mode;r.animRefreshMode=mode;M.changes=M.changes+1 end
 end
 local function choose(r,mode)
  M.parked[r]=nil
  if(r.animBudgetSleeping or r.animRefreshMode~=mode)and valid(r.pose)then
   if r.animBudgetSleeping then r.pose:SetComponentTickEnabled(true)end
   if M.available then set(r,mode)end
  end
  r.animBudgetSleeping=false;r.animSleepOwner=nil
 end
 local function wake(r)choose(r,all)end
 function M.setup(r)
  if r.animBudgetSleeping and r.animSleepOwner==r.pose and valid(r.pose)then r.pose:SetComponentTickEnabled(true)end
  M.parked[r]=nil;r.animBudgetSleeping=false;r.animSleepOwner=nil;r.animFar=false;r.animRefreshMode=all or 0;r.animMeshRadius=nil
  if r.animBoneOwner~=r.pose then r.animBoneOwner=r.pose;r.animBonePinned=false;r.animBoneConsumers={};r.animBoneCount=0 end
  if config.OffscreenEnemyAnimation~=false then
   local bounds=r.pose.SkeletalMesh:GetBounds();local o=bounds.Origin
   r.animMeshRadius=bounds.SphereRadius+math.sqrt(o.X*o.X+o.Y*o.Y+o.Z*o.Z)
  end
 end
 function M.pin(r,consumer)
  if not r or not r.native then return end
  assert(consumer,'bone attachment identity required');r.animBoneConsumers=r.animBoneConsumers or{}
  -- A changed owner, activity or attachment type wakes before effect activation.
  local requirement=(consumer.boneStaticBounded and 1 or 2)+(consumer.boneActive==false and 4 or 0)
  local old=r.animBoneConsumers[consumer];if old==requirement then return r end
  r.animBoneConsumers[consumer]=requirement
  if not old then r.animBoneCount=(r.animBoneCount or 0)+1;M.pins=M.pins+1 end
  r.animBonePinned=true;wake(r);return r
 end
 function M.release(r,consumer)
  if not r or not r.animBoneConsumers or not r.animBoneConsumers[consumer]then return end
  r.animBoneConsumers[consumer]=nil;r.animBoneCount=r.animBoneCount-1;r.animBonePinned=r.animBoneCount>0
 end
 local function behind(r)
  local v=M.view;if not v or not r.animX then return false end
  -- Only bounded static attachments relax pins, with a conservative envelope.
  return (r.animX-v.x)*v.fx+(r.animY-v.y)*v.fy+(r.animZ-v.z)*v.fz < -1500
 end
 function M.frame(player,replicas,now)
  M.now=now;if not M.available then return end
  if config.OffscreenEnemyAnimation==false or config.NativeFarAnimation==false then
   for r in pairs(M.parked)do if replicas[r.id]==r then wake(r)else M.parked[r]=nil end end;M.view=nil;return
  end
  if M.player~=player or not valid(M.camera)then
   M.player=player;M.camera=StaticFindObject('/Script/Engine.Default__GameplayStatics'):GetPlayerCameraManager(player,0)
  end
  if valid(M.camera)then
   local p=M.camera:GetCameraLocation();local q=M.camera:GetCameraRotation();local pitch,yaw=math.rad(q.Pitch),math.rad(q.Yaw);local cp=math.cos(pitch)
   local v=M.view or{};M.view=v;v.x=p.X;v.y=p.Y;v.z=p.Z;v.fx=cp*math.cos(yaw);v.fy=cp*math.sin(yaw);v.fz=math.sin(pitch)
  else M.view=nil end
  -- Wake on every local frame, before the far presentation schedule. A camera
  -- turn never waits for a packet or a distant actor's next presentation slot.
  for r in pairs(M.parked)do
   if replicas[r.id]~=r or r.dead or not r.native then M.parked[r]=nil
   elseif not behind(r)or now-(r.animBudgetAt or 0)>.25 then wake(r);M.wakes=M.wakes+1
   elseif r.animBudgetSleeping then
    local needed=not valid(r.pose)or r.pose:WasRecentlyRendered(.2)
    if not needed then for c in pairs(r.animBoneConsumers or{})do
     if not c.boneStaticBounded or not valid(c.c)or(c.boneActive~=false and c.c:WasRecentlyRendered(.2))then needed=true;break end
    end end
    if needed then wake(r);M.wakes=M.wakes+1 end
   end
  end
 end
 function M.apply(r,f,distance,sample)
  if not M.available then return end
  local bits=f[34]or 0
  local combat=(f.action or 0)~=0 or(f[30]or 0)>0 or bits%4>=2 or math.floor(bits/8)%2==1
  if distance<3000^2 then r.animFar=false elseif distance>3500^2 then r.animFar=true end
  local eligible=config.NativeFarAnimation~=false and r.animFar and not combat and not r.dead
  if config.OffscreenEnemyAnimation==false then set(r,eligible and not r.animBonePinned and visible or all);return end
  -- Use the rendered mesh origin, not the gameplay actor's potentially offset
  -- origin. Bounds and socket offsets must also inherit the parent scale.
  local mx,my,mz=sample.mesh[1],sample.mesh[2],sample.mesh[3]
  if not r.animX or math.abs(r.animX-mx)+math.abs(r.animY-my)+math.abs(r.animZ-mz)>.01 then r.animStillAfter=(M.now or 0)+.5 end
  r.animBudgetAt=M.now;r.animX=mx;r.animY=my;r.animZ=mz
  if not eligible then wake(r);return end
  local fallback=r.animBonePinned and all or montages
  local scale=math.max(math.abs(sample.mesh[8]),math.abs(sample.mesh[9]),math.abs(sample.mesh[10]))
  if not r.animMeshRadius or r.animMeshRadius*scale>500 or(r.animBonePinned and scale>1)or not behind(r)then choose(r,fallback);return end
  for consumer in pairs(r.animBoneConsumers or{})do
   -- Skeletal meshes, particles, beams, trails and unknown consumers stay full.
   if not consumer.boneStaticBounded or not valid(consumer.c)then wake(r);return end
   if consumer.boneActive~=false then M.checks=M.checks+1;if consumer.c:WasRecentlyRendered(.5)then wake(r);return end end
  end
  local sleep=bits%32==0 and(M.now or 0)>=(r.animStillAfter or math.huge)and not r.pose:WasRecentlyRendered(.5)
  if sleep then
   if not r.animBudgetSleeping then r.pose:SetComponentTickEnabled(false);r.animBudgetSleeping=true;r.animSleepOwner=r.pose;M.sleeps=M.sleeps+1 end
  elseif r.animBudgetSleeping then choose(r,montages)end
  M.parked[r]=true;set(r,montages)
 end
 function M.reset()M.parked={};M.camera=nil;M.player=nil;M.view=nil end
 function M.status(replicas)
  local far,pinned,budgeted,parked,sleeping=0,0,0,0,0
  for _,r in pairs(replicas)do if not r.dead and r.native then
   if r.animRefreshMode==visible or r.animRefreshMode==montages then far=far+1 end
   if r.animRefreshMode==montages then budgeted=budgeted+1 end
   if r.animBonePinned then pinned=pinned+1 end
   if M.parked[r]then parked=parked+1 end
   if r.animBudgetSleeping then sleeping=sleeping+1 end
  end end
  return string.format('farAnimAvailable=%s farAnimEnabled=%s farAnimCulled=%d farAnimPinned=%d farAnimChanges=%d farAnimPinEvents=%d farAnimError=%s animBudgetPolicy=%s animBudgetMontages=%d animBudgetParked=%d animBudgetSleeping=%d animBudgetSleepEvents=%d animBudgetWakeups=%d animBudgetConsumerChecks=%d',tostring(M.available),tostring(config.NativeFarAnimation~=false),far,pinned,M.changes,M.pins,(M.error or'none'):gsub('[\r\n ]','_'):sub(1,100),config.OffscreenEnemyAnimation==false and'preview6'or'visibility',budgeted,parked,sleeping,M.sleeps,M.wakes,M.checks)
 end
 return M
end
