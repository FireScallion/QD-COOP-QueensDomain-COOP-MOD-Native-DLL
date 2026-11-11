-- One native skeletal component, engine animation graph, no full-body bone copy.
-- This pilot deliberately retains wire B poses for old peers and fallback actions.
return function(dir,valid,full,C,weapons)
 local skin=dofile(dir..'skin_visual.lua')(dir,valid)
 local Q=dofile(dir..'pose_math.lua');local rig=dofile(dir..'spear_rig.lua')
 local Locomotion=dofile(dir..'locomotion.lua')(Q)
 local M={ready=false,failed=false,starts=0,corrections=0,frames=0,boneWrites=0}
 local cat=dofile(dir..'body_catalog.lua');M.catalog=cat
 local clips=cat.clips;local cursor=1;local retained={};local fallback=dofile(dir..'motion_rig.lua');local warm={fallback.mesh,fallback.clips.walk,fallback.clips.run,fallback.clips.jump};local warmCursor=1
 local function load(ks,p)return ks:LoadAsset_Blocking(ks:Conv_SoftObjPathToSoftObjRef(ks:MakeSoftObjectPath(p)))end
 local ref,head={},nil;local pos,rot={},{}
 for i,b in ipairs(rig.target)do rot[i]=b.parent>0 and Q.mul(rot[b.parent],b.q)or b.q;pos[i]=b.parent>0 and Q.add(pos[b.parent],Q.rot(rot[b.parent],b.p))or b.p;ref[b.name]=pos[i];if b.name=='head_x'then head=b.cq end end
 local footNames={'foot_l','foot_r','toes_01_l','toes_01_r'};local names={};for _,n in ipairs(footNames)do names[n]=FName(n)end;names.hand_l=FName('hand_l');names.hand_r=FName('hand_r')
 function M.preload(gi)
  if C.NativeSpear==false or M.ready or M.failed or not valid(gi)then return end
  local ok,err=pcall(function()
   local ks=C.ObjectRefs.find('/Script/Engine.Default__KismetSystemLibrary')
   if not M.class then
    local p='/Game/QDCoopBody/ABP_QDBody.ABP_QDBody_C'
    M.class=ks:LoadClassAsset_Blocking(ks:Conv_SoftClassPathToSoftClassRef(ks:MakeSoftClassPath(p)));assert(valid(M.class),'native body class missing')
    retained[#retained+1]=M.class
   end
   -- Bounded warm-up before connection: never load the entire catalog in a frame.
   for i=cursor,math.min(cursor+7,#clips)do
    local c=clips[i];local p='/Game/QDCoopBody/Animations/'..c.name..'.'..c.name
    c.sequence=load(ks,p);assert(valid(c.sequence),'native sequence missing '..c.name);retained[#retained+1]=c.sequence
    if c.montage then c.asset=load(ks,'/Game/QDCoopBody/Montages/'..c.name..'.'..c.name);assert(valid(c.asset));retained[#retained+1]=c.asset end
    cursor=i+1
   end
   if cursor>#clips and warmCursor<=#warm then local a=LoadAsset(warm[warmCursor]);assert(valid(a),'fallback asset missing');retained[#retained+1]=a;warmCursor=warmCursor+1 end
   local refs={};gi.ReferencedObjects:ForEach(function(_,v)refs[#refs+1]=v:get()end)
   for _,r in ipairs(retained)do refs[#refs+1]=r end;retained={};gi.ReferencedObjects=refs
   M.ready=cursor>#clips and warmCursor>#warm
  end)
  if not ok then M.failed=true;M.error=tostring(err);print('[QDCoop] Native body unavailable; retaining legacy: '..M.error..'\n')end
 end
 function M.eligibleView(view)
  local b=view and view.body
  return C.NativeSpear~=false and M.ready and not M.failed and b and b.hash==cat.hash and b.ready and b.root>0 and b.left>0
 end
 function M.eligible(snapshot)return M.eligibleView(snapshot.view)end
 function M.releasePresentation()
  skin.reset();M.skin=nil
  if valid(M.pose)then M.pose:SetVisibility(false,false);M.pose:SetComponentTickEnabled(false);M.pose:K2_DestroyComponent(M.actor)end
  if M.actor then weapons.clearPresentation()end
  M.root=nil;M.left=nil;M.pose=nil;M.actor=nil;M.instance=nil;M.current=nil;M.serial=nil;M.lastTime=nil;M.lastActionTime=nil;M.ground=0;M.lastGround=nil;M.motion=Locomotion.new();M.turn=0;M.run=0;M.lanes={};M.wasDown=false;M.lifeStart=nil
 end
 M.releasePresentation()
 function M.apply(actor,base,snapshot,now,downId)
  local selected=C.SkinCatalog.get(snapshot.view and snapshot.view.skin).id
  if not valid(M.actor)or full(M.actor)~=full(actor)or M.skin~=selected then
   M.releasePresentation();M.actor=actor
   local pose=actor:AddComponentByClass(C.ObjectRefs.find('/Script/Engine.SkeletalMeshComponent'),false,Q.transform({0,0,0},Q.axis(0,0,1,-90)),false)
   M.pose=pose;skin.apply(pose,selected);M.skin=selected;pose:SetCollisionEnabled(0);pose:SetGenerateOverlapEvents(false)
   pose.VisibilityBasedAnimTickOption=0;pose.bEnableUpdateRateOptimizations=false;pose:SetAnimClass(M.class);pose:SetComponentTickEnabled(true)
   M.instance=pose:GetAnimInstance();assert(valid(M.instance),'native animation instance unavailable')
   base:SetVisibility(false,false);base:SetComponentTickEnabled(false)
  end
  local clock=snapshot.sampleTime or now;local dt=M.lastTime and Q.clamp(clock-M.lastTime,0,.1)or .016;M.lastTime=clock
  local v=snapshot.view;local a=v.action;local m=snapshot.motion;local state=M.motion:update(m,v,dt,snapshot.yaw);M.state=state.state
  local speed=math.sqrt(m.vx*m.vx+m.vy*m.vy);local rad=snapshot.yaw*math.pi/180;local forward=m.vx*math.cos(rad)+m.vy*math.sin(rad);local right=-m.vx*math.sin(rad)+m.vy*math.cos(rad)
  local sign=forward< -math.abs(right)*.5 and -1 or 1;local baseSpeed=math.max(v.walkSpeed or 400,1)
  local inst=M.instance
  inst.WalkRate=Q.clamp(speed/baseSpeed,.25,1.8)*sign;inst.RunRate=Q.clamp(speed/(baseSpeed*1.6),.25,1.8)*sign
  M.run=M.run+(((v.running==1 or m.dash==1)and 1 or 0)-M.run)*(1-math.exp(-20*dt));inst.RunBlend=M.run
  inst.MoveWeight=M.motion.weight;inst.JumpBlend=state.clip=='jump'and state.weight or 0;inst.JumpTime=state.clip=='jump'and state.time or 0
  M.turn=M.turn+(state.yaw-M.turn)*(1-math.exp(-20*dt));inst.LowerTurn={Pitch=0,Yaw=M.turn,Roll=0}
  M.pitch=Q.pitch(v.pitch or 0);local h=Q.mul(Q.axis(1,0,0,M.pitch),head)
  inst.HeadRotation=C.ObjectRefs.find('/Script/Engine.Default__KismetMathLibrary'):Quat_Rotator({X=h[1],Y=h[2],Z=h[3],W=h[4]})
  local body=v.body
  if M.root~=body.root then inst.RootSequence=clips[body.root].sequence;M.root=body.root end
  if M.left~=body.left then inst.LeftSequence=clips[body.left].sequence;M.left=body.left end
  inst.IdleWeight=.35;inst.LeftIdleWeight=.35
  local stride=Q.clamp(speed/baseSpeed,0,1)*.45
  inst.ArmWalkWeight=stride;inst.LeftWalkWeight=stride
  local down=(downId or 0)>0
  if down~=M.wasDown then M.lifeStart=now;M.wasDown=down end
  if down then
   inst.LifeSequence=clips[184].sequence;inst.LifeTime=math.min(now-(M.lifeStart or now),.72);inst.LifeWeight=Q.clamp((now-(M.lifeStart or now))/.15,0,1)
  elseif M.lifeStart and now-M.lifeStart<.48 then
   inst.LifeSequence=clips[185].sequence;inst.LifeTime=now-M.lifeStart;inst.LifeWeight=1-Q.clamp((now-M.lifeStart-.33)/.15,0,1)
  else inst.LifeWeight=0 end
  for group=1,7 do
   local lane=body.lanes[group];local current=M.lanes[group]
   if lane then
    local clip=clips[lane.id];local ratio=clip.length/clip.source
    local elapsed=Q.clamp(clock-(v.bodyTime or clock),0,.25)
    local expected=lane.position+(lane.playing==1 and elapsed*lane.rate or 0)
    local section=clip.sections[lane.section]
    if section then
     local ending=clip.sections[lane.section+1]and clip.sections[lane.section+1].time or clip.source
     if section.next==section.name and ending>section.time and expected>=ending then expected=section.time+(expected-section.time)%(ending-section.time)end
    end
    expected=Q.clamp(expected,0,clip.source)*ratio
    if not current or current.id~=lane.id or current.serial~=lane.serial then
     if current then inst:Montage_Stop(math.min(clips[current.id].blendOut,.15),clips[current.id].asset)end
     -- Original source groups are managed explicitly: the game's skeleton is untouched.
     inst:Montage_Play(clip.asset,lane.rate*ratio,0,expected,false)
     current={id=lane.id,serial=lane.serial};M.lanes[group]=current;M.starts=M.starts+1
    end
    if current.time~=v.bodyTime then
     local actual=inst:Montage_GetPosition(clip.asset);local error=expected-actual
     if section and section.next==section.name then
      local ending=clip.sections[lane.section+1]and clip.sections[lane.section+1].time or clip.source
      local span=(ending-section.time)*ratio;if span>0 then error=(error+span*.5)%span-span*.5 end
     end
     if inst:Montage_IsPlaying(clip.asset)then
      if math.abs(error)>.65 then inst:Montage_SetPosition(clip.asset,expected);M.corrections=M.corrections+1
      else inst:Montage_SetPlayRate(clip.asset,lane.playing==1 and Q.clamp(lane.rate*ratio+error*8,-4,4)or 0)end
     end
     current.time=v.bodyTime
    end
   elseif current then inst:Montage_Stop(math.min(clips[current.id].blendOut,.25),clips[current.id].asset);M.lanes[group]=nil end
  end
  local positions={};for _,n in ipairs({'hand_l','hand_r'})do positions[n]=Q.vec(M.pose:GetSocketTransform(names[n],2).Translation)end
  local correction=0
  if state.grounded then local minimum=math.huge;for _,n in ipairs(footNames)do local z=M.pose:GetSocketTransform(names[n],2).Translation.Z;minimum=math.min(minimum,z-ref[n][3])end;correction=Q.clamp(-minimum,-30,30)end
  M.ground=M.ground+(correction-M.ground)*(1-math.exp(-35*dt))
  if not M.lastGround or math.abs(M.ground-M.lastGround)>.0001 then M.pose:K2_SetRelativeTransform(Q.transform({0,0,M.ground},Q.axis(0,0,1,-90)),false,{},true);M.lastGround=M.ground end
  weapons.apply(actor,M.pose,positions,v);M.frames=M.frames+1
 end
 function M.status()return string.format('nativeBodyReady=%s nativeFrames=%d nativeStarts=%d nativeResyncs=%d nativeError=%s',tostring(M.ready),M.frames,M.starts,M.corrections,M.error or 'none')end
 return M
end
