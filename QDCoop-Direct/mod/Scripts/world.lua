return function(dir,C,valid,full,read,write,log,checkpoint)
 local W=dofile(dir..'world_protocol.lua');local M={};local gs=StaticFindObject('/Script/Engine.Default__GameplayStatics')
 local Gates=dofile(dir..'encounter_gates.lua').runtime(valid,full,StaticFindObject,gs,W)
 local Travel=dofile(dir..'world_travel_policy.lua')(dir,C,valid)
 local Books=dofile(dir..'world_library_books.lua')(valid,full)
 local localShopTravel=false;local shopRejoinPending=false
 local locationFields={SavedArea=true,PlayingMusicTrack=true,SaveLevel=true,SavePointID=true,LastTouchedCheckpoint=true}
 local Delta=dofile(dir..'world_delta.lua')
 local Interaction=dofile(dir..'world_interaction_presentation.lua')(valid,gs)
 local reconcile=dofile(dir..'reconcile_queue.lua')(os.clock,function(key,err,count)
  write('world-reconcile-error.txt',key..' failures='..count..' '..tostring(err)..'\n');log('WORLD pending '..key..' '..tostring(err))
 end)
 local elevators=dofile(dir..'world_elevators.lua')(valid)
 local shrineGates=dofile(dir..'world_shrine_gates.lua')(valid)
 local gi,baseline,diskCopy,joined,pending,latest,lastRaw,applied,spawnPending
 local pickupGUIDs=dofile(dir..'personal_pickup_guids.lua');local pickupClass
 local actorRefresh,snapshotJob,snapshot
 local mapRevision=0;local disconnectAt;local pausedMovement
 local rollback
 local rollbackListener=function()end
 function M.setRollbackListener(fn)rollbackListener=fn end
 local syncCopies,asyncCopies={},{}
 local hooks={};local hooked=false;local replay=dofile(dir..'replay_scope.lua')();local changeVersion=0;local overlayFault=false;local tickCount=0;local revision=0;local travel=0;local room=0;local token='';local lastBody='';local lastPos;local lastAppliedStreams='';local actorIndex={};local stats={snapshots=0,restored=0,requests=0,accepted=0,saves=0,travels=0}
 local copyNames={}
 local function asset(o)return valid(o)and full(o):match(' (/Game/.*)$')or ''end
 local function guid(g)return string.format('%d,%d,%d,%d',g.A,g.B,g.C,g.D)end
 local pickupMaps={SavableActorStates=true,SavableActorItems=true}
 local function isPickup(a)
  if not valid(a)then return false end
  if not valid(pickupClass)then pickupClass=StaticFindObject('/Game/Blueprints/CoreItemPickupActor.CoreItemPickupActor_C')end
  return valid(pickupClass)and a:IsA(pickupClass)
 end
 local function copyPickups(from,to,ids)
  for name in pairs(pickupMaps)do for k in pairs(ids)do local g=W.guid(k);local p=from[name]:Contains(g)and from[name]:Find(g);local v=p and p:get();to[name]:Remove(g);if p then to[name]:Add(g,v)end end end
 end
 local function classifyPickup(a,restoreBeforePlay)
  if not isPickup(a)then return false end
  local ids={};a.GUIDs:ForEach(function(_,v)local k=guid(v:get());pickupGUIDs[k]=true;ids[k]=true end)
  if restoreBeforePlay and joined and valid(baseline)and valid(gi)and valid(gi.ActiveSaveSlot)then copyPickups(baseline,gi.ActiveSaveSlot,ids)end
  return true
 end
 -- Restore the selected character's pickup flags before native BeginPlay reads
 -- them, including pickups in streamed areas absent at the time of joining.
 RegisterBeginPlayPreHook(function(ctx)
  if C.Role~='host'and C.Role~='client'then return end
  local a=ctx:get()
  if joined and C.Role=='client'then Books.lock(a)end
  local ok,err=pcall(classifyPickup,a,true)
  if not ok then write('error.txt','Personal pickup initialization: '..tostring(err))end
 end)
 RegisterBeginPlayPostHook(function(ctx)
  if joined and C.Role=='client'then Books.lock(ctx:get())end
 end)
 local function actors(path)
  local cls=StaticFindObject(path);if not valid(cls)then return{}end
  local out={};gs:GetAllActorsOfClass(gi,cls,out);local a={};for _,v in ipairs(out)do local o=v:get();if valid(o)then a[#a+1]=o end end;return a
 end
 local function resolve(path)if path==''then return nil end;local o=StaticFindObject(path);if not valid(o)then o=LoadAsset(path)end;assert(valid(o),'World asset unavailable: '..path);return o end
 local Music=dofile(dir..'world_music.lua')(valid,asset,resolve)
 local StoryRewards=dofile(dir..'world_story_rewards.lua')(valid,log)
 local function root(o)local a={};gi.ReferencedObjects:ForEach(function(_,p)local v=p:get();if valid(v)then a[#a+1]=v end end);a[#a+1]=o;gi.ReferencedObjects=a end
 local function unroot(o)if not valid(gi)or not valid(o)then return end;local a={};gi.ReferencedObjects:ForEach(function(_,p)local v=p:get();if valid(v)and full(v)~=full(o)then a[#a+1]=v end end);gi.ReferencedObjects=a end
 local function setSave(save)
  gi.ActiveSaveSlot=save
  gi.BaseActiveSave=save
 end
 local function clone(s)
  local d=StaticConstructObject(s:GetClass(),gi);assert(valid(d),'Save copy construction failed');root(d)
  if #copyNames==0 then
   for _,c in ipairs({s:GetClass(),StaticFindObject('/Script/QueensDomain.FS_SaveGame')})do c:ForEachProperty(function(p)local n=p:GetFName():ToString();local f=p:GetFullName();if n~='UberGraphFrame'and not f:find('DelegateProperty',1,true)then copyNames[#copyNames+1]=n end end)end
  end
  for _,n in ipairs(copyNames)do d[n]=s[n]end;return d
 end
 clone=C.Perf.measure(clone,'world.cloneSave')
 local function personalSave(s)
  -- Live overlays already preserve the guest's inventory, XP and pickup keys.
  -- Persist that merged world; restoring the pre-join baseline here resurrected
  -- the intro, barricades and bosses when this character later hosted.
  local player=gs:GetPlayerPawn(gi,0)
  if full(s)==full(gi.ActiveSaveSlot)then StoryRewards.ensure(gi,player)end
  local copy=clone(s)
  if not copy.IsSuspendSave and not spawnPending and valid(player)and not player.IsDead and not player.Teleporting and not player.QuickTeleporting and not player.DoingTeleportCrystal and not player.SafeLocationIsForcedMidair then
   local manager=FindFirstOf('FS_LevelManager');local music=gi.MusicController
   if valid(manager)and not manager:IsProcessingLevelChanges(gi)and valid(music)then
    -- Primary saves spawn at SavePointID: native player loading does not read
    -- SaveLocation. Use the game's existing resumable-save representation in
    -- this disk copy, including the streaming/music state from the SAME safe
    -- location. Native loading then restores the guest's feet position and
    -- capsule height correctly when this character later hosts or plays solo.
    copy.SaveLocation=player.LastSafeLocation
    copy.SuspendLocation=player.LastSafeLocation
    copy.SuspendPositionIsForcedMidair=false
    copy.LevelLoadingSuspendState=manager:GetSafeLocationSuspendState()
    local areas={};for _,v in ipairs(music:GetSafeLocationSuspendState())do areas[#areas+1]=v:get()end
    copy.MusicAreaSuspendState=areas
    copy.IsSuspendSave=true
   end
  end
  stats.saves=stats.saves+1;return copy
 end
 local function indexActor(a,index)
  if not valid(a)then return end
  local name=full(a)
  if not name:find('Default__',1,true)then
   if joined and C.Role=='client'then Books.lock(a,name)end
   classifyPickup(a,false);local path=name:match(' (.+)$')
   a.GUIDs:ForEach(function(i,g)index[guid(g:get())]={path=path,index=i-1}end)
  end
 end
 local function refreshActors(background)
  if background then
   if not actorRefresh then actorRefresh={list=actors('/Game/Blueprints/SaveSystem/SavableActor.SavableActor_C'),at=1,index={}}end
  else
   actorRefresh=nil;actorIndex={};for _,a in ipairs(actors('/Game/Blueprints/SaveSystem/SavableActor.SavableActor_C'))do indexActor(a,actorIndex)end
  end
 end
 function M.background()
  Interaction.tick()
  local started=os.clock()
  if reconcile.count>0 then reconcile.run(.0007,3)end
  if actorRefresh then for i=1,12 do local a=actorRefresh.list[actorRefresh.at];if not a then actorIndex=actorRefresh.index;actorRefresh=nil;break end;actorRefresh.at=actorRefresh.at+1;indexActor(a,actorRefresh.index);if os.clock()-started>=.001 then break end end end
  if snapshotJob and os.clock()-started<.0015 then
   local job=snapshotJob;local ok,done=pcall(snapshot,job.player,job.world,job)
   if not ok or done then snapshotJob=nil end
   if not ok then error(done)end
  end
 end
 local function live(b)return b and b.path and StaticFindObject(b.path)or nil end
 local function restore(a)
  if not valid(a)then return end
  if elevators.is(a)then elevators.restore(a,gi.ActiveSaveSlot);return end
  local before=Interaction.before(a)
  a.MySaveData:Empty();a:RestoreFromSave();Interaction.after(a,before)
  if joined and C.Role=='client'then Books.lock(a)end
  stats.restored=stats.restored+1
 end
 local function transform(pos,yaw)local r=math.rad(yaw or 0)/2;return{Rotation={X=0,Y=0,Z=math.sin(r),W=math.cos(r)},Translation=pos,Scale3D={X=1,Y=1,Z=1}}end
 local function streamsKey(s)local r={};for n=1,2 do for _,v in ipairs(s[n])do r[#r+1]=n..':'..v.path..':'..v.priority end end;return table.concat(r,';')end
 local function applyStreams(snapshot)
  local lm=FindFirstOf('FS_LevelManager');if not valid(lm)then return false end
  local lists={{},{}};for n=1,2 do for _,v in ipairs(snapshot.streams[n])do lists[n][#lists[n]+1]={List=resolve(v.path),Priority=v.priority,bTrigger=n==2}end end
  -- Nested TArrays in a Lua table are not marshalled by this UE4SS UFunction
  -- struct argument path. Populate a real reflected struct before passing it.
  local state=gi.ActiveSaveSlot.LevelLoadingSuspendState
  state.SafeLocLevelListStack=lists[1];state.SafeLocTriggerLevelListStack=lists[2]
  lm:RestoreFromSuspendState(state);lm:FlushLevelManagerActions(gi,false)
  lastAppliedStreams=streamsKey(snapshot.streams);return true
 end
 local function reconcilePresentation(snapshot)
  if not snapshot then return end
  for k,v in pairs(snapshot.doors)do
   local key,wanted=k,v
   reconcile.put('door:'..key,function(work)replay.run(function()
    local a=live(actorIndex[key]);if valid(a)and a:IsA(StaticFindObject('/Game/Blueprints/Door_Base.Door_Base_C'))then
     if work.wanted~=wanted.open then work.wanted=wanted.open;work.action=nil end
     if a.CurrentActivationCount~=wanted.count then a.CurrentActivationCount=wanted.count end
     if shrineGates.is(a)then
      -- Compare actual gate presentation, without letting the completed quest
      -- mask a still-closed local collision box during reconciliation.
      local t=shrineGates.timeline(a);local open=t:IsPlaying()and not t:IsReversing()or(not t:IsPlaying()and t:GetPlaybackPosition()>=t:GetTimelineLength()-.001)
      if open~=wanted.open or work.action then work.action='shrine';shrineGates.apply(a,wanted.open);work.action=nil end
      return
     end
     if wanted.open and(not a.Opened or work.action=='open')then work.action='open';a.Opened=true;a:OpenDoor();work.action=nil
     elseif not wanted.open and(a.Opened or work.action=='close')and a.ScriptOpenable then work.action='close';a.Opened=false;a:CloseDoor();work.action=nil end
    end
   end)end)
  end
  for k,v in pairs(snapshot.levers)do
   local key,wanted=k,v
   reconcile.put('lever:'..key,function(work)replay.run(function()
    if work.wanted~=wanted then work.wanted=wanted;work.pending=nil end
    local a=live(actorIndex[key]);if valid(a)and a:IsA(StaticFindObject('/Game/LD45/Blueprints/Objects/Gate/Lever.Lever_C'))and(a.ToggledOn~=wanted or work.pending)then
     work.pending=true;local before=Interaction.before(a);a.ToggledOn=wanted;if wanted then a['Restore to switched'](a)end;Interaction.after(a,before);work.pending=nil
    end
   end)end)
  end
  for k,v in pairs(snapshot.gates or{})do
   reconcile.put('encounter-gate:'..k,function()if not Gates.apply(v)then return 'retry' end end)
  end
 end
 local function restoreGuestActor(key)
  local ok,err=pcall(replay.run,function()local a=live(actorIndex[key]);if valid(a)then restore(a)end end)
  -- Native lever restore can activate a door after an older queued correction.
  -- Reconcile again against the newest snapshot, including partial failures.
  reconcilePresentation(applied)
  if not ok then error(err)end
 end
 local function overlay(s,snapshot,initial)
  local fields=W.fields
  if localShopTravel then fields={};for name,kind in pairs(W.fields)do if not locationFields[name]then fields[name]=kind end end end
  local plan=Delta.plan(s,snapshot,fields,{
   asset=asset,resolve=resolve,name=FName,guid=guid,parseGuid=W.guid,
   personal=function(name,key)return(localShopTravel and locationFields[name])or(name=='NamedSaveData'and W.personalName(key))or(pickupMaps[name]and pickupGUIDs[key])end
  })
  -- No resource load, actor restoration or notification occurs while live save
  -- fields are being committed. A failed setter rolls back the touched keys.
  local ok,err=pcall(replay.run,function()Delta.commit(s,plan)end)
  overlayFault=not ok;if not ok then error(err)end
  stats.fieldWrites=(stats.fieldWrites or 0)+#plan.ops;stats.lastFieldWrites=#plan.ops
  applied=snapshot;stats.snapshots=stats.snapshots+1
  if initial then return end
  for k in pairs(plan.actors)do
   local key=k;reconcile.put('actor:'..key,function()restoreGuestActor(key)end)
  end
  reconcilePresentation(snapshot)
  if #plan.ops>0 then reconcile.put('notify',function()replay.run(function()gi:GI_NotifyWorldStateChanged()end)end)end
  for n in pairs(plan.names)do local name=n;reconcile.put('quest:'..name,function()replay.run(function()gi:GI_UpdateQuestState(FName(name))end)end)end
 end
 overlay=C.Perf.measure(overlay,'world.applySnapshot')
 local function dirty()if C.Role=='host'then changeVersion=changeVersion+1 end end
 local function install()
  if hooked then return end;hooked=true
  local function hook(path,fn,post)
   assert(valid(StaticFindObject(path)),'Missing world function '..path)
   local a,b;if post and path:match('^/Script/')then a,b=RegisterHook(path,function()end,fn)else a,b=RegisterHook(path,fn)end;hooks[#hooks+1]={path,a,b}
  end
  for _,name in ipairs({'GI_NotifyWorldStateChanged','GI_SaveGameToDisk','GI_NotifyAreaChanged'})do
   hook('/Game/Blueprints/FS_GameInstance.FS_GameInstance_C:'..name,dirty,true)
  end
  -- Native busy-lever messages can be empty and leave ReadTextUI consuming
  -- camera/attack input. A moving shared elevator simply ignores another pull.
  resolve('/Game/Blueprints/Elevator/Elevator_Base.Elevator_Base_C')
  hook('/Game/Blueprints/Elevator/Elevator_Base.Elevator_Base_C:StartElevatorTowardsEnd',function(ctx)if M.connected or joined then elevators.moving(ctx:get(),true)end end,true)
  hook('/Game/Blueprints/Elevator/Elevator_Base.Elevator_Base_C:StoppedMoving',function(ctx)elevators.moving(ctx:get(),false)end,true)
  hook('/Game/Blueprints/FS_GameInstance.FS_GameInstance_C:ForceLoadNewSlot',function(ctx)
   if not pending or C.Role~='client'then return end
   gi=ctx:get();local s=gi.ActiveSaveSlot;assert(valid(s),'Selected guest save unavailable')
   baseline=clone(s);joined=true;pending=false
   overlay(s,latest,true);spawnPending=true
   -- Existing character is initialized normally, but location/intro belong to host.
   s.IsSuspendSave=false;s.SaveLocation=transform(latest.pos,latest.yaw);s.SuspendLocation=s.SaveLocation
   log('WORLD guest character selected; host world overlay ready')
  end,true)
  local function saveGuard(save)
   if joined and valid(baseline)and valid(save:get())and save:get():IsA(baseline:GetClass())then local copy=personalSave(save:get());save:set(copy);return copy end
   return false
  end
  -- UE4SS post-hook arguments expose the original save parameter, not the
  -- substituted object. Pair synchronous calls by nesting instead of identity.
  hook('/Script/Engine.GameplayStatics:SaveGameToSlot',function(_,s,slot)
   syncCopies[#syncCopies+1]={copy=saveGuard(s),slot=slot:get():ToString():gsub('\\','/')}
  end)
  hook('/Script/Engine.GameplayStatics:SaveGameToSlot',function(_,result)
   local saved=table.remove(syncCopies)
   if not saved then return end
   if saved.copy then unroot(saved.copy) end
   local success=result
   if type(result)~='boolean' and result~=nil then success=result:get() end
   checkpoint.savedSlot(saved.slot,success==true)
  end,true)
  hook('/Script/FB_SaveUtils.FB_AsyncSaveSystem:FB_AsyncSaveGameToSlot',function(_,s)local copy=saveGuard(s);if copy then asyncCopies[#asyncCopies+1]=copy end end)
  hook('/Game/Blueprints/FS_GameInstance.FS_GameInstance_C:AsyncSuspendSaveFinished',function()for _,s in ipairs(asyncCopies)do unroot(s)end;asyncCopies={}end,true)
  hook('/Game/Blueprints/SaveSystem/SavableActor.SavableActor_C:Save Data Index to World Save',function(ctx,index,data)
   local a=ctx:get();dirty()
   if classifyPickup(a,false)then
    if joined and valid(baseline)and not replay.active then
     local ids={};a.GUIDs:ForEach(function(_,g)ids[guid(g:get())]=true end);copyPickups(gi.ActiveSaveSlot,baseline,ids)
    end
    return
   end
   if not joined or C.Role~='client'or replay.active or not applied or spawnPending then return end
   local i=index:get();local v=data:get();if not valid(a)or i<0 or i>=#a.GUIDs then return end
   if Books.hostOnly(a)or a:IsA(StaticFindObject('/Game/Blueprints/BreakableBarricade/BreakableBarricade.BreakableBarricade_C'))then return end
   local k=guid(a.GUIDs[i+1]);write('out.events','WC|'..applied.session..'|'..k..'|'..i..'|'..v..'\n',true);stats.requests=stats.requests+1
  end,true)
  hook('/Game/Blueprints/BreakableBarricade/BreakableBarricade.BreakableBarricade_C:TryApplyDamage',function(ctx,hit,damage,applier)
   if not joined or C.Role~='client'or replay.active or spawnPending or not applied then return end
   local a=ctx:get();if not valid(a)or #a.GUIDs==0 or full(applier:get())~=full(gs:GetPlayerPawn(gi,0))then return end
   local amount=damage:get().DamageAmount;if not W.number(amount,0.001,10000)then return end
   write('out.events','WB|'..applied.session..'|'..guid(a.GUIDs[1])..'|'..amount..'\n',true);stats.requests=stats.requests+1
  end,true)
  hook('/Game/Player/FS_PlayerChar.FS_PlayerChar_C:FinishTeleport',function(ctx)
   if full(ctx:get())~=full(gs:GetPlayerPawn(gi,0))then return end
   if C.Role=='client'and shopRejoinPending then localShopTravel=false;shopRejoinPending=false end
   if C.Role=='host'then travel=travel+1 elseif joined and not localShopTravel then spawnPending=true;lastAppliedStreams=''end
  end,true)
  for _,name in ipairs({'TeleportToShop','TeleportBackFromShop'})do
   hook('/Game/Player/FS_PlayerChar.FS_PlayerChar_C:'..name,function(ctx)
    if C.Role=='client'and joined and full(ctx:get())==full(gs:GetPlayerPawn(gi,0))then
     -- A delayed first-shop callback must not discard an already queued host
     -- arrival. Let the original teleport finish, then resume that arrival.
     shopRejoinPending=shopRejoinPending or spawnPending==true
     localShopTravel=true;log('WORLD native shop travel started; host arrival pending='..tostring(shopRejoinPending))
    end
   end,true)
  end
  hook('/Game/UI/Menus/SaveMenu.SaveMenu_C:Construct',function(ctx)
   if C.Role=='client'and joined then local ui=ctx:get();ui.DisableShopButton=true;ui.ShopButtonBox:SetVisibility(1)end
  end,true)
  for _,entry in ipairs({{'Activate',1},{'Deactivate',-1}})do
   local delta=entry[2]
   hook('/Game/Blueprints/Door_Base.Door_Base_C:'..entry[1],function(ctx,byLoad,source)
    dirty()
    if not joined or replay.active or spawnPending or not applied or byLoad:get()then return end
    local a,t=ctx:get(),source:get();if not valid(a)or not valid(t)or #a.GUIDs==0 then return end
    local lever=t:IsA(StaticFindObject('/Game/LD45/Blueprints/Objects/Gate/Lever.Lever_C'))
    local trigger=t:IsA(StaticFindObject('/Game/Blueprints/ActivationTrigger.ActivationTrigger_C'))
    local transient=(lever and t.DisableSaving)or(trigger and(not t.OnlyOnce or t.DisableSaveOnlyOnce))
    if transient and #t.GUIDs>0 then write('out.events','DC|'..applied.session..'|'..guid(a.GUIDs[1])..'|'..guid(t.GUIDs[1])..'|'..delta..'\n',true);stats.requests=stats.requests+1 end
   end,true)
  end
 end
 snapshot=function(player,w,job)
  if not valid(player)or not valid(gi)then return true end
  local manager=FindFirstOf('FS_LevelManager')
  if player.Teleporting or player.QuickTeleporting or player.DoingTeleportCrystal or(valid(manager)and manager:IsProcessingLevelChanges(gi))then return true end
  local save=gi.ActiveSaveSlot
  if job.phase==0 then job.change=changeVersion;job.travel=travel;job.mapRevision=mapRevision;job.slot=full(save);job.lines={};job.fields={};for name in pairs(W.fields)do job.fields[#job.fields+1]=name end;table.sort(job.fields);job.index=1;job.phase=1 end
  if not valid(save)or full(save)~=job.slot or job.change~=changeVersion or job.travel~=travel or job.mapRevision~=mapRevision then stats.abandoned=(stats.abandoned or 0)+1;return true end
  local lines=job.lines;local function add(...)lines[#lines+1]=table.concat({...},'|')end
  if job.phase==1 then
   local n=job.fields[job.index]
   if n then local t=W.fields[n];local v=save[n]
    if #t==1 then local x=t=='o'and asset(v)or(t=='n'and v:ToString()or(t=='b'and(v and '1'or'0')or tostring(v)));add('F',n,W.escape(x))
    else add('C',n);local rows={}
     if t=='ao'then v:ForEach(function(_,e)rows[#rows+1]='E|'..n..'|'..W.escape(asset(e:get()))end)
     else v:ForEach(function(k,e)local key=t:sub(1,1)=='g'and guid(k:get())or k:get():ToString();if(n=='NamedSaveData'and W.personalName(key))or(pickupMaps[n]and pickupGUIDs[key])then return end;local x=t:sub(2)=='o'and asset(e:get())or tostring(e:get());rows[#rows+1]='E|'..n..'|'..W.escape(key)..'|'..W.escape(x)end)end
     table.sort(rows);for _,r in ipairs(rows)do lines[#lines+1]=r end
    end
    job.index=job.index+1;return false
   end
   job.phase=2;return false
  elseif job.phase==2 then
   local lm=FindFirstOf('FS_LevelManager');if valid(lm)then for i,n in ipairs({'LevelListStack','TriggerLevelListStack'})do lm[n]:ForEach(function(_,e)local v=e:get();local p=asset(v.List);if p~=''then add('L',i,p,v.Priority)end end)end end
   job.phase=3;return false
  elseif job.phase==3 then
   local doors,seen={},{};for _,a in ipairs(actors('/Game/Blueprints/Door_Base.Door_Base_C'))do if valid(a)and #a.GUIDs>0 then local k=guid(a.GUIDs[1]);if not seen[k]then doors[#doors+1]='D|'..k..'|'..(shrineGates.open(a,gi.ActiveSaveSlot) and 1 or 0)..'|'..a.CurrentActivationCount;seen[k]=true end end end;table.sort(doors);for _,d in ipairs(doors)do lines[#lines+1]=d end
   job.phase=4;return false
  elseif job.phase==4 then
   local levers={};for _,a in ipairs(actors('/Game/LD45/Blueprints/Objects/Gate/Lever.Lever_C'))do if #a.GUIDs>0 then levers[#levers+1]='J|'..guid(a.GUIDs[1])..'|'..(a.ToggledOn and 1 or 0)end end;table.sort(levers);local previous;for _,l in ipairs(levers)do if l~=previous then lines[#lines+1]=l;previous=l end end
   job.phase=5;return false
  end
  if job.phase==5 then
   job.gateIndex=(job.gateIndex or 0)+1;Gates.capture(player,add,job.gateIndex)
   if job.gateIndex>=Gates.count then job.phase=6 end
   return false
  end
  local body='R|'..mapRevision..'\nW|'..w..'\nMU|'..Music.capture(gi)..'\n'..table.concat(lines,'\n')..'\n'
  local pos=player:K2_GetActorLocation();if lastPos and(pos.X-lastPos.X)^2+(pos.Y-lastPos.Y)^2+(pos.Z-lastPos.Z)^2>2500^2 then travel=travel+1 end;lastPos={X=pos.X,Y=pos.Y,Z=pos.Z}
  if body~=lastBody or travel~=M.lastTravel or read('out.world')==''then
   revision=revision+1;local raw=string.format('QDW1|%s|%d|%d\nP|%.3f|%.3f|%.3f|%.3f\n',token,revision,travel,pos.X,pos.Y,pos.Z,player:K2_GetActorRotation().Yaw)..body..'END\n'
   if not W.decode(raw)then write('world-invalid.txt',raw);error('World snapshot failed schema validation')end
   write('out.world',raw);lastBody=body;M.lastTravel=travel
  end
  return true
 end
 snapshot=C.Perf.measure(snapshot,'world.buildSnapshotSlice')
 function M.prepareJoin()
  local s=W.decode(read('in.world'));if not s then return false,'receiving_world'end
  latest=s;pending=true;gi=FindFirstOf('FS_GameInstance_C');install();return true
 end
 function M.poll(player,w,connected,frame)
  M.connected=connected;Travel.poll(connected)
  gi=FindFirstOf('FS_GameInstance_C');if not valid(gi)then return false end
  tickCount=tickCount+1
  if (C.Role=='host'or(C.Role=='client'and joined and not spawnPending))and tickCount%10==0 then StoryRewards.ensure(gi,player)end
  if C.Role=='host'and valid(player)then
   install();if token==''then room=room+1;token=C.RunId..'_'..room end
   -- While the guest chooses/loads its character, keep the arrival position
   -- fresh even if the host only walks and changes no saved world properties.
   if connected and not frame then lastBody=''end
   if tickCount%10==0 and not snapshotJob then snapshotJob={player=player,world=w,phase=0}end
   if tickCount%100==1 then refreshActors(true)end
  elseif C.Role=='client'and joined and valid(player)then
   if not connected then
    disconnectAt=disconnectAt or os.time()
    if os.time()-disconnectAt>=15 then if not player.IsDead then gi:GI_WriteSuspendSaveImmediately(true)end;gs:OpenLevel(gi,FName('/Game/LD45/Maps/title_screen'),true,'');disconnectAt=nil;return true end
   else disconnectAt=nil end
   -- Native travel owns its streaming list, temporary sky-room position and
   -- movement lock until FinishTeleport. Applying a host snapshot in the
   -- middle can unload the destination underneath its latent callbacks.
   -- Read the newest host snapshot after completion; do not consume its travel
   -- revision early or replace native movement mode with our arrival lock.
   if player.Teleporting or player.QuickTeleporting or player.DoingTeleportCrystal then return false end
   if tickCount%100==1 or next(actorIndex)==nil then refreshActors(true)end
   local raw=read('in.world');if raw~=''and raw~=lastRaw then local s=W.decode(raw);if s and (not applied or s.session~=applied.session or s.revision>applied.revision)then latest=s end;lastRaw=raw end
   local reloadNeeded=latest and applied and (latest.mapRevision~=applied.mapRevision or latest.session~=applied.session)
   if rollback and latest and latest.session==rollback.session and latest.mapRevision>rollback.mapRevision then
    unroot(baseline)
    rollbackListener(rollback.save)
    baseline=clone(rollback.save)
    setSave(rollback.save)
    unroot(rollback.save)
    rollback=nil
    log('WORLD restored guest checkpoint character')
   end
   if latest and (not applied or latest.session~=applied.session or latest.revision>applied.revision)then
    local moved=applied and latest.travel~=applied.travel
    if moved then localShopTravel=false;shopRejoinPending=false;spawnPending=true end
    overlay(gi.ActiveSaveSlot,latest,false)
   end
   if latest and (latest.world~=w or reloadNeeded)then
    overlay(gi.ActiveSaveSlot,latest,true);spawnPending=true;gs:OpenLevel(gi,FName(latest.world:match('^[^.]+')),true,'');stats.travels=stats.travels+1;return true
   end
   local lm=FindFirstOf('FS_LevelManager')
   if not localShopTravel and latest and tickCount%10==0 and valid(lm)and not lm:IsProcessingLevelChanges(gi)then
    local observed={{},{}};for i,n in ipairs({'LevelListStack','TriggerLevelListStack'})do lm[n]:ForEach(function(_,e)local v=e:get();local path=asset(v.List);if path~=''then observed[i][#observed[i]+1]={path=path,priority=v.Priority}end end)end
    if streamsKey(observed)~=streamsKey(latest.streams)then lastAppliedStreams=''end
   end
   if not localShopTravel and latest and lastAppliedStreams~=streamsKey(latest.streams)then
    local q=player:K2_GetActorLocation();local p=frame and frame.P or nil
    if p and (q.X-p.x)^2+(q.Y-p.y)^2+(q.Z-p.z)^2>2500^2 then spawnPending=true end
    applyStreams(latest)
   end
   if spawnPending and latest and connected then
    if pausedMovement==nil then pausedMovement=player.CharacterMovement.MovementMode;player.CharacterMovement:DisableMovement()end
    local lm=FindFirstOf('FS_LevelManager')
    if valid(lm)and not lm:IsProcessingLevelChanges(gi)then
     -- Position and streaming lists must come from the same stable snapshot.
     -- The faster motion channel can already be inside the next teleport's
     -- temporary sky room while this destination is still being loaded.
     local pos,yaw=latest.pos,latest.yaw
     player:K2_SetActorLocationAndRotation(pos,{Pitch=0,Yaw=yaw,Roll=0},false,{},true);player.CharacterMovement:StopMovementImmediately()
     player:GetController():SetControlRotation({Pitch=0,Yaw=yaw,Roll=0});refreshActors();overlay(gi.ActiveSaveSlot,latest,false)
     player.CharacterMovement:SetMovementMode(pausedMovement~=0 and pausedMovement or 1,0);pausedMovement=nil;spawnPending=false;stats.travels=stats.travels+1;log(string.format('WORLD guest arrived in host area travel=%d pos=%.1f,%.1f,%.1f',latest.travel,pos.X,pos.Y,pos.Z))
    end
   end
  end
  if C.Role=='client'and joined and connected and latest and not spawnPending and not localShopTravel and tickCount%10==0 then Music.apply(gi,latest.music or latest.fields.PlayingMusicTrack)end
  if tickCount%10==0 then write('world-status.txt',string.format('role=%s joined=%s revision=%d applied=%d snapshots=%d restored=%d requests=%d accepted=%d saves=%d travels=%d pending=%s reconcile=%d reconcileFailures=%d fieldWrites=%d abandoned=%d overlayFault=%s\n',C.Role,tostring(joined==true),revision,applied and applied.revision or 0,stats.snapshots,stats.restored,stats.requests,stats.accepted,stats.saves,stats.travels,tostring(spawnPending==true),reconcile.count,reconcile.failures,stats.fieldWrites or 0,stats.abandoned or 0,tostring(overlayFault)))end
  return false
 end
 function M.event(f,w,peer)
  if C.Role~='host'or(f[2]~='WC'and f[2]~='WB'and f[2]~='DC')or f[3]~=token or not peer then return end
  if #f~=(f[2]=='WB'and 5 or 6)or not W.guid(f[4]or'')then return end
  local b=actorIndex[f[4]];local target=live(b);if not valid(target)then return f[2]=='WC'and'retry'or nil end
  if isPickup(target)or Books.hostOnly(target)then return end
  if f[2]=='DC'then
   local source=live(actorIndex[f[5]]);local delta=W.integer(f[6],-1,1);if #f~=6 or not delta or delta==0 or not valid(source)then return end
   local pos=source:K2_GetActorLocation();if(pos.X-peer.X)^2+(pos.Y-peer.Y)^2+(pos.Z-peer.Z)^2>1200^2 then return end
   local a=target;if not a:IsA(StaticFindObject('/Game/Blueprints/Door_Base.Door_Base_C'))then return end
   local lever=source:IsA(StaticFindObject('/Game/LD45/Blueprints/Objects/Gate/Lever.Lever_C'))
   local trigger=source:IsA(StaticFindObject('/Game/Blueprints/ActivationTrigger.ActivationTrigger_C'))
   if not((lever and source.DisableSaving)or(trigger and(not source.OnlyOnce or source.DisableSaveOnlyOnce)))then return end
   replay.run(function()
    if delta==1 then a:Activate(false,source)else a:Deactivate(false,source)end
    if lever and not source.ToggledOn then local before=Interaction.before(source);source.ToggledOn=true;source['Restore to switched'](source);Interaction.after(source,before)end
   end);dirty();lastBody='';stats.accepted=stats.accepted+1;return
  end
  local a=target;local p=a:K2_GetActorLocation()
  local barrier=a:IsA(StaticFindObject('/Game/Blueprints/BreakableBarricade/BreakableBarricade.BreakableBarricade_C'))
  local lift=elevators.is(a)
  if lift then if not elevators.near(a,peer)then return end
  elseif(p.X-peer.X)^2+(p.Y-peer.Y)^2+(p.Z-peer.Z)^2>(barrier and 12000 or 1200)^2 then return end
  local n,v,amount
  if f[2]=='WB'then amount=W.number(f[5],0.001,10000);if #f~=5 or not barrier or not amount then return end
  else n=W.integer(f[5],0,32);v=W.integer(f[6],-100000,100000);if #f~=6 or n~=b.index or not v or barrier then return end end
  if lift and not elevators.accept(a,n,v)then return end
  if not barrier then
   local ok,err=pcall(replay.run,function()a['Save Data Index to World Save'](a,n,v)end)
   if not ok then
    local key=W.guid(f[4]);local saved=gi.ActiveSaveSlot.SavableActorStates
    if not saved:Contains(key)or saved:Find(key):get()~=v then return 'retry' end
    log('WORLD recovered state write after error '..tostring(err))
   end
   local key=f[4];reconcile.put('hostactor:'..key,function()replay.run(function()local actor=live(actorIndex[key]);if valid(actor)then restore(actor)end end)end)
   dirty();lastBody='';stats.accepted=stats.accepted+1;return
  end
  replay.run(function()
  if barrier then
   if a.HP>0 then a:TryApplyDamage({},{DamageAmount=amount},gs:GetPlayerPawn(gi,0),{},p,true)end
  end
  end);dirty();lastBody='';stats.accepted=stats.accepted+1
 end
 function M.resetMap()Interaction.reset();Music.reset(false);Books.reset();Travel.reset();localShopTravel=false;shopRejoinPending=false;reconcile.clear();overlayFault=false;changeVersion=changeVersion+1;snapshotJob=nil;actorRefresh=nil;actorIndex={};lastAppliedStreams='';pausedMovement=nil;mapRevision=mapRevision+1;if joined then spawnPending=true end end
 function M.checkpointReady(player)
  if overlayFault then return false end
  if C.Role=='client'and latest and(not applied or latest.session~=applied.session or latest.revision>applied.revision)then return false end
  if C.Role~='host'and(not joined or spawnPending)then return false end
  if player.IsDead or player.Teleporting or player.QuickTeleporting or player.DoingTeleportCrystal then return false end
  local manager=FindFirstOf('FS_LevelManager')
  return valid(manager)and not manager:IsProcessingLevelChanges(gi)
 end
 function M.stageRollback(save)
  if not joined or not applied or not valid(save)then return false end
  if rollback then unroot(rollback.save)end
  root(save)
  rollback={save=save,session=applied.session,mapRevision=applied.mapRevision}
  return true
 end
 function M.cancelRollback()
  if rollback then unroot(rollback.save);rollback=nil end
 end
 function M.rollbackHost(save)
  if not valid(save)then return false end
  local level=save.SaveLevel:ToString()
  if not level:match('^[%w_/%.-]+$') or level=='None'then return false end
  rollbackListener(save)
  setSave(save)
  gs:OpenLevel(gi,FName(level),true,'')
  return true
 end
 function M.ended()
  Interaction.reset()
  Music.reset(true)
  Books.reset();Travel.reset();localShopTravel=false;shopRejoinPending=false
  M.connected=false;elevators.reset();reconcile.clear();overlayFault=false;changeVersion=changeVersion+1
  snapshotJob=nil;actorRefresh=nil
  M.cancelRollback()
  -- Keep the last shared world in the selected character after leaving, too.
  unroot(baseline);unroot(diskCopy);baseline=nil;diskCopy=nil;joined=false;pending=false;latest=nil;applied=nil;lastRaw=nil;spawnPending=false;token='';lastBody='';lastPos=nil;lastAppliedStreams='';actorIndex={};write('out.world','')
 end
 function M.storyChanged()dirty();lastBody='';travel=travel+1;snapshotJob=nil end
 function M.inspect()return{joined=joined,baseline=baseline,copy=diskCopy,applied=applied,stats=stats,token=token,replaying=replay.active,reconcile=reconcile.count,overlayFault=overlayFault}end
 return M
end
