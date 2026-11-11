-- Native local playback is preserved. Only peer events create cosmetic voices.
return function(dir,C,valid,full,read,write,log,localVisualSource)
 local refs=C.ObjectRefs;local findObject=refs.find
 local scope=dofile(dir..'object_scope.lua')(valid,full,refs.validation);valid=scope.valid;full=scope.full
 local resources=dofile(dir..'audio_resources.lua')(valid,full,scope.clear)
 local loadAsset=resources.load
 local P=dofile(dir..'audio_protocol.lua');local cat=P.catalog
 local M={serial=0,hooks={},sources={},prepared={},excluded={},pending={},enemy={},heard={},voices=P.new(),played=0,sent=0,echoes=0,stopped=0,captured=0,unknown={},suppressed=0,overflows=0}
 local ids,attens={},{};for i,v in ipairs(cat.sounds)do ids[v.path]=i end;for i,v in ipairs(cat.attenuations)do attens[v]=i end
 local function safe(fn)local ok,v=pcall(fn);if ok then return v end end
 local function asset(o)return valid(o)and full(o):match(' (/[^ ]+)$')end
 local function same(a,b)return valid(a)and valid(b)and full(a)==full(b)end
 local function is(o,c)return valid(o)and o:IsA(c)end
 local function now()return M.gs:GetRealTimeSeconds(M.player)end
 local function pos(o)return safe(function()if is(o,'/Script/Engine.SceneComponent')then return o:K2_GetComponentLocation()else return o:K2_GetActorLocation()end end)end
 local function vec(v)return {X=v.X,Y=v.Y,Z=v.Z}end
 local function owned(o,depth,seen)
  if not valid(o)or(depth or 0)>12 or M.excluded[full(o)]then return false end
  seen=seen or{info={}};local address=o:GetAddress();local level=depth or 0;if seen[address]and seen[address]<=level then return false end;seen[address]=level
  local info=seen.info[address];if not info then info={};seen.info[address]=info end
  if same(o,M.player)then return true end
  if info.visual==nil then info.visual=localVisualSource and localVisualSource(o)or false end
  if info.visual then return true end
  if M.prepared[full(o)]then return true end
  if not info.queried then
   info.parent=is(o,'/Script/Engine.SceneComponent')and o:GetAttachParent()or nil
   info.owner=safe(function()return o:GetOwner()end);info.instigator=safe(function()return o:GetInstigator()end);info.queried=true
  end
  local a=info.parent;if valid(a)and not same(o,a)and owned(a,level+1,seen)then return true end
  local owner=info.owner;if valid(owner)and not same(o,owner)and owned(owner,level+1,seen)then return true end
  return same(info.instigator,M.player)
 end
 function M.bindEnemies(enemies)M.enemies=enemies end
 local function enemy(o)
  if M.enemies then local r=M.enemies.recordFor(o);if r then return r.e end end
  if is(o,'/Game/Enemies/EnemyBase.EnemyBase_C')then return o end
  local a=safe(function()return o:GetOwner()end);if is(a,'/Game/Enemies/EnemyBase.EnemyBase_C')then return a end
 end
 local function send(f)if M.connected then write('out.events','SND|'..M.world..'|'..M.session..'|'..P.encode(f)..'\n',true);M.sent=M.sent+1 end end
 local function stopSource(s,fade)if s and not s.ended then send({'R',s.id,now(),fade or 0});s.ended=true end end
 local function emit(r,echo,ordinal)
  M.serial=M.serial+1;r.id=M.serial;r.time=now();r.echo=echo or 0;r.ordinal=ordinal or 0
  local info=cat.sounds[r.asset];r.life=math.min(600,math.max(.1,info.duration or 0));if r.life<=.1 then r.life=5 end
  r.loop=info.looping and 1 or 0
  send({'A',r.id,r.asset,r.time,r.anchor,r.pos.X,r.pos.Y,r.pos.Z,r.volume,r.pitch,r.start,r.atten,r.echo,r.ordinal,r.life,r.loop})
  if valid(r.c)then local k=full(r.c);local old=M.sources[k];if old then stopSource(old)end;M.sources[k]=r;r.last=now()end
  return r
 end
 local function capture(sound,owner,where,volume,pitch,start,atten,c,anchor)
  if M.rendering or not M.connected or not valid(sound)then return end
  local key=asset(sound);local id=ids[key];if not id then M.unknown[key or '?']=true;return end
  local e=enemy(owner);if not e and not owned(owner)then return end
  if M.replay and not e then return end
  local v=math.max(0,math.min(8,volume or 1));local rate=math.max(.1,math.min(4,pitch or 1));local point=where or pos(owner)or pos(M.player);if not point then return end
  if e then local ep=pos(e);if ep and ((point.X-ep.X)^2+(point.Y-ep.Y)^2+(point.Z-ep.Z)^2)>640000 then point=ep end end
  M.captured=M.captured+1
  local r={asset=id,owner=owner,c=c,pos=vec(point),volume=v,pitch=rate,start=math.max(0,math.min(600,start or 0)),atten=attens[asset(atten)]or 0,anchor=anchor or 0}
  if r.anchor==1 then local p=pos(M.player);r.pos={X=point.X-p.X,Y=point.Y-p.Y,Z=point.Z-p.Z}end
  if e then
   local k=full(e);if M.replicaDeath==k then return end
   if C.Role=='client' and M.replicaDeath then return end
   local pending=M.enemy[k]or {};M.enemy[k]=pending
   -- Spawn, Play and the passive observer can see the same component in one
   -- frame. Preserve separate component-less impacts, but send this voice once.
   if valid(c)then for _,queued in ipairs(pending)do if same(queued.c,c)then return end end end
   r.at=now();pending[#pending+1]=r
   if #pending>32 then table.remove(pending,1);M.overflows=M.overflows+1 end
  else emit(r)end
 end
 -- Blueprint TryApplyDamage callbacks run AFTER native sound calls. Commit the
 -- pending calls to the actual successful local hit, including its request id.
 function M.commitHit(e,token,predicted)
  local key=full(e);local list=M.enemy[key]or {};M.enemy[key]=nil;local ord={};local t=now()
  for _,r in ipairs(list)do if t-r.at<.25 then
   ord[r.asset]=(ord[r.asset]or 0)+1
   if predicted then M.heard[token..':'..r.asset..':'..ord[r.asset]]=t
   else emit(r,M.replay or 0,ord[r.asset])end
  end end
 end
 function M.beginReplay(token)M.replay=token end
 function M.endReplay()M.replay=nil end
 function M.beginReplicaDeath(e)M.replicaDeath=full(e)end
 function M.endReplicaDeath()M.replicaDeath=nil end
 local function hook(path,pre,post)
  if not valid(findObject(path))then return end
  local function wrap(fn)return scope.wrap(function(...)if not M.connected or not valid(M.player)then return end;local ok,e=pcall(fn,...);if not ok then M.error=tostring(e)end end)end
  local a,b=RegisterHook(path,wrap(pre or function()end),post and wrap(post)or nil);M.hooks[#M.hooks+1]={path,a,b}
 end
 function M.install()
  if M.installed then return end;M.installed=true
  if not M.observer then M.observer=true;NotifyOnNewObject('/Script/Engine.AudioComponent',function(c)if M.connected and #M.pending<256 then M.pending[#M.pending+1]=c end end)end
  local function mute(owner,volume)
   if M.rendering then return end
   local e=enemy(owner)
   if (e and M.replicaDeath==full(e))or(C.Role=='client'and M.enemies and M.enemies.isCosmetic(owner))or(M.replay and not e and owned(owner))then volume:set(0);M.suppressed=M.suppressed+1 end
  end
  for _,name in ipairs({'PlaySoundAtLocation','SpawnSoundAtLocation'})do
   local spawning=name:sub(1,5)=='Spawn'
   hook('/Script/Engine.GameplayStatics:'..name,function(ctx,w,s,l,r,v)mute(w:get(),v)end,function(ctx,...)
    if M.rendering then return end;local a={...};local c;if spawning then c=a[1]:get();table.remove(a,1)end
    capture(a[2]:get(),a[1]:get(),vec(a[3]:get()),a[5]:get(),a[6]:get(),a[7]:get(),a[8]:get(),c,0)
   end)
  end
  hook('/Script/Engine.GameplayStatics:SpawnSoundAttached',function(ctx,s,parent,socket,l,r,typ,stop,v)mute(parent:get(),v)end,function(ctx,ret,s,parent,socket,l,r,typ,stop,v,pitch,start,atten)
   if M.rendering then return end;local c=ret:get();local o=parent:get();local point
   if typ:get()==1 then point=vec(l:get())elseif valid(o)then point=M.maths:TransformLocation(o:GetSocketTransform(socket:get(),0),vec(l:get()))end
   local anchor=owned(o)and(not enemy(o))and safe(function()local a=o;for i=1,12 do if same(a,M.player.Arms)then return true end;a=a:GetAttachParent();if not valid(a)then return false end end end)and 1 or 0
   capture(s:get(),o,point,v:get(),pitch:get(),start:get(),atten:get(),c,anchor)
  end)
  for _,name in ipairs({'PlaySound2D','SpawnSound2D','CreateSound2D'})do
   local spawning=name~='PlaySound2D'
   hook('/Script/Engine.GameplayStatics:'..name,function(ctx,w,s,v)mute(w:get(),v)end,function(ctx,...)
    if M.rendering then return end;local a={...};local c;if spawning then c=a[1]:get();table.remove(a,1)end
    local owner=a[1]:get();if name=='CreateSound2D'then if valid(c)and owned(owner)then M.prepared[full(c)]=owner end;return end
    capture(a[2]:get(),owner,pos(M.player),a[3]:get(),a[4]:get(),a[5]:get(),nil,c,1)
   end)
  end
  for _,name in ipairs({'Play','FadeIn'})do hook('/Script/Engine.AudioComponent:'..name,nil,function(ctx,...)
   if M.rendering then return end;local c=ctx:get();if M.excluded[full(c)]or(not owned(c)and not enemy(c))then return end;local a={...};local st=name=='Play'and a[1]:get()or a[3]:get()
   capture(c.Sound,c,pos(c),c.VolumeMultiplier,c.PitchMultiplier,st,c.AttenuationSettings,c,0)
  end)end
  hook('/Script/Engine.AudioComponent:Stop',function(ctx)if not M.rendering then stopSource(M.sources[full(ctx:get())])end end)
  hook('/Script/Engine.AudioComponent:FadeOut',function(ctx,duration)if not M.rendering then stopSource(M.sources[full(ctx:get())],math.min(10,math.max(0,duration:get())))end end)
 end
 -- SpawnSoundAtLocation uses bAutoDestroy=true. Stop owns its teardown; do not
 -- call another destruction function after Stop may already have destroyed it.
 local function dispose(v)if valid(v.c)then safe(function()v.c:Stop()end)end end
 function M.resetRemote(travel)
  if not travel then M.rendering=true;for _,v in pairs(M.voices.voices)do dispose(v)end;M.rendering=false end
  M.voices=P.new();M.peer=nil
 end
 function M.reset(travel)
  M.resetRemote(travel);resources.reset();M.sources={};M.prepared={};M.excluded={};M.pending={};M.enemy={};M.heard={};M.attenCache={};M.conc=nil;M.replay=nil;M.replicaDeath=nil;M.connected=false;M.player=nil;M.enemyCursor=0;M.nextEnemyScan=0
 end
 function M.suspend()M.connected=false;for _,h in ipairs(M.hooks)do UnregisterHook(h[1],h[2],h[3])end;M.hooks={};M.installed=false end
 capture=C.Perf.measure(capture,'audio.capture')
 function M.update(player,world,session,time,connected)
  M.player=player;M.world=world;M.session=session;M.connected=connected;M.now=time;M.gs=findObject('/Script/Engine.Default__GameplayStatics');M.maths=findObject('/Script/Engine.Default__KismetMathLibrary')
  if not connected then return end
  -- Ambient components may have started before the player joined the room.
  if C.Role=='host'and M.enemies and time>=(M.nextEnemyScan or 0)then
   M.nextEnemyScan=time+.05
   local list=M.enemies.visualSources()or{}
   if #list>0 then
    M.enemyCursor=((M.enemyCursor or 0)%#list)+1
    local actor=list[M.enemyCursor].e
    if valid(actor)then
     for _,entry in ipairs(actor:K2_GetComponentsByClass(findObject('/Script/Engine.AudioComponent')))do
      local c=entry:get()
      if valid(c)and c:IsPlaying()and not M.sources[full(c)]then
       capture(c.Sound,c,pos(c),c.VolumeMultiplier,c.PitchMultiplier,0,c.AttenuationSettings,c,0)
      end
     end
    end
   end
  end
  for i=#M.pending,1,-1 do local c=M.pending[i];table.remove(M.pending,i);if valid(c)and not M.excluded[full(c)]and not M.sources[full(c)]and(owned(c)or enemy(c))and c:IsPlaying()then capture(c.Sound,c,pos(c),c.VolumeMultiplier,c.PitchMultiplier,0,c.AttenuationSettings,c,0)end end
  for key,s in pairs(M.sources)do
   -- An inaudible one-shot may be culled locally by the audio device. That is
   -- not a gameplay cancellation for a listener on the other computer.
   if s.ended or not valid(s.c)or not s.c:IsPlaying()then if s.loop==1 then stopSource(s)end;M.sources[key]=nil
   elseif time-(s.checked or 0)>.1 then
    s.checked=time
    local p=pos(s.c);if p then if s.anchor==1 then local a=pos(player);p={X=p.X-a.X,Y=p.Y-a.Y,Z=p.Z-a.Z}end;local volume=math.max(0,math.min(8,s.c.VolumeMultiplier));local pitch=math.max(.1,math.min(4,s.c.PitchMultiplier));local old=s.update
     if not old or math.abs(p.X-old[1])+math.abs(p.Y-old[2])+math.abs(p.Z-old[3])>1 or math.abs(volume-old[4])>.001 or math.abs(pitch-old[5])>.001 or(s.loop==1 and time-(s.last or 0)>.5)then
      send({'U',s.id,time,p.X,p.Y,p.Z,volume,pitch});s.last=time;s.update={p.X,p.Y,p.Z,volume,pitch}
     end end
   end
  end
  -- Damage hooks consume their sounds synchronously. Remaining host sounds
  -- are attack, movement, ambience or death events and must also reach guests.
  for k,list in pairs(M.enemy)do
   M.enemy[k]=nil
   if C.Role=='host'then for _,r in ipairs(list)do emit(r)end end
  end
  for k,t in pairs(M.heard)do if time-t>15 then M.heard[k]=nil end end
 end
 function M.event(fields,world,peer)
  if fields[2]~='SND'or fields[3]~=world or fields[4]~=peer or not peer then return false end
  if M.peer~=peer then M.resetRemote();M.peer=peer end
  local f=P.parse(table.concat(fields,'|',5));if f then M.voices:push(f)end;return true
 end
 local function attenuation(id,sound)
  M.attenCache=M.attenCache or {};local key=id..':'..sound;if valid(M.attenCache[key])then return M.attenCache[key]end
  local asset=loadAsset(cat.sounds[sound].path);local src=id>0 and loadAsset(cat.attenuations[id])or asset.AttenuationSettings
  -- Keep the cooked asset, including its native curves and nested settings.
  -- 2D local cues use the game's existing spatial player attenuation remotely.
  if not valid(src)or not src.Attenuation.bAttenuate or not src.Attenuation.bSpatialize then src=loadAsset(cat.fallbackAttenuation)end
  resources.hold(src);M.attenCache[key]=src;return src
 end
 local function concurrency()
  if valid(M.conc)then return M.conc end
  local template=loadAsset(cat.concurrency)
  -- Native template construction preserves the complete reflected struct.
  M.conc=StaticConstructObject(findObject('/Script/Engine.SoundConcurrency'),M.player,FName('None'),0,0,false,false,template)
  scope.clear();resources.hold(M.conc);M.conc.Concurrency.MaxCount=16;return M.conc
 end
 function M.apply(time,pose)
  if not M.connected or not valid(M.player)then return end
  M.rendering=true;local ok,err=pcall(function()
   M.voices:prune(time)
   for id,v in pairs(M.voices.voices)do local a=v.start
    if a then
     local ended=v.stop and time>=v.stop[3]
     if not v.started and time>=a[4]then
      v.started=true
      local heard=a[13]>0 and M.heard[a[13]..':'..a[3]..':'..a[14]]
      if heard then M.echoes=M.echoes+1;v.skipped=true
      elseif time-a[4]>1 or ended then v.skipped=true
      else
       local p={X=a[6],Y=a[7],Z=a[8]};if a[5]==1 then p={X=pose.X+p.X,Y=pose.Y+p.Y,Z=pose.Z+p.Z}end
       local sound=loadAsset(cat.sounds[a[3]].path)
       local atten=attenuation(a[12],a[3]);local conc=concurrency()
       v.c=M.gs:SpawnSoundAtLocation(M.player,sound,p,{Pitch=0,Yaw=0,Roll=0},a[9],a[10],a[11],atten,conc,true)
       scope.clear()
       if valid(v.c)then M.excluded[full(v.c)]=true;M.played=M.played+1 else M.unavailable=(M.unavailable or 0)+1 end
      end
     end
     if valid(v.c)then
      local u=v.update;local p={X=a[6],Y=a[7],Z=a[8]};if u and u[3]<=time then p={X=u[4],Y=u[5],Z=u[6]};if v.appliedUpdate~=u then v.c:SetVolumeMultiplier(u[7]);v.c:SetPitchMultiplier(u[8]);v.appliedUpdate=u end end
      if a[5]==1 then p={X=pose.X+p.X,Y=pose.Y+p.Y,Z=pose.Z+p.Z}end;if not v.position or math.abs(p.X-v.position.X)+math.abs(p.Y-v.position.Y)+math.abs(p.Z-v.position.Z)>.1 then v.c:K2_SetWorldLocation(p,false,{},true);v.position=p end
      if ended and not v.stopping then v.c:FadeOut(v.stop[4],0,0);v.stopping=time+v.stop[4];M.stopped=M.stopped+1 end
     end
     if (v.stopping and time>v.stopping+.1)or (ended and not valid(v.c))or(time-a[4]>a[15]/a[10]+1 and a[16]==0)or(a[16]==1 and time-(v.last or a[4])>1)then dispose(v);M.voices:forget(id)end
    end
   end
  end);M.rendering=false;if not ok then M.error=tostring(err);log('Audio '..tostring(err))end
 end
 function M.status()
  local sources,voices=0,0;for _ in pairs(M.sources)do sources=sources+1 end;for _,v in pairs(M.voices.voices)do if valid(v.c)then voices=voices+1 end end
  local u={};for k in pairs(M.unknown)do u[#u+1]=k end
  return string.format('captured=%d sent=%d played=%d localEchoSkipped=%d nativeSuppressed=%d sources=%d voices=%d stopped=%d overflow=%d unavailable=%d duplicates=%d\nerror=%s\nunknown=%s\n',M.captured,M.sent,M.played,M.echoes,M.suppressed,sources,voices,M.stopped,M.overflows+M.voices.dropped,M.unavailable or 0,M.voices.duplicate,M.error or'',table.concat(u,';'))
 end
 M.update=scope.wrap(M.update);M.apply=scope.wrap(M.apply);M.status=scope.wrap(M.status)
 return M
end
