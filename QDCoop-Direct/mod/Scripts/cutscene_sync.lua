-- A native sequence is presentation; its detached completion delegates are a
-- separate authority transaction. No arbitrary remote function/path dispatch.
return function(dir,C,valid,full,read,write,log,world,enemies)
 local N=dofile(dir..'encounter_native.lua')(dir,valid)
 local W=dofile(dir..'world_protocol.lua')
 local managers={
  '/Game/Cutscenes/Ending1/Ending1CutsceneManager.Ending1CutsceneManager_C',
  '/Game/Cutscenes/Ending2/Ending2CutsceneManager.Ending2CutsceneManager_C',
  '/Game/Cutscenes/Ending3/Ending3CutsceneManager.Ending3CutsceneManager_C',
  '/Game/Cutscenes/Ending4/Ending4CutsceneManager.Ending4CutsceneManager_C',
  '/Game/Cutscenes/RitualCutscene/RitualCutsceneManager.RitualCutsceneManager_C',
  '/Game/Cutscenes/SipehaTransition/SipehaTransitionCutsceneManager.SipehaTransitionCutsceneManager_C',
 }
 local callbacks={
  {manager=1,cls='/Game/Enemies/Queen/Queen_BP.Queen_BP_C',fn='Ending1Complete'},
  {manager=2,cls='/Game/Enemies/Queen/Queen_BP.Queen_BP_C',fn='Ending2CutsceneEnded'},
  {manager=6,cls='/Game/Enemies/Queen/Queen_BP.Queen_BP_C',fn='SipehaTransitionEnded'},
  {manager=3,cls='/Game/Enemies/Sipeha/Sipeha_NPC.Sipeha_NPC_C',fn='CustomEvent_0'},
  {manager=4,cls='/Game/Enemies/Sipeha/Sipeha_NPC.Sipeha_NPC_C',fn='CustomEvent'},
  {manager=5,cls='/Game/Blueprints/SecretSkullRitual_BP.SecretSkullRitual_BP_C',fn='CustomEvent_0'},
 }
 local maps={'998_QueenCutscenes','998_QueenCutscenes','999_SipehaCutscenes','999_SipehaCutscenes','997_FinalZoneCutscene','998_QueenCutscenes'}
 local leases={}
 local M={serial=0,commits=0,starts=0,recoveries=0,callbacks=callbacks,managers=managers}
 local skipPath='/Game/Cutscenes/CutsceneManagerBase.CutsceneManagerBase_C:SkipCutscene'
 local gs=StaticFindObject('/Script/Engine.Default__GameplayStatics')
 local context,scene,playingRemote,requestSerial,installed,guardAt=nil,nil,false,0,false,0
 local requests={};local completed=0;local lastRoom='';local cleaning=false
 local function path(o)return full(o):match(' (/Game/.*)$')or''end
 local function object(p)return p~=''and C.ObjectRefs.find(p)or nil end
 local function managerId(m)local c=path(m:GetClass());for i,p in ipairs(managers)do if p==c then return i end end end
 local function manager(id)
  local l=gs:GetStreamingLevel(context.player,FName('/Game/RuinsLevel/Maps/'..maps[id]))
  if valid(l)then
   if not leases[maps[id]]then leases[maps[id]]={level=l,loaded=l:IsLevelLoaded(),visible=l:IsLevelVisible()}end
   l:SetShouldBeLoaded(true);l:SetShouldBeVisible(true)
   if not l:IsLevelLoaded()or not l:IsLevelVisible()then return end
  end
  local c=C.ObjectRefs.find(managers[id]);if not valid(c)then return end
  local list={};gs:GetAllActorsOfClass(context.player,c,list)
  for _,v in ipairs(list)do local o=v:get();if valid(o)and not o:IsActorBeingDestroyed()and valid(o.Sequencer)then return o end end
 end
 local function send(op,s,value)
  if not context or context.room==''then return end
  write('out.events',table.concat({'SC',context.room,s.gen or 0,op,s.manager or 0,s.callback or 0,W.escape(s.source or''),value or'-'},'|')..'\n',true)
 end
 local function report()
  write('cutscene-status.txt',string.format('role=%s connected=%s generation=%s manager=%s stage=%s localDone=%s peerDone=%s commits=%d starts=%d recoveries=%d recovery=%s error=%s\n',C.Role,tostring(context and context.connected),scene and scene.gen or'-',scene and scene.manager or'-',scene and scene.stage or'idle',tostring(scene and scene.done),tostring(scene and scene.peerDone),M.commits,M.starts,M.recoveries,M.lastRecovery or'none',M.error or'none'))
 end
 local function protect(s)
  local p=context and context.player;if not valid(p)then return end
  if not s.held then s.held=true;s.damageBefore=p.bCanBeDamaged;p:LockInputForLoadingOrCutscene();p:HideHUD()end
  p.bCanBeDamaged=false
 end
 local function release(s,force)
  if not s or s.controlsReleased then return true end
  force=force or(s.entered and not s.done)
  local p=s.player or(context and context.player)
  if valid(p)then
   local current=p.CurrentCutsceneManager
   local owns=valid(current)and valid(s.actor)and full(current)==full(s.actor)
   -- A new cutscene or a native teleport owns its own input lock.
   if force and not s.done and s.entered and not s.noPlayback and(not valid(current)or owns)and not p.Teleporting and not p.QuickTeleporting then
    cleaning=true
    local ok,err=pcall(function()
     if valid(s.sequence)then s.sequence:Stop()end
     local manager=valid(s.actor)and not s.actor:IsActorBeingDestroyed()and s.actor or StaticFindObject('/Game/Cutscenes/CutsceneManagerBase.CutsceneManagerBase_C'):GetCDO()
     p:EndManagerCutscene(manager)
     local controller=p:GetController();if valid(controller)then controller:SetViewTargetWithBlend(p,0,0,0,false)end
    end)
    cleaning=false;if not ok then error(err)end
    s.held=true;s.damageBefore=true;M.recoveries=M.recoveries+1;M.lastRecovery='native-end-missing'
   elseif force and not s.controlsReleased and(p.Teleporting or p.QuickTeleporting)then return false end
   if s.held and not valid(p.CurrentCutsceneManager)and not p.Teleporting and not p.QuickTeleporting then
    p:UnlockInputForLoadingOrCutscene();p.bCanBeDamaged=s.damageBefore~=false;p:UnhideHUD();s.held=false
   end
  end
  local lease=leases[maps[s.manager]]
  if lease and valid(lease.level)then lease.level:SetShouldBeLoaded(lease.loaded);lease.level:SetShouldBeVisible(lease.visible)end
  leases[maps[s.manager]]=nil
  s.controlsReleased=true;return true
 end
 local function position(s)
  if not valid(s.sequence)then return 0 end
  local t=s.sequence:GetCurrentTime();return(t.Time.FrameNumber.Value+t.Time.SubFrame)*t.Rate.Denominator/math.max(1,t.Rate.Numerator)
 end
 local function seek(s,t)
  if t>.15 then s.sequence:SetPlaybackPosition({Time=t,PositionType=1,UpdateMethod=1})end
 end
 local function callbackId(escrow,id)
  if #escrow.callbacks==0 then return 0,''end
  if #escrow.callbacks~=1 then return nil end
  local cb=escrow.callbacks[1]
  for i,v in ipairs(callbacks)do if v.manager==id and cb.path==v.cls..':'..v.fn then
   local r=enemies.byActor[full(cb.object)]
   return i,r and('enemy:'..r.id)or path(cb.object)
  end end
 end
 local function begin(m)
  if not context or not context.connected or not valid(m)then return end
  local id=managerId(m);if not id then return end
  manager(id) -- Acquire the cinematic streaming level, even if locally hidden.
  local escrow=N.take(m)
  if playingRemote then
   if scene then scene.escrow=escrow;scene.actor=m;scene.sequence=m.Sequencer:GetSequencePlayer();scene.entered=true else N.putBack(escrow)end
   return
  end
  if scene and scene.actor==m and scene.stage~='committed'then
   -- Duplicate Begin must not overwrite the first completion transaction.
   for _,cb in ipairs(escrow.callbacks)do
    local duplicate=false;for _,old in ipairs(scene.escrow.callbacks)do if old.path==cb.path and full(old.object)==full(cb.object)then duplicate=true;break end end
    if not duplicate then scene.escrow.callbacks[#scene.escrow.callbacks+1]=cb end
   end
   N.discard(escrow);return
  end
  if scene then
   -- A native completion may immediately start the next sequence. Never lose
   -- an earlier, still pending completion if two native triggers overlap.
   if C.Role=='host'and not scene.committed then
    local previous=scene.remoteCallback and{scene.remoteCallback}or(scene.escrow and scene.escrow.callbacks)or{}
    for i=#previous,1,-1 do local cb=previous[i];local duplicate=false
     for _,new in ipairs(escrow.callbacks)do if new.path==cb.path and full(new.object)==full(cb.object)then duplicate=true;break end end
     if not duplicate then table.insert(escrow.callbacks,1,cb)end
    end
   end
   if not scene.done and valid(scene.sequence)then scene.sequence:Stop()end
   release(scene);N.discard(scene.escrow)
  end
  local cb,source=callbackId(escrow,id)
  scene={manager=id,actor=m,sequence=m.Sequencer:GetSequencePlayer(),player=context.player,entered=true,escrow=escrow,callback=cb or 0,source=source or'',stage='waiting',born=context.now,gen=0}
  if C.Role=='host'then M.serial=M.serial+1;scene.gen=M.serial;send('B',scene,'waiting,0')
  else
   if cb==nil then M.error='Unmapped guest completion delegate: playback only; story callback suppressed';log('CUTSCENE '..M.error)end
   requestSerial=requestSerial+1;scene.request=C.RunId..'_'..requestSerial..'_'..math.floor(os.clock()*1000000);send('Q',scene,scene.request)
  end
  M.starts=M.starts+1;report()
 end
 local function localEnded(p,m)
  if cleaning or not scene or scene.done or scene.actor~=m or not context or full(p)~=full(context.player)then return end
  scene.done=true;scene.finishedAt=context.now;scene.stage='finished';protect(scene);send('F',scene)
  if C.Role=='host'then send('K',scene)end;report()
 end
 function M.install(hooks)
  if installed then return end;installed=true
  local function hook(p,fn)local a,b=RegisterHook(p,function(...)local ok,err=pcall(fn,...);if not ok then M.error=tostring(err);log('CUTSCENE '..M.error);report()end end);hooks[#hooks+1]={p,a,b}end
  hook('/Game/Player/FS_PlayerChar.FS_PlayerChar_C:BeginCutsceneWithManager',function(_,m)begin(m:get())end)
  hook('/Game/Player/FS_PlayerChar.FS_PlayerChar_C:EndManagerCutscene',function(p,m)localEnded(p:get(),m:get())end)
  hook('/Game/Cutscenes/CutsceneManagerBase.CutsceneManagerBase_C:SkipCutscene',function(ctx)
   if C.Role=='host'and scene and scene.actor==ctx:get()and not playingRemote then scene.skip=true;send('K',scene)end
  end)
 end
 local function invoke(s)
  -- Mark before invocation; an exception after a side effect is never retried.
  s.committed=true;s.stage='committing';M.commits=M.commits+1
  local before=W.decode(read('out.world'));s.beforeRevision=before and before.revision or 0
  write('cutscene-commits.log',context.room..'|'..s.gen..'|begin\n',true)
  local jobs=s.escrow and s.escrow.callbacks or{}
  if s.remoteCallback then jobs={s.remoteCallback}end
  for _,cb in ipairs(jobs)do
   local ok,err=pcall(function()assert(valid(cb.object),'Completion owner expired before commit');cb.object[cb.fn](cb.object)end)
   if not ok then s.failed=true;M.error=tostring(err);log('CUTSCENE completion failed; not replaying '..M.error);break end
  end
  N.discard(s.escrow);world.storyChanged();s.stage='settling';s.settleAt=context.now+1;s.progressDeadline=context.now+15
  write('cutscene-commits.log',context.room..'|'..s.gen..(s.failed and'|failed\n'or'|applied\n'),true)
 end
 local function skip(s)
  if not s.done and valid(s.actor)then playingRemote=true;local ok,err=pcall(function()N.original(skipPath,function()s.actor:SkipCutscene()end)end);playingRemote=false;if not ok then error(err)end end
 end
 local function remoteOwner(id,source,peer)
  if id==0 then return true end
  local spec=callbacks[id];if not spec then return end
  local e;local enemy=tonumber(source:match('^enemy:(%d+)$'))
  if enemy then local r=enemies.actors[enemy];e=r and r.e else e=object(source)end
  if not valid(e)or not e:IsA(spec.cls)then return end
  if enemy and not(e.bDead or e.HP<=0)then return end
  if not enemy then
   local p=e:K2_GetActorLocation();if not peer or(p.X-peer.X)^2+(p.Y-peer.Y)^2+(p.Z-peer.Z)^2>1800^2 then return end
  end
  return {object=e,fn=spec.fn,path=spec.cls..':'..spec.fn}
 end
 function M.event(f,peer)
  if not context or not context.connected or #f~=9 or f[2]~='SC'or f[3]~=context.room then return end
  local gen=W.integer(f[4],0,1e9);local op=f[5];local id=W.integer(f[6],1,#managers);local cb=W.integer(f[7],0,#callbacks);local source=W.unescape(f[8])
  if not gen or not id or not cb or not source or #source>450 or #f[9]>100 or(cb>0 and callbacks[cb].manager~=id)then return end
  if C.Role=='host'and op=='Q'then
   if gen~=0 or not f[9]:match('^[%w_%-]+$')then return end
   if requests[f[9]]then
    if scene and scene.gen==requests[f[9]]and scene.stage~='committed'then send('B',scene,scene.stage..','..position(scene))else send('A',{gen=0,manager=id,callback=cb,source=source},f[9])end;return
   end
   if scene and scene.stage~='committed'then
    if scene.manager==id then requests[f[9]]=scene.gen;send('B',scene,scene.stage..','..position(scene))else send('A',{gen=0,manager=id,callback=cb,source=source},f[9])end;return
   end
   local owner=remoteOwner(cb,source,peer);if not owner then return end
   local m=manager(id);if not m then return 'retry'end
   M.serial=M.serial+1;scene={gen=M.serial,manager=id,callback=cb,source=source,actor=m,sequence=m.Sequencer:GetSequencePlayer(),player=context.player,remoteCallback=type(owner)=='table'and owner or nil,stage='waiting',born=context.now}
   playingRemote=true;local ok,err=pcall(function()m:BeginCutscene(nil)end);playingRemote=false;if not ok then error(err)end
   requests[f[9]]=scene.gen;M.starts=M.starts+1;send('B',scene,'waiting,0');report();return
  end
  if C.Role=='client'and op=='A'and scene and scene.gen==0 and scene.request==f[9]then
   skip(scene);release(scene);N.discard(scene.escrow);scene=nil;report();return
  end
  if C.Role=='client'and op=='B'then
   if gen<=completed or(scene and scene.gen>gen)then return end
   if not scene or scene.gen~=gen then
    if scene and scene.gen==0 and scene.manager==id then scene.gen=gen
    else
     local m=manager(id);if not m then return 'retry'end
     if scene then skip(scene);release(scene);N.discard(scene.escrow)end
     scene={gen=gen,manager=id,callback=cb,source=source,actor=m,sequence=m.Sequencer:GetSequencePlayer(),player=context.player,stage='waiting',born=context.now}
     playingRemote=true;local ok,err=pcall(function()m:BeginCutscene(nil)end);playingRemote=false;if not ok then error(err)end;M.starts=M.starts+1
    end
   end
   send(scene.done and'F'or'R',scene);report();return
  end
  if C.Role=='client'and op=='E'and gen<=completed then
   local rev=W.integer(f[9],0,1e9);local ws=world.inspect()
   if rev and ws.applied and ws.applied.revision>=rev and world.checkpointReady(context.player)then send('Z',{gen=gen,manager=id,callback=cb,source=source})end;return
  end
  if C.Role=='client'and op=='E'and gen>completed and(not scene or gen>scene.gen)then
   local rev=W.integer(f[9],0,1e9);if not rev then return end
   if scene then skip(scene);release(scene,true);N.discard(scene.escrow)end
   scene={gen=gen,manager=id,callback=cb,source=source,player=context.player,noPlayback=true,done=true,stage='settling',commitRevision=rev,progressDeadline=context.now+15}
  end
  if not scene or gen~=scene.gen or id~=scene.manager then return end
  if C.Role=='host'and op=='R'then
   scene.ready=true
   if scene.stage=='committed'then send('E',scene,tostring(scene.commitRevision));return end
   if scene.stage=='waiting'then scene.stage='playing';scene.sequence:Play()end
   send('P',scene,tostring(position(scene)))
  elseif C.Role=='client'and op=='P'and not scene.done then
   local t=W.number(f[9],0,7200);if not t then return end
   if scene.stage=='waiting'then seek(scene,t);scene.sequence:Play();scene.stage='playing'end
  elseif op=='K'and C.Role=='client'then
   skip(scene)
  elseif C.Role=='host'and op=='F'then scene.peerDone=true
  elseif C.Role=='client'and op=='E'then
   local rev=W.integer(f[9],0,1e9);if not rev then return end
   if scene.stage=='committed'then send('Z',scene);return end
   skip(scene);scene.commitRevision=rev;scene.stage='settling';scene.progressDeadline=scene.progressDeadline or(context.now+15)
  elseif op=='Z'and scene.stage=='committed'then
   scene.acked=true;if C.Role=='host'then send('Z',scene)end
  end
  report()
 end
 function M.poll(player,w,connected,now)
  local state=world.inspect();local room=C.Role=='host'and state.token or(state.applied and state.applied.session)or''
  if room~=''and lastRoom~=room then M.reset(false);completed=0;lastRoom=room end
  context={player=player,world=w,room=room,connected=connected,now=now}
  if connected and C.Role=='client'and now>=guardAt then
   guardAt=now+1;for _,c in ipairs(callbacks)do N.guard(c.cls..':'..c.fn)end;N.guard(skipPath)
  elseif not connected then N.restore()end
  local s=scene;if not s then return end
  local lease=leases[maps[s.manager]]
  if s.stage~='committed'and lease and valid(lease.level)and not lease.level:IsLevelVisible()then lease.level:SetShouldBeLoaded(true);lease.level:SetShouldBeVisible(true)end
  if not s.noPlayback and(not valid(s.actor)or s.actor:IsActorBeingDestroyed()or not valid(s.sequence))then
   if release(s,true)then completed=math.max(completed,s.gen or 0);N.discard(s.escrow);scene=nil;M.lastRecovery='sequence-unavailable';report()end;return
  end
  if not connected then
   s.disconnected=s.disconnected or now
   if now-s.disconnected>=3 then
    if C.Role=='client'then skip(s);if release(s,true)then completed=math.max(completed,s.gen or 0);N.discard(s.escrow);scene=nil;M.lastRecovery='peer-disconnected';report()end;return end
   end
  else s.disconnected=nil end
  if s.stage=='waiting'then
   if C.Role=='host'and(now-s.born>=8 or(s.disconnected and now-s.disconnected>=3))then s.stage='playing';s.sequence:Play();send('P',s,tostring(position(s)))
   else s.sequence:Pause()end
  elseif s.stage=='finished'then protect(s)end
  if C.Role=='host'and s.done and not s.committed and(s.peerDone or not connected or now-(s.finishedAt or now)>=3)then invoke(s)end
  if s.stage=='settling'then
   if s.progressDeadline and now>=s.progressDeadline and not s.controlsReleased then release(s,true);M.lastRecovery='progress-wait-input-released';report()end
   local ws=world.inspect()
   if C.Role=='host'and now>=(s.settleAt or now)and not player.Teleporting and not player.QuickTeleporting then
    local raw=read('out.world');local snapshot=W.decode(raw)
    if snapshot and snapshot.revision>(s.beforeRevision or 0)then
     s.commitRevision=snapshot.revision;s.stage='committed';release(s);completed=math.max(completed,s.gen);send('E',s,tostring(s.commitRevision));report()
    end
   elseif C.Role=='client'and ws.applied and ws.applied.revision>=(s.commitRevision or math.huge)and world.checkpointReady(player)then
    if s.manager==5 and s.callback==6 then
     local actor=object(s.source)
     if valid(actor)and actor:IsA(callbacks[6].cls)then actor:SetToEndState()end
    end
    release(s);N.discard(s.escrow);s.stage='committed';completed=math.max(completed,s.gen);send('Z',s);report()
   end
  end
  if connected and not s.acked and now>=(s.nextSend or 0)then
   s.nextSend=now+1
   if C.Role=='host'then
    if s.stage=='committed'then send('E',s,tostring(s.commitRevision))else send('B',s,s.stage..','..position(s))end
   elseif s.stage=='committed'then send('Z',s)
   elseif s.gen==0 then send('Q',s,s.request)
   elseif s.done then send('F',s)else send('R',s)end
  end
 end
 function M.busy()return scene~=nil and scene.stage~='committed'and not scene.controlsReleased end
 function M.reset(uninstall)
  if scene then completed=math.max(completed,scene.gen or 0);release(scene,true);if C.Role=='host'and not scene.committed then N.putBack(scene.escrow)else N.discard(scene.escrow)end end
  for name,lease in pairs(leases)do if valid(lease.level)then lease.level:SetShouldBeLoaded(lease.loaded);lease.level:SetShouldBeVisible(lease.visible)end;leases[name]=nil end
  N.restore();scene=nil;context=nil;requests={};guardAt=0
  if uninstall then installed=false;lastRoom='';completed=0 end
 end
 function M.inspect()return{scene=scene,context=context,starts=M.starts,commits=M.commits,error=M.error,recoveries=M.recoveries,lastRecovery=M.lastRecovery}end
 return M
end
