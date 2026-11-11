-- QD COOP 1.18.0-preview3: hash-only lookup, no full-table miss fallback.
-- Host-owned world overlay with separate guest character persistence.
local source=debug.getinfo(1,'S').source:gsub('^@','')
local dir=assert(source:match('^(.*[/\\])'))
local _ENV=dofile(dir..'fast_lookup.lua')(dir,_G)
local C=_G.QDCoopNativeConfig or dofile(dir..'config.lua')
local P=dofile(dir..'protocol.lua')
C.SkinCatalog=dofile(dir..'skin_catalog.lua')
C.SkinSelection=dofile(dir..'skin_selection.lua')(C.SkinCatalog,C.Ipc..'/../player-skin.txt')
C.Skin=C.SkinSelection.selected
local L=dofile(dir..'localization.lua')()
local S=dofile(dir..'smoothing.lua')
local smooth=S.new()
local function log(s) print('[QDCoop] '..tostring(s)..'\n') end
C.GCPolicy=dofile(dir..'gc_policy.lua').apply(C.GenerationalGC,log)
local function safe(fn) local ok,r=pcall(fn);if ok then return r end end
local function objectValid(o)return o:IsValid()end
local function objectName(o)return o:GetFullName()end
local function valid(o)if o==nil then return false end;local ok,v=pcall(objectValid,o);return ok and v==true end
local function full(o)if not valid(o)then return ''end;local ok,v=pcall(objectName,o);return ok and v or ''end
local action=dofile(dir..'action_state.lua')(P,valid,full)
local bodySource=dofile(dir..'body_source.lua')(dir,valid,full)
local animation=dofile(dir..'body_router.lua')(dir,valid,full,C)
animation.source=bodySource

local ipc=dofile(dir..'async_ipc.lua')(dir,C.Ipc,C.IpcChannel)
local read,write=ipc.read,ipc.write
local frameCost=dofile(dir..'frame_cost.lua')(write)
C.Perf=frameCost -- private diagnostics object; never serialized into the protocol
local lookupCost=dofile(dir..'lookup_profile.lua')(frameCost.clock,write)
C.ObjectRefs=dofile(dir..'object_refs.lua')(dir,C,valid)
local function findObject(path)return C.ObjectRefs.find(path)end
local hookTrace=dofile(dir..'hook_trace.lua')(frameCost.clock,read,write,frameCost);hookTrace.install()
local guard=dofile(dir..'runtime_guard.lua')(log,write,os.clock)
local effects=dofile(dir..'effects.lua')(dir,C,valid,full,read,write,log)
local audio=dofile(dir..'audio.lua')(dir,C,valid,full,read,write,log,effects.isLocalSource)
local enemies=dofile(dir..'enemies.lua')(dir,C,valid,full,read,write,log,audio)
effects.bindEnemies(enemies)
effects.bindBody(animation)
audio.bindEnemies(enemies)
local checkpoint=dofile(dir..'checkpoint.lua')(C,P,write,log,valid)
local worldSync=dofile(dir..'world.lua')(dir,C,valid,full,read,write,log,checkpoint)
local scenes=dofile(dir..'cutscene_sync.lua')(dir,C,valid,full,read,write,log,worldSync,enemies)
local rescue=dofile(dir..'rescue.lua')(dir,C,P,valid,full,write,log,checkpoint,worldSync)
local items=dofile(dir..'items.lua')(dir,C,P,valid,full,write,log,checkpoint,L)
checkpoint.setGuard(function()return not items.busy()and not rescue.busy()and not scenes.busy()end)
local function norm(s)return (s:gsub('\\','/'):gsub('/+$',''):lower())end
C.SteamJoinAllowed=function()return not items.busy()and not rescue.busy()and not scenes.busy()end
local menu=C.MenuMode and dofile(dir..'menu.lua')(C,P,read,write,log,worldSync,L) or nil
local function transform(pos) return {Rotation={X=0,Y=0,Z=0,W=1},Translation=pos,Scale3D={X=1,Y=1,Z=1}}end
local function dist(a,b)return math.sqrt((a.X-b.X)^2+(a.Y-b.Y)^2+(a.Z-b.Z)^2)end
local loading=false
local epoch,ticks,warmup=0,0,0
local player,remote,target
local targetId,targetHP,targetPos='',C.TargetHP,nil
local targetDead=false
local remotePose,peerWorld,connected=nil,'',false
smooth.nativeBody=function(view,now)return animation.usesNative(view,now,remotePose and remotePose.down)end
local version,attackSerial,lastAttack,eventOffset,eventSerial=0,0,0,0,0
local seenEvents={}
local hookIds={}
local hooksReady,loadedSlot,verified,disabled=false,false,false,false
local gs,ks
local presentRemote,serviceMotion
local nextCapture,nextReceive=0,0
local lastInbound=""
local sampleSerial,jumpSerial,landSerial=0,0,0
local renderFrames,renderFrom,remoteMotion=0,nil,'idle'
local lastRemoteAt=nil
local traceRows={}
local bridgeState,bridgeStamp='',0
local function peerConnected()
 local raw=read('status.txt');local f=P.split(raw:gsub('[\r\n]',''))
 local stamp=tonumber(f[10])
 if (f[1]=='connected' or f[1]=='waiting') and #f==10 and stamp then bridgeState=f[1];bridgeStamp=stamp
 elseif raw:match('^stopped|') then bridgeState='stopped' end
 return bridgeState=='connected' and math.abs(os.time()-bridgeStamp)<=8
