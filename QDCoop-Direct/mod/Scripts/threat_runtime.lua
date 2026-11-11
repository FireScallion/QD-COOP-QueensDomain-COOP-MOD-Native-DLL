return function(dir,C,valid,full,write,log,records,byActor,resolveSource)
 local policy=dofile(dir..'threat_policy.lua');local Interest=dofile(dir..'enemy_interest.lua');local DP=dofile(dir..'damage_protocol.lua');local codec=dofile(dir..'damage_codec.lua')(DP)
 local gs=C.ObjectRefs.find('/Script/Engine.Default__GameplayStatics');local ks=C.ObjectRefs.find('/Script/Engine.Default__KismetSystemLibrary')
 local maths=C.ObjectRefs.find('/Script/Engine.Default__KismetMathLibrary')
 local T={sent=0,received=0,switches=0,hitSerial=0,seen={},unknown=0};local clsPath='/Game/QDCoop/QDCoopGuestTarget.QDCoopGuestTarget_C'
 local function same(a,b)return valid(a)and valid(b)and a:GetAddress()==b:GetAddress()end
 local function point(a)return a:K2_GetActorLocation()end
 local function distance(a,b)return math.sqrt((a.X-b.X)^2+(a.Y-b.Y)^2+(a.Z-b.Z)^2)end
 local function state(r)r.threat=r.threat or policy.new();return r.threat end
 local function emit(s)write('out.events','THT|'..T.world..'|'..T.session..'|'..s..'\n',true)end
 local function loadClass()
  local c=C.ObjectRefs.find(clsPath)
  if not valid(c)then local path=ks:MakeSoftClassPath(clsPath);local ref=ks:Conv_SoftClassPathToSoftClassRef(path);c=ks:LoadClassAsset_Blocking(ref)end
  assert(valid(c),'Guest target asset missing; launch both games from the complete 0.10 package')
  local gi=FindFirstOf('FS_GameInstance_C');if valid(gi)then local refs,found={},false;gi.ReferencedObjects:ForEach(function(i,v)local o=v:get();refs[#refs+1]=o;if same(o,c)then found=true end end);if not found then refs[#refs+1]=c;gi.ReferencedObjects=refs end end
  return c
 end
 function T.scratch()
  loadClass();return C.ObjectRefs.find('/Game/QDCoop/QDCoopGuestTarget.Default__QDCoopGuestTarget_C')
 end
 function T.isGuest(a)return same(a,T.proxy)end
 function T.target(e)
  local r=byActor()[full(e)];local s=r and r.threat
  if T.connected and C.Role=='host'and s and r.syncActive~=false then if valid(r.memoryActor)then return r.memoryActor end;return s.target=='client'and valid(T.proxy)and T.proxy or T.player end
 end
 local function perception(e,r)
  local s=r and r.threat;local c=s and r.threatCandidates and r.threatCandidates[s.target]
  if not T.connected or C.Role~='host'or not s or r.syncActive==false then return end
  local seen=c and c.alive and c.inRange and c.los or false
  local los=e.EnemyLineOfSight;local range=c and c.distance or 100000
  e.SeenPlayerRange=range;e.bPlayerIsSeen=seen;e.HasPlayerlos=seen
  los.SeenRange=range;los.bCanSeeTarget=seen;los.bHasLOS=seen;los.bTargetIsInRange=c and c.inRange or false
  return seen
 end
 function T.record(r,id,amount)
  if not T.connected or C.Role~='host'or not r then return end
  r.syncActive=true;r.wakeUntil=gs:GetRealTimeSeconds(r.e)+4;policy.hit(state(r),id,amount,gs:GetRealTimeSeconds(r.e));r.threatTotals=r.threatTotals or{host=0,client=0};r.threatTotals[id]=r.threatTotals[id]+math.max(0,amount or 0)
 end
 function T.capture(d)return codec.capture(d)end
 function T.build(d)return codec.build(T.scratch(),d)end
 function T.decode(...)return DP.decode(...)end
 function T.install(hookIds)
  if T.installed then return end;T.installed=true
  if C.Role~='host'then return end
  loadClass()
  local function hook(path,fn,post)
   local function active(f)if not f then return nil end;return function(...)if not T.connected or C.Role~='host' then return end;return f(...)end end
   local label='hook.threat.'..path:match(':([^:]+)$')
   local a,b=RegisterHook(path,C.Perf.hook(active(fn),label),post and C.Perf.hook(active(post),label..'.post')or nil);hookIds[#hookIds+1]={path,a,b}
  end
  -- EnemyBase pays kill XP to TargetPlayer, which may now be our inert guest
  -- target. The guest already receives its own ER reward; preserve the host's
  -- native award here instead of letting the proxy swallow it. Do not turn
  -- XP-steal attacks into an extra party kill reward.
  hook(clsPath..':AddXP',function(ctx,amount,fromSteal)
   if not T.isGuest(ctx:get())or fromSteal:get()or not valid(T.player)then return end
   local xp=amount:get();if xp<=0 then return end
   T.player:AddXP(xp,false)
   T.hostXPRecovered=(T.hostXPRecovered or 0)+xp
   write('threat-xp-status.txt','hostKillXPRecovered='..T.hostXPRecovered..'\n')
  end)
  -- The original BP supplies GetPlayerPawn(0) here even after TargetPlayer changes.
  hook('/Script/QueensDomain.FS_EnemyCharacterBase:DoBasicCharacterRotationUpdate',function(ctx,a,b,c,d,e,f,target)
   local t=T.target(ctx:get());if valid(t)then target:set(t)end
  end)
  hook('/Game/Enemies/EnemyBase.EnemyBase_C:UpdateCanSeePlayer',function(ctx)local e=ctx:get();perception(e,byActor()[full(e)])end)
  hook('/Game/Enemies/EnemyBase.EnemyBase_C:EvaluateAwarenessCones',function(ctx,sensitivity,saw)
   local e=ctx:get();local r=byActor()[full(e)];local seen=perception(e,r);if seen~=nil then saw:set(seen);if not seen then sensitivity:set(0)end end
  end)
  hook('/Game/Enemies/EnemyBase.EnemyBase_C:GetChaseTarget',function(ctx,target)local r=byActor()[full(ctx:get())];if T.connected and C.Role=='host'and r and valid(r.memoryActor)then target:set(r.memoryActor)end end)
  hook('/Game/Enemies/EnemyBase.EnemyBase_C:GetLastSeenPlayerLoc',function(ctx,loc)local r=byActor()[full(ctx:get())];if T.connected and C.Role=='host'and r and valid(r.memoryActor)then local p=point(r.memoryActor);local out=loc:get();out.X=p.X;out.Y=p.Y;out.Z=p.Z end end)
  hook('/Script/QueensDomain.FS_EnemyCharacterBase:GetDistanceToPlayerForAI',function()end,function(ctx,d3,d2)
   local e=ctx:get();local r=byActor()[full(e)];if T.connected and C.Role=='host'and r and valid(r.memoryActor)then local a,b=point(e),point(r.memoryActor);d3:set(distance(a,b));d2:set(math.sqrt((a.X-b.X)^2+(a.Y-b.Y)^2))end
  end)
  hook(clsPath..':TryApplyDamage',function(ctx,hit,damage,applier,applied,pos,hitValid)
   if C.Role~='host'or not T.connected or not T.isGuest(ctx:get())or not T.peer or not T.peerAlive then return false end
   -- Native attacks may supply their projectile/component/action as applier.
   -- Resolve the owning enemy using the same bounded chain as effects/audio.
   local source=applier:get();local r=valid(source)and(resolveSource and resolveSource(source)or byActor()[full(source)])
   if not r or r.retired or not valid(r.e)then T.unknown=T.unknown+1;return false end
   local d,encoded=codec.capture(damage:get());if not encoded or d.amount<=0 then return false end
   T.hitSerial=T.hitSerial+1;local p=pos:get()
   emit(string.format('D|%s|%d|%d|%s|%.4f|%.4f|%.4f|%d',T.peer,r.id,T.hitSerial,encoded,p.X,p.Y,p.Z,hitValid:get()and 1 or 0));T.sent=T.sent+1
   return true
  end)
 end
 function T.reset(travel)
  if not travel then
   for _,r in pairs(records())do
    if valid(r.e)then
     if valid(T.player)and T.isGuest(r.e.TargetPlayer)then r.e.TargetPlayer=T.player;local c=r.e:GetController();if valid(c)then c:StopMovement()end end
     if r.losOriginal then r.e.EnemyLineOfSight.bEnabled=r.losOriginal.enabled;r.e.EnemyLineOfSight:SetComponentTickEnabled(r.losOriginal.tick)end
    end
    if valid(r.memoryActor)then r.memoryActor:K2_DestroyActor()end;r.threat=nil;r.memoryActor=nil;r.losOriginal=nil
   end
   if valid(T.proxy)then local g=gs:GetGameState(T.proxy);if valid(g)and g:IsA(C.ObjectRefs.find('/Script/FBAttributes.FB_GameState'))then g:UnregisterDamageableActor(T.proxy)end;T.proxy:K2_DestroyActor()end
  end
  T.proxy=nil;T.proxyAlive=false;T.peer=nil;T.connected=false;T.seen={};T.seenOrder={};T.lastDamage={};T.nextEvaluate=0;T.evaluateQueue=nil;T.evaluateIndex=1;T.proxyAt=nil
 end
 function T.uninstall()T.installed=false end
 local function proxy(player,p)
  local t=player:GetTransform();t.Translation={X=p.X,Y=p.Y,Z=p.Z}
  local a=gs:BeginDeferredActorSpawnFromClass(player,loadClass(),t,1,nil);assert(valid(a),'Guest collision proxy spawn failed')
  a:SetActorHiddenInGame(true);gs:FinishSpawningActor(a,t);a:SetActorTickEnabled(false);a.CharacterMovement:DisableMovement();a.CharacterMovement:SetComponentTickEnabled(false)
  a.Mesh:SetCollisionEnabled(0);a.Mesh:SetComponentTickEnabled(false)
  a.CapsuleComponent:SetCollisionEnabled(1);a.CapsuleComponent:SetCollisionResponseToChannel(2,1);a.CapsuleComponent:SetCollisionResponseToChannel(3,0);a.CapsuleComponent:SetCollisionResponseToChannel(4,0)
  a.Health=p.hp;a.IsDead=p.hp<=0;local g=gs:GetGameState(player);if valid(g)and g:IsA(C.ObjectRefs.find('/Script/FBAttributes.FB_GameState'))then g:RegisterDamageableActor(a)end
  log('THREAT guest proxy ready '..full(a));return a
 end
 local function candidate(e,a,id,r,now,sight)
  if not valid(a)or a.IsDead or a.Health<=0 then return {alive=false}end
  if not sight.position then sight.position=point(e);sight.forward=e:GetAIForwardVector()end
  local ep,ap=sight.position,point(a);local dist=distance(ep,ap);local s=state(r);local recent=policy.recent(s,id,now)
  -- Idle detection range is not a combat leash. Short-sighted enemies must
  -- still pursue an observed ranged attacker, without acquiring idle bystanders.
  local range=policy.range(r.awarenessRange,s.target==id or recent>0)
  local inRange=false
  if dist<=range then
   if sight.height==nil then sight.height=e:GetAggroHeight()end
   inRange=math.abs(ep.Z-ap.Z)<=sight.height
  end
   local f=sight.forward;local dx,dy=ap.X-ep.X,ap.Y-ep.Y;local len=math.sqrt(dx*dx+dy*dy);local front=len<1 or (dx*f.X+dy*f.Y)/len>=policy.frontCos
  local los=false
  if inRange then
   if not sight.viewpoint then sight.viewpoint=maths:TransformLocation(e.CapsuleComponent:K2_GetComponentToWorld(),e.LocalViewpoint.Translation)end
   los=not ks:LineTraceSingle(e,sight.viewpoint,ap,0,true,{e,T.player,T.proxy},0,{},true,{},{},0)
  end
  if los and(front or s.target==id or recent>0)then r.lastVisible=r.lastVisible or{};r.lastVisible[id]={X=ap.X,Y=ap.Y,Z=ap.Z};s.seen[id]=now end
  return {alive=true,inRange=inRange,distance=dist,front=front,los=los,visible=front and los}
 end
 function T.update(player,world,session,now,connected,peer,p)
  T.player=player;T.world=world;T.session=session;T.connected=connected
  if C.Role~='host'or not connected then return end
  if T.peer and T.peer~=peer then T.reset(false);T.connected=connected end;T.peer=peer
  T.peerAlive=p and p.hp and p.hp>0 and p.at and now-p.at<8 and peer~=nil
  if T.peerAlive then
   if not valid(T.proxy)then T.proxy=proxy(player,p)end
   if not T.proxyAlive then T.proxy:SetActorEnableCollision(true);T.proxyAlive=true end
   if T.proxyAt~=p.at then T.proxy.Health=p.hp;T.proxy.IsDead=false; T.proxy:K2_SetActorLocationAndRotation({X=p.X,Y=p.Y,Z=p.Z},{Pitch=0,Yaw=p.yaw or 0,Roll=0},false,{},true);T.proxyAt=p.at end
  elseif T.proxyAlive and valid(T.proxy)then T.proxy.Health=0;T.proxy.IsDead=true;T.proxy:SetActorEnableCollision(false);T.proxyAlive=false end
  -- Distribute sight traces across frames instead of a single 5 Hz burst.
  if (not T.evaluateQueue or T.evaluateIndex>#T.evaluateQueue)and now>=(T.nextEvaluate or 0)then
   T.nextEvaluate=now+.2
   T.evaluateQueue={};T.evaluateIndex=1
   local own=point(player)
   for _,r in pairs(records())do
    -- Cached snapshots permit the interest decision without reflected actor
    -- traversal. This gates EXTRA co-op work, not native actors or wire cadence.
    r.syncActive=Interest.active(r,own,p,now)
    if r.syncActive then T.evaluateQueue[#T.evaluateQueue+1]=r
    elseif r.losOriginal and valid(r.e)then local l=r.e.EnemyLineOfSight;l.bEnabled=r.losOriginal.enabled;l:SetComponentTickEnabled(r.losOriginal.tick);r.losOriginal=nil end
   end
  end
  local processed=0;local workStart=os.clock()
  while T.evaluateQueue and T.evaluateIndex<=#T.evaluateQueue do
   local r=T.evaluateQueue[T.evaluateIndex];T.evaluateIndex=T.evaluateIndex+1
   processed=processed+1
   local e=r.e
   if valid(e)and valid(e.Data)and e.HP>0 and not e.bDead and not e.BestiaryInstance and not e.bSpawnerPreviewInstance then
    if r.flee==nil then r.flee=e:IsA('/Game/Enemies/Lizer_00/Default_Lizer_01.Default_Lizer_01_C')and not e.DefaultEnemyMode end
    r.awarenessRange=e.Data.AwarenessProfile:GetRealMaxRangeForProfile(e.AggroRange)
    if not r.losOriginal then local l=e.EnemyLineOfSight;r.losOriginal={enabled=l.bEnabled,tick=l:IsComponentTickEnabled()};l.bEnabled=false;l:SetComponentTickEnabled(false)end
    if not r.threatInitialized and(e.bIsAttacking or e.bInRecoveryTime)then local s=state(r);s.target=T.isGuest(e.TargetPlayer)and'client'or'host';s.since=now end
    r.threatInitialized=true
    local s=state(r);local sight={}
    local candidates={host=candidate(e,player,'host',r,now,sight),client=T.peerAlive and candidate(e,T.proxy,'client',r,now,sight)or{alive=false}}
    r.threatCandidates=candidates
    local previous,previousSince=s.target,s.since
    local target,changed=policy.choose(s,candidates,now,e.bIsAttacking or e.bInRecoveryTime,e.Data.HP)
    -- A frightened treasure lizard runs AWAY from its target. Preserve its
    -- native fear decay; a chase-memory timeout must not send it back home.
    if r.flee and not target and e.bAggrod and previous and candidates[previous].alive then
     s.target=previous;s.since=previousSince;target=previous;if changed then s.switches=s.switches-1 end;changed=false
    end
    local memory=not r.flee and target and not candidates[target].los and r.lastVisible and r.lastVisible[target]
    if memory and not valid(r.memoryActor)then
     local t=e:GetTransform();t.Translation=memory;local a=gs:BeginDeferredActorSpawnFromClass(e,C.ObjectRefs.find('/Script/Engine.SkeletalMeshActor'),t,1,nil);gs:FinishSpawningActor(a,t);a:SetActorHiddenInGame(true);a:SetActorEnableCollision(false);a:SetActorTickEnabled(false);r.memoryActor=a;local c=e:GetController();if valid(c)and e.bIsChasing then c:StopMovement()end
    elseif not memory and valid(r.memoryActor)then r.memoryActor:K2_DestroyActor();r.memoryActor=nil;local c=e:GetController();if valid(c)and e.bIsChasing then c:StopMovement()end end
    local chosen=target=='client'and T.proxy or player
    if valid(chosen)and not same(e.TargetPlayer,chosen)then
     e.TargetPlayer=chosen
     -- Cancel the old MoveTo once; the original chase-state continuation issues
     -- its next request against GetChaseTarget, which now returns the new pawn.
     local c=e:GetController();if not r.flee and valid(c)and e.bIsChasing then c:StopMovement()end
     e.EnemyLineOfSight:UpdateLOSParams()
    end
    if changed then T.switches=T.switches+1;log('THREAT target '..r.id..'='..tostring(target))end
    perception(e,r)
    if changed and not target and e.bAggrod and not r.flee then e:ForceUnaggro()end
    if not e.bAggrod and target and candidates[target].visible and not e.bIsAIDisabled then e.AggroBuildup=1;e:ForceAggroupdate()end
    if not e.bAggrod and not target then r.threat=nil end
   else r.threat=nil;r.threatTotals=nil;r.threatInitialized=nil;if valid(r.memoryActor)then r.memoryActor:K2_DestroyActor();r.memoryActor=nil end;if valid(e)and r.losOriginal then e.EnemyLineOfSight.bEnabled=r.losOriginal.enabled;e.EnemyLineOfSight:SetComponentTickEnabled(r.losOriginal.tick);r.losOriginal=nil end end
   if processed>=8 or os.clock()-workStart>=.001 then break end
  end
 end
 function T.event(f,world,peer,replicas)
  if C.Role~='client'or not T.connected or f[2]~='THT'or #f~=16 or f[3]~=world or f[4]~=peer or f[5]~='D'or f[6]~=T.session then return end
  local id,serial=tonumber(f[7]),tonumber(f[8]);local d=DP.decode(f[9],f[10],f[11],f[12]);local p={X=tonumber(f[13]),Y=tonumber(f[14]),Z=tonumber(f[15])};local validHit=tonumber(f[16]);local r=id and replicas[id]
  if not r or not valid(r.e)or not d or d.amount<=0 or not serial or serial%1~=0 or serial<1 or serial>1e9 or not p.X or not p.Y or not p.Z or(validHit~=0 and validHit~=1)then return end
  for _,v in pairs(p)do if v~=v or math.abs(v)>1e7 then return end end
  local key=peer..':'..serial;if T.seen[key]then return end;T.seen[key]=true;T.seenOrder=T.seenOrder or{};T.seenOrder[#T.seenOrder+1]=key;if #T.seenOrder>512 then T.seen[table.remove(T.seenOrder,1)]=nil end
  if not valid(T.player)or T.player.IsDead or T.player.Health<=0 then return end
  local before=T.player.Health;local damage=T.build(d)
  T.player:TryApplyDamage({},damage,r.e,{},p,validHit==1);T.received=T.received+1
  write('threat-damage.txt',string.format('enemy=%d serial=%d raw=%.4f before=%.4f after=%.4f\n',id,serial,d.amount,before,T.player.Health))
 end
 function T.status()
  local rows={};for id,r in pairs(records())do local s=r.threat;if s then local t=r.threatTotals or{};rows[#rows+1]=string.format('%d|%s|%.4f|%.4f|%.4f|%.4f|%d|%.4f|%.4f',id,s.target or'none',s.scores and s.scores.host or 0,s.scores and s.scores.client or 0,s.recentDamage and s.recentDamage.host or 0,s.recentDamage and s.recentDamage.client or 0,s.switches,t.host or 0,t.client or 0)end end
  write('threat-bindings.txt',table.concat(rows,'\n'));write('threat-status.txt',string.format('proxy=%s switches=%d sent=%d received=%d unknownSources=%d extraDamageObjects=%d\n',valid(T.proxy)and full(T.proxy)or'none',T.switches,T.sent,T.received,T.unknown,codec.extraObjects))
 end
 T.update=C.Perf.measure(T.update,'enemy.threat')
 return T
end
