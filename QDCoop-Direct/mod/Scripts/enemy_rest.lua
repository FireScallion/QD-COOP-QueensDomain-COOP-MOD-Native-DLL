-- Only the host refreshes authoritative spawners. Native checkpoint UI/healing stay local.
return function(C,M,valid,full,gs,write,log,health)
 local protocol=dofile(debug.getinfo(1,'S').source:gsub('^@',''):match('^(.*[/\\])')..'enemy_protocol.lua')
 local R={serial=0,seen={},generation=0,cleanup={}}
 local spawnerPath='/Game/Enemies/EnemySpawner.EnemySpawner_C'
 local checkpointPath='/Game/LD45/Blueprints/Checkpoint/Checkpoint.Checkpoint_C'
 local function send(op,id,at)
  write('out.events',table.concat({'RST',M.world,M.session,op,id,at or 0},'|')..'\n',true)
 end
 local function enqueue(list)
  if R.queue then return end
  local queue={}
  local p=M.player:K2_GetActorLocation()
  for _,v in ipairs(list)do local ok,a=pcall(function()return v:get()end);if not ok then a=v end;if valid(a)then local x=a:K2_GetActorLocation();local d=(x.X-p.X)^2+(x.Y-p.Y)^2+(x.Z-p.Z)^2;local q=M.peerPosition;if q then d=math.min(d,(x.X-q.X)^2+(x.Y-q.Y)^2+(x.Z-q.Z)^2)end;queue[#queue+1]={a=a,d=d}end end
  R.queue=queue;R.index=1
  table.sort(R.queue,function(a,b)return a.d<b.d end)
  -- Old IDs remain tombstones long enough to clear transport caches. New spawns get new IDs.
  local now=gs:GetRealTimeSeconds(M.player)
  for _,r in pairs(M.actors)do
   local e=r.e
   if valid(e)and valid(e.OwningSpawner)and not e.Data.Boss and not valid(e.OwningSpawner['Boss Group DA'])then
   if r.last then r.last[5]=now;r.last[9]=2;r.encoded=protocol.encode(r.last)end
   r.ended=now;r.retired=true
   if valid(r.e)then r.e:SetActorHiddenInGame(true);r.e:SetActorEnableCollision(false);r.e.CustomTimeDilation=0 end
   end
  end
  R.generation=R.generation+1;send('BEGIN',R.generation,now)
  log('REST refresh '..R.generation..' spawners='..#R.queue)
 end
 function R.install(hooks)
  if R.installed then return end;R.installed=true
  -- Native output parameter interception replaces the expensive Blueprint loop
  -- only for checkpoint callers asking for enemy spawners.
  local path='/Script/Engine.GameplayStatics:GetAllActorsOfClass'
  local a,b=RegisterHook(path,function()end,function(_,context,class,out)
   if not M.connected or not valid(M.player)then return end
   local owner=context:get();local cls=class:get()
   if not valid(owner)or not owner:IsA(checkpointPath)or not valid(cls)or full(cls)~='BlueprintGeneratedClass '..spawnerPath then return end
   if C.Role=='host'then R.requestRefresh=true
   else R.serial=R.serial+1;send('REQUEST',R.serial)end
   out:set({})
  end);hooks[#hooks+1]={path,a,b}
  local restPath=checkpointPath..':Rest'
  local c,d=RegisterHook(restPath,function(ctx)
   if not M.connected then return end
   local point=ctx:get();local p=point['Player Interacting']
   if valid(p)and full(p)==full(M.player)then R.localRest={player=p,damage=not point.GodModeOn}end
  end);hooks[#hooks+1]={restPath,c,d}
 end
 function R.event(f,world,peer)
  if #f~=7 or f[2]~='RST'or f[3]~=world or f[4]~=peer then return end
  local id,at=tonumber(f[6]),tonumber(f[7]);if not id or id<1 or id%1~=0 or not at or at~=at then return end
  if f[5]=='REQUEST'and C.Role=='host'then
   local key=peer..':'..id;if R.seen[key]then return end;R.seen[key]=true
   StaticFindObject('/Script/QueensDomain.Default__FS_SaveGameUtils'):ClearAllEnemySaveStates(M.player)
   R.requestRefresh=true
  elseif f[5]=='BEGIN'and C.Role=='client'and at>(M.rejectBefore or 0)then
   M.rejectBefore=at;M.pendingFrames={}
   -- Bosses and non-spawner enemies retain their identity across vanilla rests.
   -- Retire only IDs for which the host explicitly sends a hidden tombstone.
   M.lastLines={};M.raw=nil
   log('REST joined refresh '..id)
  end
 end
 function R.retire(r)
  health.update(r,0,true)
  if valid(r.visual)then M.byActor[full(r.visual)]=nil;r.visual:SetActorHiddenInGame(true)end
  if valid(r.e)then r.e:SetActorHiddenInGame(true);r.e:SetActorEnableCollision(false);M.byActor[full(r.e)]=nil end
  R.cleanup[#R.cleanup+1]=r
 end
 function R.update(now)
  local own=R.localRest
  if own then
   if not valid(own.player)then R.localRest=nil
   elseif not own.player.Resting and not own.player.IsDead then
    -- Release only the checkpoint's temporary immunity, preserving prior god mode.
    own.player.bCanBeDamaged=own.damage;R.localRest=nil
   end
  end
  if R.requestRefresh then
   R.requestRefresh=nil
   local list={};gs:GetAllActorsOfClass(M.player,StaticFindObject(spawnerPath),list)
   enqueue(list)
  end
  if R.queue then
   local started=os.clock()
   for i=1,8 do
    local entry=R.queue[R.index];if not entry then R.queue=nil;log('REST refresh complete '..R.generation);break end
    R.index=R.index+1;if valid(entry.a)then entry.a:PlayerRested()end
    if os.clock()-started>.0015 then break end
   end
  end
  for i=1,2 do
   local r=table.remove(R.cleanup);if not r then break end
   if valid(r.visual)then r.visual:K2_DestroyActor()end
   if valid(r.e)then M.frozen[full(r.e)]=nil;r.e:K2_DestroyActor()end
  end
 end
 function R.reset(travel)
  if not travel then for _,r in ipairs(R.cleanup)do if valid(r.visual)then r.visual:K2_DestroyActor()end;if valid(r.e)then r.e:K2_DestroyActor()end end end
  R.queue=nil;R.cleanup={};R.seen={};R.requestRefresh=nil;R.localRest=nil
 end
 return R
end