end
local bodyAsset='/Game/Player/Assets/FPModel/Player2026/Player_FullChar_2026.Player_FullChar_2026'
local bodyAnim='/Game/Player/Assets/FPModel/Player2026/Player_FullChar_2026_coolPose.Player_FullChar_2026_coolPose'
local enemyAsset='/Game/Enemies/Slime_00/Slimer_base_BP.Slimer_base_BP_C'
local function world(p) return full(p):match('(/Game/[^:]+):') end
local function same(a,b)return valid(a) and valid(b) and full(a)==full(b)end
local function dispose(o) if valid(o) then safe(function()o:K2_DestroyActor()end)end end
local function setPos(o,pos,yaw)
 if valid(o) then o:K2_SetActorLocationAndRotation(pos,{Pitch=0,Yaw=yaw,Roll=0},false,{},true) end
end
local function spawn(class,pos)
 local t=transform(pos);local a=gs:BeginDeferredActorSpawnFromClass(player,class,t,1,nil)
 assert(valid(a),'Spawn failed');gs:FinishSpawningActor(a,t);return a
end
local function freezeEnemy(e)
 e:SetActorTickEnabled(false)
 local controller=e:GetController()
 if valid(controller) then controller:StopMovement();controller:UnPossess();controller:SetActorTickEnabled(false)end
 local move=e.CharacterMovement;if valid(move) then move:DisableMovement()end
end
local function createTarget(pos,id)
 log('TARGET_SPAWN begin')
 local e=spawn(LoadAsset(enemyAsset),pos)
 log('TARGET_SPAWN actor ready')
 e.HP=C.TargetHP;freezeEnemy(e)
 log('TARGET_SPAWN frozen')
 target=e;targetId=id;targetHP=C.TargetHP;targetDead=false;targetPos=pos
 write('target-binding.txt',world(player)..'|'..id..'|'..full(e))
 log('TARGET '..id..' '..full(e))
end
local function createRemote()
 local pos=remotePose or {X=0,Y=0,Z=0}
 remote=spawn(findObject('/Script/Engine.SkeletalMeshActor'),{X=pos.X,Y=pos.Y,Z=pos.Z-85})
 remote:SetActorEnableCollision(false)
 local mesh=remote:GetComponentByClass(findObject('/Script/Engine.SkeletalMeshComponent'))
 mesh:SetSkeletalMesh(LoadAsset(bodyAsset),true);mesh:PlayAnimation(LoadAsset(bodyAnim),true)
 log('REMOTE '..full(remote))
end
local function registerHooks()
 effects.install()
 audio.install()
 if hooksReady then return end
 scenes.install(hookIds);enemies.install(hookIds);bodySource.install(hookIds)
 local pauseHooks=dofile(dir..'no_pause.lua')(C,log,read)
 for _,h in ipairs(pauseHooks)do hookIds[#hookIds+1]=h end
 local playerPath='/Game/Player/FS_PlayerChar.FS_PlayerChar_C:'
 local function playerHook(name,fn)
  local path=playerPath..name;assert(valid(findObject(path)),'Missing native player event: '..path)
  local a,b=RegisterHook(path,function(ctx,...)
   if not disabled and not loading and same(ctx:get(),player) then
    guard.call('player:'..name,fn,...)
   end
  end);hookIds[#hookIds+1]={path,a,b}
 end
 playerHook('ReceiveTick',function()rescue.localFrame(gs:GetRealTimeSeconds(player))end)
 playerHook('OnJumped',function()jumpSerial=jumpSerial+1 end)
 playerHook('OnLanded',function()landSerial=landSerial+1 end)
 local placedClass=ks:LoadClassAsset_Blocking(ks:Conv_SoftClassPathToSoftClassRef(ks:MakeSoftClassPath('/Game/QDCoop/QDCoopPlacedItem.QDCoopPlacedItem_C')))
 local gi=FindFirstOf('FS_GameInstance_C');local refs={};gi.ReferencedObjects:ForEach(function(_,v)refs[#refs+1]=v:get()end);refs[#refs+1]=placedClass;gi.ReferencedObjects=refs
 items.install(function(path,fn)
  local a,b=RegisterHook(path,function(ctx,...)
   if disabled or loading then return end
   local ok,err=pcall(fn,ctx,...);if not ok then log('ITEMS hook error '..tostring(err))end
  end);hookIds[#hookIds+1]={path,a,b}
 end)
 rescue.install(function(path,fn)
  assert(valid(findObject(path)),'Missing rescue function: '..path)
  local a,b=RegisterHook(path,function(ctx,...)
   if disabled or loading then return end
   local ok,result=pcall(fn,ctx,...)
   if not ok then log('RESCUE hook error '..tostring(result))else return result end
  end)
  hookIds[#hookIds+1]={path,a,b}
 end)
 local path='/Game/Enemies/EnemyBase.EnemyBase_C:DamageHP'
 assert(valid(findObject(path)),'Enemy damage function not loaded')
 local a,b=RegisterHook(path,function(ctx,amount)
  -- Hook callbacks may run in a separate Lua execution context. Resolve the
  -- current binding from data, never capture a mutable actor handle from tick.
  local e=ctx:get();if enemies.damage(e,amount:get())then return end;local binding=P.split(read('target-binding.txt'))
  if not valid(e) or full(e)~=binding[3] then return end
  if C.Role=='client' then
   local frame=P.frame(read('in.state'))
   local hp=frame and frame.E and frame.E.id==binding[2] and frame.E.hp or C.TargetHP
   e.HP=math.max(1,hp)
  else
   log('HOST_DAMAGE '..tostring(amount:get())..' HP='..e.HP)
  end
 end)
 hookIds[#hookIds+1]={path,a,b}
 path='/Game/Enemies/EnemyBase.EnemyBase_C:TryApplyDamage'
 a,b=RegisterHook(path,function(ctx,hit,damage,applier,applied,hitPos,hitValid)
  if not peerConnected() then return end
  local binding=P.split(read('target-binding.txt'))
  local e=ctx:get();if not valid(e)then return end
  local pawn=findObject('/Script/Engine.Default__GameplayStatics'):GetPlayerPawn(e,0)
  eventSerial=eventSerial+1;if enemies.hit(e,safe(function()return damage:get().DamageAmount end),eventSerial,applier:get(),damage:get())then return end
  if not same(applier:get(),pawn) then return end
  if C.Role~='client' or full(e)~=binding[3]then audio.commitHit(e,0,false);return end
  local frame=P.frame(read('in.state'))
  if not frame or not frame.E or frame.E.id~=binding[2] or frame.E.hp<=0 then return end
  local amount=safe(function()return damage:get().DamageAmount end)
  if type(amount)~='number' or amount<=0 or amount>10000 then return end
  local p=valid(pawn) and pawn:K2_GetActorLocation() or nil;if not p then return end
  if dist(p,{X=frame.E.x,Y=frame.E.y,Z=frame.E.z})>800 then return end
  eventSerial=eventSerial+1
  audio.commitHit(e,eventSerial,true)
  write('out.events',string.format('HIT|%s|%s|%.6f|%d\n',binding[1],binding[2],amount,eventSerial),true)
  log('GUEST_HIT '..amount..' request='..eventSerial)
 end)
 hookIds[#hookIds+1]={path,a,b};hooksReady=true
end
local eventWorld
local journal=dofile(dir..'event_mailbox.lua')(C.Ipc..'/in.events',function(line)
 local currentWorld=eventWorld
  local f=P.split(line);local key=f[1]
  if connected then
   local kind=f[2]
   if kind=='SC'then return scenes.event(f,remotePose)
   elseif kind=='VFX'then return effects.event(f,currentWorld,smooth.session)
   elseif kind=='SND'then return audio.event(f,currentWorld,smooth.session)
   elseif kind=='BI'or kind=='ER'or kind=='NPC'or kind=='RST'or kind=='THT'then return enemies.event(f,currentWorld,smooth.session,remotePose)
   elseif kind=='WC'or kind=='WB'or kind=='DC'then return worldSync.event(f,currentWorld,remotePose)
   elseif kind=='CP'then return checkpoint.event(f)
   elseif kind=='RV'then return rescue.event(f)
   elseif kind=='IT'then return items.event(f)end
  end
  if C.Role=='host' and connected and #f==6 and f[2]=='HIT' and f[3]==currentWorld and f[4]==targetId and not seenEvents[key] and not targetDead and valid(target) and remotePose and peerWorld==currentWorld then
   seenEvents[key]=true
   local amount=P.number(f[5],0.001,10000)
   if amount and dist(remotePose,target:K2_GetActorLocation())<=800 then
    local applied={}
    -- Basic damage uses the game's real resistance, HP, hit response and death path.
    -- Applier bookkeeping is the host player in this limited stationary-target prototype.
    audio.beginReplay(tonumber(f[6])or 0)
    local ok,err=pcall(function()target:TryApplyDamage({}, {DamageAmount=amount},player,applied,target:K2_GetActorLocation(),true)end)
    audio.endReplay();if not ok then error(err)end
    log('APPLIED_GUEST_HIT '..key..' amount='..amount..' HP='..target.HP)
   else log('REJECTED_HIT distance/value')end
  end
end,os.clock,guard.report,ipc.events,write,dofile(dir..'event_counts.lua'))
local function processEvents(currentWorld)eventWorld=currentWorld;journal.poll()end
local actionDiagnostic
local function updateReplica(frame,currentWorld)
 -- A brief incomplete/missing mailbox read is not a peer disconnect.
 if not frame or not frame.T or not frame.M then return end
 if frame.P.world~=currentWorld then
  dispose(remote);remote=nil;remotePose=nil;peerWorld='';smooth:reset();effects.resetRemote();audio.resetRemote();return
 end
 local p=frame.P;peerWorld=p.world

 local now=gs:GetRealTimeSeconds(player)
 if frame.view then frame.view.action=frame.A;frame.view.actionTime=frame.T.time;frame.view.body=frame.R;frame.view.bodyTime=frame.T.time end
 if not smooth:push(p,frame.T,frame.M,now,frame.B,frame.view)then return end
 animation.observe(frame,now)
 remotePose={X=p.x,Y=p.y,Z=p.z,yaw=p.yaw,hp=p.hp,at=now,down=frame.downed}
 lastRemoteAt=now
 if frame.A then
  local text=string.format('event=%d weapon=%s montage=%s rate=%.3f position=%.3f section=%s\n',frame.A.serial,frame.A.weapon,frame.A.montage,frame.A.rate,frame.A.position,frame.A.section)
  if text~=actionDiagnostic then write('remote-action.txt',text);actionDiagnostic=text end
 end
 if not valid(remote) then createRemote()end
 if C.Role=='client' and frame.E and frame.E.world==currentWorld then
  local e=frame.E
  if e.id~=targetId then dispose(target);createTarget({X=e.x,Y=e.y,Z=e.z},e.id)end
  targetPos={X=e.x,Y=e.y,Z=e.z};targetHP=math.max(0,e.hp);version=e.action
  if valid(target)then
   if targetHP<=0 then
    if not targetDead then targetDead=true;target.HP=0;audio.beginReplicaDeath(target);local ok,err=pcall(function()target:Kill(true,true)end);audio.endReplicaDeath();if not ok then error(err)end;log('REPLICA_DEATH '..targetId)end
   else
    setPos(target,targetPos,e.yaw);target.HP=targetHP
   end
  end
 end
end
local function captureMotion(w)
 local pos=player:K2_GetActorLocation();local yaw=player:K2_GetActorRotation().Yaw
 local lines={'QDCP1',P.row('P',w,C.Role,pos,yaw,tonumber(player.Health) or 0,attackSerial,player.Dashing and 1 or 0)}
 lines[#lines+1]='Y|1|'..C.Skin
 if rescue.id()>0 then lines[#lines+1]='G|'..rescue.id()end
 if C.Role=='host'and C.MenuMode then lines[#lines+1]='S|'..tostring(C.SelectedSlot or 0)end
 local movement=player.CharacterMovement;if not valid(movement)then return end
 local velocity=player:GetVelocity();local now=gs:GetRealTimeSeconds(player)
 sampleSerial=sampleSerial+1
 lines[#lines+1]=string.format('T|%s_%d|%d|%.6f',C.RunId,epoch,sampleSerial,now)
 lines[#lines+1]=string.format('M|%.4f|%.4f|%.4f|%d|%d|%d|%d|%d',velocity.X,velocity.Y,velocity.Z,movement.MovementMode,movement:IsFalling() and 1 or 0,player.Dashing and 1 or 0,jumpSerial,landSerial)
 local actionRow,actionData=action.capture(player);lines[#lines+1]=actionRow
 local bodyState,bodyRows=bodySource.capture(player);actionData.body=bodyState;if bodyRows then lines[#lines+1]=bodyRows end
 local capability,lite=animation.captureMode(C.RunId..'_'..epoch,actionData,rescue.id(),now);lines[#lines+1]=capability
 for _,line in ipairs(animation.capture(player,lite))do lines[#lines+1]=line end
 if C.Role=='host' and targetId~='' and targetPos then
  if C.MenuMode then lines[#lines+1]='S|'..tostring(C.SelectedSlot or 0)end
  local ty=valid(target) and target:K2_GetActorRotation().Yaw or 0
  lines[#lines+1]=P.row('E',w,targetId,targetPos,ty,targetHP,version,targetDead and 1 or 0)
 end
 lines[#lines+1]='END';write('out.state',table.concat(lines,'\n')..'\n')
end
captureMotion=frameCost.measure(captureMotion,'body.capture')
serviceMotion=function()
 if loading or disabled or not valid(player)then return end
 local now=gs:GetRealTimeSeconds(player);local w=world(player);if not w then return end
 guard.call('world-background',worldSync.background)
 guard.call('enemy-update',enemies.update,player,w,C.RunId..'_'..epoch,now,connected,smooth.session,remotePose)
 guard.call('fx-update',effects.update,player,w,C.RunId..'_'..epoch,now,connected)
 guard.call('audio-update',audio.update,player,w,C.RunId..'_'..epoch,now,connected)
 if connected and now>=nextReceive then
  nextReceive=nextReceive+1/60;if nextReceive<=now then nextReceive=now+1/60 end;local raw=read('in.state')
  if raw~='' and raw~=lastInbound then local frame=P.frame(raw);if frame then updateReplica(frame,w);lastInbound=raw end end
 end
 if connected and now>=nextCapture then
  nextCapture=nextCapture+1/30;if nextCapture<=now then nextCapture=now+1/30 end;guard.call('body-capture',captureMotion,w)
 end
end
presentRemote=function()
 if not connected or not valid(player) then return end
 local now=gs:GetRealTimeSeconds(player)
 guard.call('enemy-present',enemies.apply,now)
 if not valid(remote) then return end
 -- Keep the last accepted sequence on a presentation timeout. Otherwise the
 -- same stale mailbox frame can recreate and destroy a proxy every few ticks.
 if lastRemoteAt and now-lastRemoteAt>3 then dispose(remote);remote=nil;animation.reset();effects.resetRemote();audio.resetRemote();lastRemoteAt=nil;return end
 local pose=smooth:sample(now);if not pose then return end
 setPos(remote,{X=pose.X,Y=pose.Y,Z=pose.Z-85},pose.yaw)
 guard.call('body-present',animation.apply,remote,remote:GetComponentByClass(findObject('/Script/Engine.SkeletalMeshComponent')),pose,now,remotePose and remotePose.down)
 guard.call('fx-present',effects.apply,pose.sampleTime,animation.pose,enemies.sampleTime)
 guard.call('audio-present',audio.apply,pose.sampleTime,pose)
 if C.MotionTrace then
  local b=pose.bones or {};local f=smooth.frames[#smooth.frames]
  traceRows[#traceRows+1]=string.format('%.6f,%s,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f',now,pose.kind,smooth.lastSeq or 0,f and f.time or 0,pose.X,pose.Y,pose.Z,b[1]or 0,b[2]or 0)
  if #traceRows>=30 then write('motion-trace.csv',table.concat(traceRows,'\n')..'\n',true);traceRows={}end
 end
 remoteMotion=S.state(pose.motion)
 renderFrom=renderFrom or now;renderFrames=renderFrames+1
end
local function tick(uiOnly)
 if uiOnly and valid(player) then return end
 if not uiOnly then ticks=ticks+1 end
 if disabled or loading or ticks<warmup then return end
 gs=findObject('/Script/Engine.Default__GameplayStatics');ks=findObject('/Script/Engine.Default__KismetSystemLibrary')
 if not valid(gs) or not valid(ks) then return end
 if not verified then
  local saved=ks:GetProjectSavedDirectory():ToString()
  assert(norm(saved)==norm(C.ExpectedSaveDirectory),'Unexpected save directory: '..saved)
  write(C.NativeStartup and 'save-directory.txt' or 'isolation-verified.txt',saved..'\n');verified=true;log((C.NativeStartup and 'ORIGINAL_SAVE_DIRECTORY ' or 'SAVE_ISOLATION_OK ')..saved)
 end
 -- GC can leave the previous map's pawn discoverable after travel. Resolve the
 -- currently possessed pawn through the persistent game instance instead.
 local gi=FindFirstOf('FS_GameInstance_C')
 if animation.preload then animation.preload(gi)end
 local pawn=valid(gi) and gs:GetPlayerPawn(gi,0) or nil
 local playerClass=findObject('/Game/Player/FS_PlayerChar.FS_PlayerChar_C')
 player=valid(pawn) and valid(playerClass) and pawn:IsA(playerClass) and valid(pawn.CharacterMovement) and pawn or nil
 if menu then
  local block=menu.tick(player)
  if menu.ended then
   journal.offset=0
   items.reset(false)
   rescue.reset()
   checkpoint.reset()
   scenes.reset(true);worldSync.ended()
   for _,h in ipairs(hookIds)do UnregisterHook(h[1],h[2],h[3])end
   hookIds={};hooksReady=false;enemies.uninstall();bodySource.reset();menu.ended=false
  end
  if block then return end
 end
 if uiOnly then return end
 if not valid(player) then
  if C.AutoLoadSlot and not loadedSlot and ticks>30 then
   local gi=FindFirstOf('FS_GameInstance_C');if not valid(gi)then return end
   loadedSlot=true;gi:ForceLoadNewSlot(false,0)
   local level=gi.ActiveSaveSlot.SaveLevel:ToString();assert(level~='' and level~='None','No copied save available')
   gs:OpenLevel(gi,FName(level),true,'')
  end
  return
 end
 local w=world(player);if not w then return end
 if not hooksReady then
  local before=#hookIds;local ok,err=pcall(registerHooks)
  if not ok then for i=#hookIds,before+1,-1 do local h=table.remove(hookIds);pcall(UnregisterHook,h[1],h[2],h[3])end;enemies.uninstall();error(err)end
 end
 -- Cosmetic hooks are unloaded on map travel and must be reinstalled even
 -- while the persistent gameplay hooks remain registered.
 effects.install();audio.install()
 local was=connected;connected=peerConnected()
 local frame=P.frame(read('in.state'))
 if worldSync.poll(player,w,connected,frame)then return end
 local session=C.RunId..'_'..epoch
 local now=gs:GetRealTimeSeconds(player)
 scenes.poll(player,w,connected,now)
 local stable=connected and not scenes.busy()and worldSync.checkpointReady(player)
 items.poll(gi,player,w,session,connected,frame,stable,now)
 if rescue.poll(gi,player,w,session,connected,frame,stable,now)then return end
 checkpoint.poll(gi,player,w,session,connected,frame,stable and rescue.id()==0,now)
 if was~=connected then log(connected and 'CONNECTED' or 'WAITING');if not connected then dispose(remote);remote=nil;remotePose=nil;smooth:reset();effects.reset();audio.reset();enemies.reset(false);lastRemoteAt=nil end end
 if C.TestTarget and C.Role=='host' and targetId=='' then
  local q=player:K2_GetActorLocation();local rot=player:K2_GetActorRotation();local rad=rot.Yaw*math.pi/180
  createTarget({X=q.X+math.cos(rad)*260,Y=q.Y+math.sin(rad)*260,Z=q.Z-40},C.RunId..'-'..epoch..'-target')
 end
 processEvents(w)
 if valid(target) then
  if C.Role=='host' then local hp=math.max(0,target.HP);if hp~=targetHP then version=version+1 end;targetHP=hp;targetPos=target:K2_GetActorLocation();targetDead=targetDead or targetHP<=0 end
 end
 local at=tonumber(player.LastAttackTime) or 0
 if at~=lastAttack then attackSerial=attackSerial+1;lastAttack=at end

 local reportNow=os.clock() -- world real-time restarts after OpenLevel
 if not C.PerfStatusAt or reportNow>=C.PerfStatusAt then
  C.PerfStatusAt=reportNow+2
  local now=gs:GetRealTimeSeconds(player)
  local st=smooth.stats
  write('network-smoothing.txt',string.format('sourceHz=%.2f receiveHz=%.2f bufferMs=%.1f jitterMs=%.1f interpolate=%d extrapolate=%d samples=%d captureHz=30 receivePollHz=60\n',1/math.max(smooth.interval,.001),smooth.receiveInterval and 1/math.max(smooth.receiveInterval,.001)or 0,smooth.delay*1000,smooth.jitter*1000,st.interpolate,st.extrapolate,st.samples))
  write('animation-status.txt',animation.status()..'\n')
  write('effects-status.txt',effects.status())
  write('audio-status.txt',audio.status());write('ipc-status.txt',ipc.status())
  write('enemy-status.txt',enemies.status())
  lookupCost.flush();write('object-refs-status.txt',C.ObjectRefs.status())
  write('game-status.txt',string.format('role=%s connected=%s world=%s target=%s HP=%.3f remote=%s ticks=%d\n',C.Role,tostring(connected),w,targetId,targetHP,full(remote),ticks))
  write('motion-status.txt',string.format('renderFrames=%d renderHz=%.2f accepted=%d state=%s localJumps=%d localLands=%d bufferDelay=%.3f\n',renderFrames,renderFrom and renderFrames/math.max(now-renderFrom,0.01) or 0,smooth.accepted,remoteMotion,jumpSerial,landSerial,smooth.delay))
 end
end
RegisterLoadMapPreHook(function()
 actionDiagnostic=nil
 items.reset(true)
 rescue.reset()
 checkpoint.reset()
 scenes.reset(false);worldSync.resetMap()
 loading=true
 -- Suspend cosmetic observation while the old world is unloaded.
 effects.suspend()
 effects.reset(true)
 audio.suspend()
 audio.reset(true)
 enemies.reset(true)
 if C.MotionTrace then traceRows={};write('motion-trace.csv','')end
 if menu then menu.resetUI()end
 loading=true;epoch=epoch+1;player=nil;remote=nil;target=nil
 targetId='';targetPos=nil;remotePose=nil;peerWorld='';targetDead=false;seenEvents={}
 smooth:reset();renderFrames=0;renderFrom=nil;lastRemoteAt=nil;jumpSerial=0;landSerial=0
 animation.reset();nextCapture=0;nextReceive=0;lastInbound=""
 write('target-binding.txt','')
 log('WORLD_RESET '..epoch)
end)
RegisterLoadMapPostHook(function()loading=false;warmup=ticks+60;end)
local uiPulse=0
-- Menu travel must not execute reentrantly inside another Blueprint's
-- ProcessEvent call. The bundled UE4SS exposes the actual engine-tick queue.
assert(EngineTickAvailable and type(LoopInGameThreadWithDelay)=='function','QDCoop 0.15.2 requires the bundled UE4SS engine-tick timer')
-- One persistent game-thread timer. Do not allocate callback references on an
-- async thread while native hooks are dispatching on the game thread (UE4SS #1180).
-- The isolated launcher's DefaultExecuteInGameThreadMethod is EngineTick.
-- Network work runs from the engine frame scheduler, outside the player's
-- Blueprint input/tick call stack. Local input never awaits a packet or ACK.
frameCost.wrap(enemies,'update','enemies.update')
frameCost.wrap(enemies,'apply','enemies.apply')
frameCost.wrap(effects,'update','effects.update')
frameCost.wrap(effects,'apply','effects.apply')
frameCost.wrap(audio,'update','audio.update')
frameCost.wrap(audio,'apply','audio.apply')
frameCost.wrap(worldSync,'poll','worldSync.poll')
frameCost.wrap(worldSync,'background','worldSync.background')
frameCost.wrap(checkpoint,'poll','checkpoint.poll')
frameCost.wrap(rescue,'poll','rescue.poll')
frameCost.wrap(items,'poll','items.poll')
frameCost.wrap(animation,'apply','body.apply')
frameCost.wrap(journal,'poll','events.poll')
frameCost.setContext(function()
 local e=enemies.inspect();local host,replica,native,fallback=0,0,0,0
 for _ in pairs(e.actors)do host=host+1 end
 for _,r in pairs(e.replicas)do replica=replica+1;if r.native then native=native+1 else fallback=fallback+1 end end
 local pos=valid(player)and player:K2_GetActorLocation();local view=valid(player)and player:GetControlRotation();local scene=pos and string.format(' sceneX=%.1f sceneY=%.1f sceneZ=%.1f sceneYaw=%.1f scenePitch=%.1f',pos.X,pos.Y,pos.Z,view.Yaw,view.Pitch)or' sceneUnavailable=true'
 return scene..' '..string.format('role=%s connected=%s loading=%s world=%s hostActors=%d activeActors=%d replicas=%d native=%d fallback=%d bodyWrites=%d bodySkipped=%d luaKB=%.0f eventsProcessed=%d eventErrors=%d %s %s',C.Role,tostring(connected),tostring(loading),e.world or '-',host,#(e.activeVisuals or{}),replica,native,fallback,animation.boneWrites or 0,animation.boneSkips or 0,collectgarbage('count'),journal.processed,journal.errors,ipc.status(),journal.status())..' luaGcMode='..C.GCPolicy.mode
end)
local nextControl = 0
LoopInGameThreadWithDelay(0,function()
 C.ObjectRefs.activate()
 local frameStart=frameCost.clock();frameCost.begin(frameStart);hookTrace.poll(frameStart);ipc.flush();frameCost.add('ipc',(frameCost.clock()-frameStart)*1000)
 if disabled or loading then frameCost.finish(frameCost.clock());return end
 local now = os.clock()
 if now >= nextControl then
  nextControl = now + (valid(player) and .05 or .016)
  local controlStart=frameCost.clock()
  if not verified then
   local ok,err=pcall(tick,false)
   if not ok then disabled=true;write('error.txt',tostring(err));log('STARTUP STOP '..tostring(err))end
  else guard.call('control',tick,false)end;frameCost.add('control',(frameCost.clock()-controlStart)*1000)
 end
 if verified and valid(player) then
  local stageStart=frameCost.clock();guard.call('motion-service',serviceMotion);frameCost.add('service',(frameCost.clock()-stageStart)*1000)
  local stageStart=frameCost.clock();guard.call('presentation',presentRemote);frameCost.add('presentation',(frameCost.clock()-stageStart)*1000)
 end
 frameCost.add('total',(frameCost.clock()-frameStart)*1000);frameCost.finish(frameCost.clock())
end)
-- Read-only inspection surface for automated local validation; no network command execution.
_G.QDCoopEnemies=enemies.inspect
_G.QDCoopScenes=scenes.inspect
_G.QDCoopWorld=worldSync.inspect
_G.QDCoopInspect=function()return {smoothingStats=smooth.stats,player=player,remote=remote,target=target,targetId=targetId,hp=targetHP,version=version,connected=connected,disabled=disabled,epoch=epoch}end
log('QD COOP 1.18.0-preview3 loaded; role='..C.Role..' engineTickQueue='..tostring(EngineTickAvailable))



