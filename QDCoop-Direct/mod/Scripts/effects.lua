-- Cosmetic replicas only. No remote gameplay Blueprint, damage or collision is executed.
return function(dir,C,valid,full,read,write,log)
 local refs=C.ObjectRefs;local findObject=refs.find
 local scope=dofile(dir..'object_scope.lua')(valid,full,refs.validation);valid=scope.valid;full=scope.full
 local Validation=dofile(dir..'weak_validation.lua')(dir,C.NativeEffectValidation~=false,C.InlineWeakValidation~=false)
 local function loadAsset(path)local a=findObject(path);if valid(a)then return a end;scope.clear();a=LoadAsset(path);scope.clear();return a end
 local E=dofile(dir..'effect_protocol.lua');local Q=dofile(dir..'pose_math.lua');local Timeline=dofile(dir..'effect_timeline.lua')(E);local catalog=E.catalog
 local M={timeline=Timeline.new(),sources={},actors={},actorRoots={},pending={},pendingComponents={},ghosts={},excluded={},serial=0,seq=0,events=0,received=0,rendered=0,unknown={},hooks={},paramUnknown={},births=0,ends=0,overflow=0,deferred=0,selectedEnemies={}}
 local admission=dofile(dir..'effect_admission.lua').new(128,1)
 local ids,classes,variables,materialVariables={},{},{},{};for i,v in ipairs(catalog.assets)do ids[v.path]=i end;for _,v in ipairs(catalog.projectiles)do classes[v]=true end;for i,v in ipairs(catalog.variables)do variables[v]=i;variables[v:gsub('^User%.','')]=i;materialVariables[v]=i end;for i,v in ipairs(catalog.variables)do variables[v]=i end
 local function safe(fn)local ok,v=pcall(fn);if ok then return v end end
 local function asset(o)return valid(o)and full(o):match(' (/[^ ]+)$')or nil end
 local function same(a,b)return valid(a)and valid(b)and full(a)==full(b)end
 local function is(o,class)return valid(o)and o:IsA(class)end
 local function vec(t)return {X=t.X,Y=t.Y,Z=t.Z}end
 local function nums(t)return {t.Translation.X,t.Translation.Y,t.Translation.Z,t.Rotation.X,t.Rotation.Y,t.Rotation.Z,t.Rotation.W,t.Scale3D.X,t.Scale3D.Y,t.Scale3D.Z}end
 local function transform(t)return Q.transform({t[1],t[2],t[3]},{t[4],t[5],t[6],t[7]},{t[8],t[9],t[10]})end
 function M.bindBody(body)M.body=body end
 function M.bindEnemies(enemies)M.enemies=enemies end
 local function enemyOf(o)
  if not valid(o)or not M.enemies then return end;local key=full(o);local s=M.sources[key];if s and s.enemy then return s.enemy end
  if M.actorRoots[key]then return M.actorRoots[key]end
  local r=M.enemies.recordFor(o);if r then return r end
  local owner=safe(function()return o:GetOwner()end);return valid(owner)and M.actorRoots[full(owner)]or nil
 end
 local function sourceEnemy(o)return C.Role=='host'and enemyOf(o)or nil end
 local function owned(o,depth,seen)
  depth=depth or 0;if depth>14 or not valid(o)or not valid(M.player)then return false end
  seen=seen or{};local address=o:GetAddress();if seen[address]and seen[address]<=depth then return false end;seen[address]=depth
  if M.excluded[full(o)]then return false end
  if M.actors[full(o)]or M.sources[full(o)]then return true end
  if same(o,M.player)then return true end
  if sourceEnemy(o)then return true end
  if is(o,'/Script/Engine.SceneComponent')then local p=o:GetAttachParent();if valid(p)and not same(o,p)and owned(p,depth+1,seen)then return true end end
  local parent=safe(function()return o:GetOwner()end);if valid(parent)and not same(o,parent)and owned(parent,depth+1,seen)then return true end
  local inst=safe(function()return o:GetInstigator()end);return same(inst,M.player)
 end
 local function handOf(c)
  local o=c;local hand=1
  for i=1,12 do if not valid(o)then return 0 end;if same(o,M.player.Arms)then return hand end;local socket=o:GetAttachSocketName():ToString();if socket=='Weapon_L'or socket=='Hand_L'then hand=2 end;o=o:GetAttachParent()end;return 0
 end
 local function send(payload)if M.connected and M.session and M.world then write('out.events','VFX|'..M.world..'|'..M.session..'|'..payload..'\n',true);M.events=M.events+1 end end
 local function resource(c)
  if is(c,'/Script/Niagara.NiagaraComponent')then return asset(c.Asset),'niagara' end
  if is(c,'/Script/Engine.ParticleSystemComponent')then return asset(c.Template),'particle'end
  if is(c,'/Script/Engine.StaticMeshComponent')then return asset(c.StaticMesh),'mesh'end
  if is(c,'/Script/Engine.SkeletalMeshComponent')then return asset(c.SkeletalMesh),'skeletal'end
 end
 local function seedParameters(c)
  local params={};local owner=c:GetOwner();if not valid(owner)then return params end
  local chain={};local class=asset(owner:GetClass());for _=1,20 do if not class then break end;table.insert(chain,1,class);class=E.enemyCatalog.parents[class]end
  for _,key in ipairs(chain)do local defs=E.enemyCatalog.defaults[key];local values=defs and defs[full(c):match('%.([^%.]+)$')];for name,p in pairs(values or{})do local id=variables[name];if id then params[id]=p end end end;return params
 end
 local function track(c,forceMesh,forcedId,forcedEnemy,allowInactive)
  if not valid(c)or M.excluded[full(c)]then return end;local key=full(c);local path,kind=resource(c);local id=path and ids[path]
  if not id then if path then M.unknown[path]=true end;return end
  if not forceMesh and(kind=='mesh'or kind=='skeletal')then return end
  if C.Role=='client'and enemyOf(c)then return end
  local old=M.sources[key]
  -- Registered sources already carry their resolved enemy. Still verify the
  -- live resource: replacing a component's asset must create a new generation.
  if old and old.asset==id and old.enemy then return old end
  local enemy=forcedEnemy or sourceEnemy(c);if enemy and valid(enemy.e)and same(c,enemy.e.Mesh)and not allowInactive then return end
  if old and old.asset==id then old.enemy=old.enemy or enemy;return old end
  if not old and(kind=='mesh'or kind=='skeletal')and(not c:IsVisible()or c.bHiddenInGame)then return end
  if not forcedId and not allowInactive and(kind=='niagara'or kind=='particle')and not c:IsActive()then return end
  if old and old.born then send('R|'..old.id..'|'..string.format('%.6f',M.now or 0))end
  local count=0;for _ in pairs(M.sources)do count=count+1 end;if count>=192 then M.overflow=M.overflow+1;return end
  M.serial=M.serial+1;local s={c=c,id=forcedId or M.serial,asset=id,kind=kind,params=seedParameters(c),materials={},start=M.now or 0,enemy=enemy};M.sources[key]=s;return s
 end
 local function scanActor(a,meshes,enemy)
  local found={};if not valid(a)then return found end
  for _,v in ipairs(a:K2_GetComponentsByClass(findObject('/Script/Engine.SceneComponent')))do local c=v:get();if valid(c)then local s=track(c,meshes,nil,enemy);if s then found[#found+1]=s end end end
  return found
 end
 local function capture(s,now)
  local c=s.c;if not valid(c)or c:IsBeingDestroyed()then return nil end
  if C.Role=='client'and enemyOf(c)then return nil end
  -- Interest limits discovery, never the lifetime of an existing attachment.
  local owner=c:GetOwner();if s.enemy and s.enemy.retired and same(owner,s.enemy.e)then return nil end;if not s.worldEffect and not valid(owner)then return nil end;if valid(owner)and owner:IsActorBeingDestroyed()then return nil end
  -- ArcProjectile_Base ends collision before its delayed actor destruction.
  -- The remote cosmetic must end at that gameplay transition, not its lifespan.
  if is(owner,'/Game/Items/Weapons/_Shared/ArcProjectile_Base.ArcProjectile_Base_C')and owner.Destroying then return nil end
  local path=resource(c);if ids[path]~=s.asset then return nil end
  local hand=s.enemy and 0 or handOf(c);local t=c:K2_GetComponentToWorld();local anchor,bone=0,0
  if s.enemy and valid(s.enemy.e)then local e=s.enemy.e;local o=c
   for _=1,12 do if not valid(o)then break end;local parent=o:GetAttachParent();if same(parent,e.Mesh)then
     local socket=o:GetAttachSocketName():ToString();s.enemy.boneIds=s.enemy.boneIds or{};if not s.enemy.bonesMapped then for b=0,e.Mesh:GetNumBones()-1 do s.enemy.boneIds[e.Mesh:GetBoneName(b):ToString()]=b end;s.enemy.bonesMapped=true end
     local bid=s.enemy.boneIds[socket];if bid then anchor=3;bone=bid;t=M.maths:MakeRelativeTransform(t,e.Mesh:GetSocketTransform(FName(socket),0))else anchor=2;t=M.maths:MakeRelativeTransform(t,e.Mesh:K2_GetComponentToWorld())end;break
    end;o=parent
   end
   if anchor==0 and same(c:GetOwner(),e)then anchor=1;t=M.maths:MakeRelativeTransform(t,e:GetTransform())end
  end
  if hand~=0 then t=M.maths:MakeRelativeTransform(t,M.player.Arms:K2_GetComponentToWorld());local h=M.player.Arms:GetSocketTransform(FName(hand==2 and 'Hand_L'or 'Hand_R'),2).Translation;t.Translation={X=t.Translation.X-h.X,Y=t.Translation.Y-h.Y,Z=t.Translation.Z-h.Z}end
  local visible=c:IsVisible()and not c.bHiddenInGame
  local a=c:GetOwner();if valid(a)and safe(function()return a.bHidden end)then visible=false end
  local flags=visible and 1 or 0
  if s.kind=='niagara'or s.kind=='particle'then
   if c:IsActive()and not s.deactivated then flags=flags+2;s.lastActive=now;s.everActive=true end
   if s.worldEffect and (now-(s.lastActive or s.start)>1 or(not s.enemy and now-s.start>30))then return nil end
   if s.everActive and now-s.lastActive>1 then return nil end
  end
  local v={id=s.id,asset=s.asset,hand=hand,active=flags,mode=0,time=now,t=nums(t),entity=s.enemy and s.enemy.id or 0,anchor=anchor,bone=bone,params=s.params,materials={}}
  local qn=0;for i=4,7 do qn=qn+v.t[i]^2 end;if qn<.8 or qn>1.2 then return nil end
  if s.kind=='mesh'or s.kind=='skeletal'then
   local mat=c:GetMaterial(0);if is(mat,'/Script/Engine.MaterialInstanceDynamic')then local value=mat:K2_GetScalarParameterValue(FName('Dissolve'));s.params[materialVariables.Dissolve]={kind='s',values={value}}end
   for slot=0,math.min(c:GetNumMaterials(),8)-1 do local m=c:GetMaterial(slot);for j=1,5 do if not valid(m)then break end;local id=ids[asset(m)];if id then v.materials[slot]=id;break end;m=safe(function()return m.Parent end)end end
  end
  return v
 end
 local function birth(v)
  local payload='B|'..E.block(v):gsub('\n','~')
  -- The event transport bounds individual events. Larger parameter sets still
  -- arrive in the ordinary visual frame; never enqueue an oversize event.
  send(#payload<850 and payload or E.encode(v))
 end
 local function hook(path,pre,post,filter)
  if not valid(findObject(path))then return end
  local function wrap(fn,gate)local scoped=scope.wrap(fn);return function(...)
   if M.rendering or not M.connected then return end
   -- No object traversal, validity lookup or scoped table allocation for
   -- parameters outside the wire catalog. Discovery is handled separately.
   if gate then local ok,accept=pcall(gate,...);if not ok then M.lastError=tostring(accept);return end;if not accept then return end end
   if not valid(M.player)then return end;local ok,err=pcall(scoped,...);if not ok then M.lastError=tostring(err)end
  end end
  local a,b;if post then a,b=RegisterHook(path,pre and wrap(pre,filter)or function()end,wrap(post))else a,b=RegisterHook(path,wrap(pre or function()end,filter))end;M.hooks[#M.hooks+1]={path,a,b}
 end
 local function parameterFilter(kind)
  return function(ctx,key)
   local name=key:get():ToString()
   local id=kind=='s'and materialVariables[name]or(kind~='s'and(variables['User.'..name]or variables[name]))
   if id then admission.accepted=admission.accepted+1;return true end
   -- Unknown parameters may be the first observation of a new component.
   -- Keep that discovery, but coalesce repeated notifications and perform
   -- the expensive ownership/resource traversal on the normal update path.
   local c=ctx:get();local entry=admission:defer(c,c:GetAddress(),name,kind,M.now or 0)
   if entry then entry.path=c:GetFullName():match(' (.+)$');entry.weak=refs.captureFresh(c)end
   return false
  end
 end
 function M.install()
  if M.installed then return end;M.installed=true
  if not M.observersInstalled then
   M.observersInstalled=true
   -- Construction notifications only enqueue. UObject inspection and engine calls
   -- belong to the player tick, never to construction during a map load.
   NotifyOnNewObject('/Script/Engine.Actor',function(o)if M.connected and #M.pending<128 then M.pending[#M.pending+1]=o end end)
   for _,class in ipairs({'/Script/Niagara.NiagaraComponent','/Script/Engine.ParticleSystemComponent'})do NotifyOnNewObject(class,function(o)if M.connected and #M.pendingComponents<256 then M.pendingComponents[#M.pendingComponents+1]=o end end)end
  end
  hook('/Script/Engine.GameplayStatics:BeginDeferredActorSpawnFromClass',nil,function(ctx,ret,wco,class)
   local a=ret:get();local enemy=sourceEnemy(wco:get());if valid(a)and(enemy or classes[asset(class:get())]and owned(wco:get()))then M.actors[full(a)]=a;M.actorRoots[full(a)]=enemy;if #M.pending<128 then M.pending[#M.pending+1]=a end end
  end)
  hook('/Script/Engine.GameplayStatics:FinishSpawningActor',nil,function(ctx,ret,a)
   local actor=a:get();if valid(actor)and(sourceEnemy(actor)or classes[asset(actor:GetClass())]and owned(actor))then M.actors[full(actor)]=actor
    -- Only inspect this freshly completed actor. Other queued sources may already
    -- have been destroyed by a preceding attack before the next player tick.
    local found=scanActor(actor,true);local now=M.gs:GetRealTimeSeconds(M.player)
    for _,s in ipairs(found)do if not s.born and not s.enemy then local v=capture(s,now);if v then birth(v);s.born=true;M.births=M.births+1 end end end
   end
  end)
  local function location(kind)
   return function(ctx,ret,wco,template,location,rotation,scale)
    if C.Role=='client'and enemyOf(wco:get())then M.enemies.suppressFx(ret:get());return end
    if not owned(wco:get())then return end;local id=ids[asset(template:get())];if not id then M.unknown[asset(template:get())or 'nil']=true;return end
    local now=M.gs:GetRealTimeSeconds(M.player);local t=nums(M.maths:MakeTransform(vec(location:get()),{Pitch=rotation:get().Pitch,Yaw=rotation:get().Yaw,Roll=rotation:get().Roll},vec(scale:get())))
    local enemy=sourceEnemy(wco:get());M.serial=M.serial+1;local v={id=M.serial,asset=id,hand=0,active=3,mode=1,time=now,t=t,entity=enemy and enemy.id or 0};if not enemy then birth(v);M.births=M.births+1 end
    local c=ret:get();if valid(c)then local old=M.sources[full(c)];if old then send('R|'..old.id..'|'..string.format('%.6f',now));M.sources[full(c)]=nil end;local s=track(c,false,v.id,enemy);if s then s.born=not enemy;s.worldEffect=true;s.start=now;s.lastActive=now end end
   end
  end
  hook('/Script/Niagara.NiagaraFunctionLibrary:SpawnSystemAtLocation',nil,location('niagara'))
  hook('/Script/Engine.GameplayStatics:SpawnEmitterAtLocation',nil,location('particle'))
  for _,path in ipairs({'/Script/Niagara.NiagaraFunctionLibrary:SpawnSystemAttached','/Script/Engine.GameplayStatics:SpawnEmitterAttached'})do
   hook(path,nil,function(ctx,ret,template,parent)if C.Role=='client'and enemyOf(parent:get())then M.enemies.suppressFx(ret:get());return end;if owned(parent:get())then local c=ret:get();if valid(c)then local old=M.sources[full(c)];if old then if old.born then send('R|'..old.id..'|'..string.format('%.6f',M.gs:GetRealTimeSeconds(M.player)))end;M.sources[full(c)]=nil end;track(c,false,nil,sourceEnemy(parent:get()))end end end)
  end
  -- Existing enemy components can be activated briefly without being spawned
  -- again. Queue those transitions so staggered discovery cannot miss them.
  for _,path in ipairs({'/Script/Engine.ActorComponent:Activate','/Script/Engine.ActorComponent:SetActive'})do
   local setActive=path:find(':SetActive',1,true)~=nil
   hook(path,function(ctx,value)
    local s=M.sources[full(ctx:get())];if s then s.deactivated=setActive and not value:get()or false;s.captureAt=0 end
   end,function(ctx)
    local c=ctx:get()
    if valid(c)and(is(c,'/Script/Niagara.NiagaraComponent')or is(c,'/Script/Engine.ParticleSystemComponent'))and c:IsActive()and #M.pendingComponents<256 then M.pendingComponents[#M.pendingComponents+1]=c end
   end)
  end
  -- SVT and several older effects use the shared FXSystem API, rather than
  -- SetNiagaraVariableFloat. Mesh opacity is a separate material parameter.
  for _,spec in ipairs({{'/Script/Engine.FXSystemComponent:SetFloatParameter','f'},
                        {'/Script/Engine.MeshComponent:SetScalarParameterValueOnMaterials','s'}})do
   local kind=spec[2]
   hook(spec[1],function(ctx,key,value)
    local c=ctx:get();if not owned(c)then return end
    local s=track(c,kind=='s',nil,nil,true);if not s then return end
    local name=key:get():ToString();local id=kind=='s'and materialVariables[name]or(variables['User.'..name]or variables[name])
    if not id then M.paramUnknown[name]=true;return end
    s.params[id]={kind=kind,values={tonumber(value:get())}}
   end,nil,parameterFilter(kind))
  end
  -- Emit the actual stop/visibility transition, even when the projectile lives
  -- for several more seconds to finish its native trail. IsActive can remain
  -- true while Niagara is draining; it must not restart emission remotely.
  for _,path in ipairs({'/Script/Engine.ActorComponent:Deactivate','/Script/Engine.SceneComponent:SetVisibility','/Script/Engine.SceneComponent:SetHiddenInGame'})do
   local stopping=path:find(':Deactivate',1,true)~=nil
   hook(path,nil,function(ctx)
    local s=M.sources[full(ctx:get())];if not s then return end
    if stopping then s.deactivated=true end
    local now=M.gs:GetRealTimeSeconds(M.player);local v=capture(s,now)
    if v then s.latest=v;s.captureAt=now+1/30;birth(v);if not s.born then s.born=true;M.births=M.births+1 end end
   end)
  end
  for _,spec in ipairs({{'Float','f'},{'Bool','b'},{'Int','i'},{'Vec3','v'},{'LinearColor','c'}})do
   for _,prefix in ipairs({'SetVariable','SetNiagaraVariable'})do local kind=spec[2]
    hook('/Script/Niagara.NiagaraComponent:'..prefix..spec[1],function(ctx,key,value)
     local c=ctx:get();if not owned(c)then return end;local s=track(c,false,nil,nil,true);if not s then return end
     local name=key:get():ToString();local id=variables['User.'..name]or variables[name];if not id then M.paramUnknown[name]=true;return end
     local v=value:get();local values=kind=='v'and{v.X,v.Y,v.Z}or kind=='c'and{v.R,v.G,v.B,v.A}or{kind=='b'and(v and 1 or 0)or tonumber(v)}
     s.params[id]={kind=kind,values=values}
    end,nil,parameterFilter(kind))
   end
  end
 end
 scanActor=C.Perf.measure(scanActor,'fx.discoverActor');capture=C.Perf.measure(capture,'fx.captureSource')
 function M.update(player,world,session,now,connected)
  M.player=player;M.world=world;M.session=session;M.now=now;M.connected=connected
  if not valid(M.gs)then M.gs=findObject('/Script/Engine.Default__GameplayStatics')end
  if not valid(M.maths)then M.maths=findObject('/Script/Engine.Default__KismetMathLibrary')end
  if not connected then return end
  local workStart=os.clock()
  for _=1,6 do
   local entry=admission:pop();if not entry then break end
   -- Resolve a fresh live wrapper before using a deferred notification. The
   -- original component may already have been destroyed between callbacks.
   local c=refs.resolve(entry)
   if valid(c)and c:GetAddress()==entry.address and owned(c)then
    local source=track(c,entry.mesh,nil,nil,true)
    if source then for name in pairs(entry.names)do M.paramUnknown[name]=true end end
   end
   if os.clock()-workStart>=.0005 then break end
  end

  for i=#M.pending,math.max(1,#M.pending-3),-1 do local a=M.pending[i];table.remove(M.pending,i);if valid(a)and(sourceEnemy(a)or classes[asset(a:GetClass())]and owned(a))then M.actors[full(a)]=a;scanActor(a,true)end end
  for i=#M.pendingComponents,math.max(1,#M.pendingComponents-5),-1 do local c=M.pendingComponents[i];table.remove(M.pendingComponents,i);if valid(c)then if C.Role=='client'and enemyOf(c)then M.enemies.suppressFx(c)elseif owned(c)then track(c,false)end end end
  if now>=(M.nextScan or 0)then
   M.nextScan=now+.1
   local w=player.CurrentInHandWeapon;local off=player.Offhand_FlyingSword_WeaponScript
   scanActor(valid(w)and w.CurrentVisuals or nil,false);scanActor(valid(off)and off.CurrentVisuals or nil,false);scanActor(player.Daggervisuals,true)
   local children={};player.Arms:GetChildrenComponents(true,children);for _,v in ipairs(children)do track(v:get(),false)end
   if C.Role=='host'and M.enemies then
    if now>=(M.nextProjectileScan or 0)then M.nextProjectileScan=now+1;local class=findObject('/Game/Enemies/Shared/Enemy_Projectile_Base.Enemy_Projectile_Base_C');if valid(class)then local list={};M.gs:GetAllActorsOfClass(player,class,list);for _,v in ipairs(list)do local a=v:get();if sourceEnemy(a)then M.actors[full(a)]=a end end end end
   end
   for name,a in pairs(M.actors)do local enemy=valid(a)and sourceEnemy(a);if valid(a)and not(enemy and same(a,enemy.e))then scanActor(a,true)else M.actors[name]=nil;M.actorRoots[name]=nil end end
  end
  -- Spread discovery of existing enemy components across frames. Newly spawned
  -- effects still arrive through the native construction/spawn hooks above.
  if C.Role=='host'and M.enemies then
   local list=M.enemies.visualSources()or{};M.enemyScanDue=M.enemyScanDue or{};local scanned=0
   local initial=M.enemies.initialVisualSource and M.enemies.initialVisualSource()
   for step=1,math.max(#list,initial and 1 or 0)do
    local r=initial
    if not r then M.enemyCursor=((M.enemyCursor or 0)%#list)+1;r=list[M.enemyCursor]end
    if initial or now>=(M.enemyScanDue[r.id]or 0)then
     M.enemyScanDue[r.id]=now+2
     if valid(r.e)then
      scanActor(r.e,true,r);local class=asset(r.e:GetClass());for _=1,20 do if not class then break end
       for _,name in ipairs(E.enemyCatalog.references[class]or{})do local c=safe(function()return r.e[name]end);if valid(c)then track(c,true,nil,r)end end
       class=E.enemyCatalog.parents[class]
      end
     end
     scanned=scanned+1;if scanned>=1 then break end
    end
   end
  end
  -- Capture sources fairly across engine frames; the 30 Hz send tick only
  -- serializes cached values, never recaptures every component in one burst.
  if not M.captureQueue or M.captureIndex>#M.captureQueue then
   M.captureQueue={};M.captureIndex=1
   for key,s in pairs(M.sources)do if now>=(s.captureAt or 0)then M.captureQueue[#M.captureQueue+1]={key,s}end end
   table.sort(M.captureQueue,function(a,b)return(a[2].captureAt or 0)<(b[2].captureAt or 0)end)
  end
  local captureStart=os.clock()
  for step=1,6 do
   local entry=M.captureQueue[M.captureIndex];if not entry then break end;M.captureIndex=M.captureIndex+1;local key,s=entry[1],entry[2]
   if M.sources[key]==s then
   local v
   -- Attached equipment follows the client's continuously animated bone. Its
   -- relative offset/materials need occasional checks, not a full ownership,
   -- socket and material traversal on every visual packet. Projectiles and
   -- particles retain their full capture cadence.
   if s.latest and s.enemy and(s.kind=='mesh'or s.kind=='skeletal')and(s.latest.anchor or 0)>0 and now<(s.fullCaptureAt or 0)then
    if valid(s.c)and not s.c:IsBeingDestroyed()and valid(s.enemy.e)and not s.enemy.retired and not s.enemy.e:IsActorBeingDestroyed()then
     v=s.latest;v.time=now;v.active=(s.c:IsVisible()and not s.c.bHiddenInGame and not s.enemy.e.bHidden)and 1 or 0
    end
   else
    v=capture(s,now);s.fullCaptureAt=now+1+(s.id%3)*.016
   end
   if not v then if s.born then send('R|'..s.id..'|'..string.format('%.6f',now));M.ends=M.ends+1 end;M.sources[key]=nil
   else
    s.latest=v;s.encoded=nil;s.captureAt=now+((s.enemy and(s.kind=='mesh'or s.kind=='skeletal'))and .2 or 1/30);if not s.born then birth(v);s.born=true;M.births=M.births+1 end
   end
   end
   if os.clock()-captureStart>=.001 then break end
  end
  if now>=(M.nextSend or 0)then
   local frame={};for _,s in pairs(M.sources)do if s.latest then frame[#frame+1]=s end end
  M.nextSend=now+1/30;M.seq=M.seq+1
   local lines={'QDVF1',string.format('H|%s|%s|%d|%.6f',session,world,M.seq,now)};local bytes=#lines[1]+#lines[2]+8
   table.sort(frame,function(a,b)return(a.sent or 0)<(b.sent or 0)end);local n=0
   for _,s in ipairs(frame)do
    if n>=24 then break end
    local text=s.encoded;if not text then text=E.block(s.latest);s.encoded=text end
    if bytes+#text+1<=6900 then lines[#lines+1]=text;bytes=bytes+#text+1;n=n+1;s.sent=now end
   end
   M.deferred=M.deferred+#frame-n
   lines[#lines+1]='END';write('out.visual',table.concat(lines,'\n')..'\n')
  end
  local raw=read('in.visual');if raw~=M.lastRaw and raw~=''then local f=E.frame(raw);if f and f.world==world then M.timeline:push(f);M.received=M.received+1;M.lastRaw=raw end end
 end
 function M.event(fields,world,peerSession)
  if fields[2]~='VFX'or fields[3]~=world or fields[4]~=peerSession then return false end
  local payload=table.concat(fields,'|',5);if payload:sub(1,2)=='B|'then local f=E.frame('QDVF1\nH|'..peerSession..'|'..world..'|0|0\n'..payload:sub(3):gsub('~','\n')..'\nEND\n');if f then if M.timeline.session~=peerSession then M.timeline:reset(peerSession)end;for _,v in pairs(f.records)do M.timeline:add(v)end end
  elseif payload:sub(1,2)=='F|'then local v=E.record(payload);if v then if M.timeline.session~=peerSession then M.timeline:reset(peerSession)end;M.timeline:add(v)end
  elseif fields[5]=='R'and #fields==7 then local id=tonumber(fields[6]);local time=tonumber(fields[7]);if id and id%1==0 and id>=1 and id<=1e7 and time and time>=0 and time<=1e9 then M.timeline:finish(id,time)end end
  return true
 end
 local function releaseBones(g)if g and g.boneOwner and M.enemies then M.enemies.releaseAnimatedBones(g.boneOwner,g);g.boneOwner=nil end end
 local function displayMesh(r)return r.dead and r.e.Mesh or r.pose end
 -- These shipped graphs copy their attached parent, then simulate the cloth.
 -- Keep their original KawaiiPhysics nodes; MasterPose would freeze that cloth.
 local clothGraphs={
  ['/Game/Enemies/Boner_00/Skeletons24/ShrineBoss/Boner_ShrineBoss_PPABP.Boner_ShrineBoss_PPABP_C']=true,
  ['/Game/Enemies/JosamiKnight_00/Josami24/JosamiKnight_Cloth_A_ABP.JosamiKnight_Cloth_A_ABP_C']=true,
  ['/Game/Enemies/JosamiKnight_00/Josami24/JosamiKnight_Cloth_B_ABP.JosamiKnight_Cloth_B_ABP_C']=true,
 }
 local function follower(g,r,mesh)
  if g.followChecked==mesh then return g.followPose end
  g.followChecked=mesh;g.followPose=false;g.followClass=nil
  if g.kind~='skeletal'or not(r.native or r.dead)then return false end
  -- The shipped EnemySkeletalMeshAttachment uses MasterPose in BeginPlay.
  -- Inspect the corresponding local component, including its instance flag;
  -- a skeletal attachment is not necessarily a follower (weapons may animate).
  local cls=findObject('/Game/Enemies/Shared/EnemySkeletalMeshAttachment.EnemySkeletalMeshAttachment_C')
  if not valid(cls)then return false end
  for _,v in ipairs(r.e:K2_GetComponentsByClass(cls))do local c=v:get()
   if valid(c)and same(c.SkeletalMesh,g.c.SkeletalMesh)then
    local child,parent=g.c.SkeletalMesh,mesh.SkeletalMesh
    if valid(child)and valid(parent)and same(child.Skeleton,parent.Skeleton)then
     if c['Attachment Is ChildPose']and same(c.MasterPoseComponent:Get(),r.e.Mesh)then g.followPose=true;break end
     if clothGraphs[asset(c.AnimClass)]then g.followPose=true;g.followClass=c.AnimClass;break end
    end
   end
  end
  return g.followPose
 end
 local function destroy(g)releaseBones(g);if g and valid(g.actor)then safe(function()scope.clear();g.actor:K2_DestroyActor();scope.clear()end)end end
 local function make(v)
  local info=catalog.assets[v.asset];local t=transform(v.t);scope.clear();local a=M.gs:BeginDeferredActorSpawnFromClass(M.player,findObject('/Script/Engine.StaticMeshActor'),t,1,nil);assert(valid(a),'Visual actor spawn failed');M.excluded[full(a)]=true;M.creating={actor=a};M.gs:FinishSpawningActor(a,t);a:SetActorEnableCollision(false)
  local base=a:GetComponentByClass(findObject('/Script/Engine.StaticMeshComponent'));M.excluded[full(base)]=true;base:SetMobility(2);base:SetCollisionEnabled(0);local c=base
  if info.kind=='mesh'then c:SetStaticMesh((loadAsset(info.path)))else
   base:SetVisibility(false,false)
   local class=info.kind=='niagara'and'/Script/Niagara.NiagaraComponent'or info.kind=='particle'and'/Script/Engine.ParticleSystemComponent'or'/Script/Engine.SkeletalMeshComponent'
   c=a:AddComponentByClass(findObject(class),false,Q.transform({0,0,0},{0,0,0,1}),false);assert(valid(c),'Visual component creation failed')
   M.excluded[full(c)]=true;c.bAutoActivate=false
   if info.kind=='niagara'then c:SetAsset(loadAsset(info.path),true)elseif info.kind=='particle'then c:SetTemplate((loadAsset(info.path)))else c:SetSkeletalMesh(loadAsset(info.path),true);c:SetComponentTickEnabled(false)end
   c:SetCollisionEnabled(0)
  end
  c:SetOnlyOwnerSee(false);c:SetOwnerNoSee(false);c:SetHiddenInGame(false,false);c.bCastDynamicShadow=false
  local boundsRadius;if info.kind=='mesh'then local b=c.StaticMesh:GetBounds();local o=b.Origin;boundsRadius=b.SphereRadius+math.sqrt(o.X*o.X+o.Y*o.Y+o.Z*o.Z)end
  M.creating=nil;return {actor=a,c=c,asset=v.asset,kind=info.kind,active=-1,params={},materials={},boundsRadius=boundsRadius}
 end
 local function properties(g,v)
    for slot,asset in pairs(v.materials or {})do if g.materials[slot]~=asset then g.c:SetMaterial(slot,(loadAsset(catalog.assets[asset].path)));g.materials[slot]=asset end end
    for key,p in pairs(v.params or{})do
     local old=g.params[key];local changed=not old or old.kind~=p.kind
     if not changed then for i,x in ipairs(p.values)do if old.values[i]~=x then changed=true;break end end end
     if changed then
      if (g.kind=='mesh'or g.kind=='skeletal')and p.kind=='s'then g.c:SetScalarParameterValueOnMaterials(FName(catalog.variables[key]),p.values[1])
      elseif g.kind=='particle'and p.kind=='f'then g.c:SetFloatParameter(FName(catalog.variables[key]:gsub('^User%.','')),p.values[1])
      elseif g.kind=='niagara'then local name=FName(catalog.variables[key]);local x=p.values
       if p.kind=='f'then g.c:SetVariableFloat(name,x[1])elseif p.kind=='b'then g.c:SetVariableBool(name,x[1]==1)elseif p.kind=='i'then g.c:SetVariableInt(name,x[1])elseif p.kind=='v'then g.c:SetVariableVec3(name,{X=x[1],Y=x[2],Z=x[3]})elseif p.kind=='c'then g.c:SetVariableLinearColor(name,{R=x[1],G=x[2],B=x[3],A=x[4]})end
      end
      g.params[key]={kind=p.kind,values={table.unpack(p.values)}}
     end
    end
 end
 local function unchanged(a,b)
  if not b or a.asset~=b.asset or a.entity~=b.entity or a.anchor~=b.anchor or a.bone~=b.bone or a.active~=b.active then return false end
  for i=1,10 do if math.abs(a.t[i]-b.t[i])>1e-5 then return false end end
  for k,v in pairs(a.materials or{})do if (b.materials or{})[k]~=v then return false end end
  for k in pairs(b.materials or{})do if (a.materials or{})[k]==nil then return false end end
  for k,p in pairs(a.params or{})do local q=(b.params or{})[k];if not q or q.kind~=p.kind then return false end;for i,v in ipairs(p.values)do if q.values[i]~=v then return false end end end
  for k in pairs(b.params or{})do if not(a.params or{})[k]then return false end end
  return true
 end
 function M.apply(time,pose,enemyTime)
  if not valid(M.player)or M.now<(M.retryAt or 0)then return end;M.rendering=true
  local ok,err=pcall(function()
   local list=M.timeline:sample(time,enemyTime);local seen={}
   M.borrowed=M.borrowed or{}
   for id,v in pairs(list)do
    local info=catalog.assets[v.asset]
    local target=v.hand~=0 and M.body and M.body.effectTarget(info.path,v.hand)
    if not target and(v.entity or 0)>0 and info.kind=='skeletal'then
     local r=M.enemies and M.enemies.presentation(v.entity);local mesh=r and valid(r.e)and displayMesh(r)
     if valid(mesh)and asset(mesh.SkeletalMesh)==info.path then target=mesh end
    end
    if target then
     if M.ghosts[id]then destroy(M.ghosts[id]);M.ghosts[id]=nil end
     local g=M.borrowed[id];if not g or g.c~=target then g={c=target,kind=info.kind,params={},materials={}};M.borrowed[id]=g end
     properties(g,v)
    elseif(v.entity or 0)>0 and M.enemies and M.enemies.isBodyMesh(info.path)then
     if M.ghosts[id]then destroy(M.ghosts[id]);M.ghosts[id]=nil end
     M.borrowed[id]=nil
    else
     M.borrowed[id]=nil
    local g=M.ghosts[id];if g and(not valid(g.actor)or g.asset~=v.asset)then destroy(g);M.ghosts[id]=nil;g=nil end
    if not g then g=make(v);M.ghosts[id]=g;M.rendered=M.rendered+1 end;seen[id]=true
    -- Bone-attached effects may remain visible while their owner is offscreen.
    -- Dynamic/unknown consumers retain full refresh; bounded native static
    -- attachments may relax it only when safely behind the local camera.
    local scale=math.max(math.abs(v.t[8]),math.abs(v.t[9]),math.abs(v.t[10]))
    g.boneStaticBounded=g.kind=='mesh'and g.parentPose~=nil and g.boundsRadius~=nil and(g.boundsRadius*scale+math.sqrt(v.t[1]^2+v.t[2]^2+v.t[3]^2))<=500
    g.boneActive=v.active%2==1
    local record=M.enemies and M.enemies.presentation(v.entity)
    local mesh=record and valid(record.e)and displayMesh(record)
    local follows=v.anchor==2 and record and valid(mesh)and follower(g,record,mesh)
    local boneOwner=record and not record.dead and(v.anchor==3 or follows)and M.enemies and M.enemies.requireAnimatedBones(v.entity,g)or nil
    if g.boneOwner~=boneOwner then releaseBones(g);g.boneOwner=boneOwner end
    local owner=g.parentPose and M.enemies and M.enemies.presentation(v.entity)
    -- UE already moves native attachments every rendered frame. An unchanged
    -- relative offset needs no reflected transforms, socket lookup or setters.
    local skip=owner and valid(owner.e)and (displayMesh(owner)==g.parentPose or g.parentRoot and owner.e==g.parentActor) and valid(g.parentPose) and g.active==v.active and (v==g.last or unchanged(v,g.last))
    if not skip then
    local t=transform(v.t);local anchored=true;local parent,socket
    if(v.entity or 0)>0 and(v.anchor or 0)>0 then local r=M.enemies and M.enemies.presentation(v.entity)
     if r and valid(r.e)and valid(displayMesh(r))then
      if v.anchor==1 and valid(r.e.RootComponent)then parent=r.e.RootComponent;socket=FName('None')
      elseif(r.native or r.dead)and(v.anchor==2 or(v.anchor==3 and r.names[v.bone+1]))then
       parent=displayMesh(r);socket=v.anchor==3 and r.names[v.bone+1]or FName('None')
      else local basis
       if v.anchor==1 then basis=r.e:GetTransform()elseif v.anchor==2 then basis=r.pose:K2_GetComponentToWorld()elseif r.names[v.bone+1]then basis=r.native and r.pose:GetSocketTransform(r.names[v.bone+1],0)or r.pose:GetBoneTransformByName(r.names[v.bone+1],0)end
       if basis then t=M.maths:ComposeTransforms(t,basis)else anchored=false end
      end
     else anchored=false end
    end
    if v.hand~=0 and valid(pose)then local hand=pose:GetSocketTransform(FName(v.hand==2 and'hand_l'or'hand_r'),2).Translation;t.Translation={X=t.Translation.X+hand.X,Y=t.Translation.Y+hand.Y,Z=t.Translation.Z+hand.Z};t=M.maths:ComposeTransforms(t,pose:K2_GetComponentToWorld())end
    -- Native skeletal attachment follows the bone at engine frame rate. Lua
    -- only updates its relative offset when the replicated offset changes.
    local attachment=parent and(full(parent)..':'..socket:ToString())or''
    if g.attachment~=attachment then
     if parent then assert(g.c:K2_AttachToComponent(parent,socket,0,0,0,false),'effect attachment failed')
     elseif g.attachment and g.attachment~=''then g.c:K2_DetachFromComponent(1,1,1,false)end
     g.attachment=attachment;g.parentPose=parent;g.parentRoot=parent and v.anchor==1;g.parentActor=g.parentRoot and M.enemies.presentation(v.entity).e or nil;g.placement=nil
     if g.kind=='skeletal'then
      g.c:SetMasterPoseComponent(follows and not g.followClass and parent or nil,true)
      if follows and g.followClass then g.c:SetAnimClass(g.followClass);g.c.VisibilityBasedAnimTickOption=0;g.c:SetComponentTickEnabled(true)
      else g.c:SetComponentTickEnabled(false)end
     end
    end
    local placement=nums(t);local moved=not g.placement
    if g.placement then for k=1,10 do if math.abs(placement[k]-g.placement[k])>1e-5 then moved=true;break end end end
    if moved then if parent then g.c:K2_SetRelativeTransform(t,false,{},true)else g.c:K2_SetWorldTransform(t,false,{},true)end;g.placement=placement end
    properties(g,v)
    local active=anchored and v.active or 0
    if g.active~=active then
     g.c:SetVisibility(active%2==1,false)
     if(g.kind=='niagara'or g.kind=='particle')and(g.active<0 or(g.active>=2)~=(active>=2))then
      if active>=2 then g.c:Activate(true)else g.c:Deactivate()end
     end;g.active=active
    end
    g.entity=v.entity or 0;g.anchor=v.anchor or 0
    end
    g.last=v
    end
   end
   for id in pairs(M.borrowed)do if not list[id]then M.borrowed[id]=nil end end
   for id,g in pairs(M.ghosts)do if not seen[id]then
    releaseBones(g)
    local track=M.timeline.tracks[id]
    if track and not track.ended and valid(g.c)then
     -- A missing visual frame hides immediately, but retains the component
     -- through the timeline's short grace period instead of reallocating it.
     if g.active~=-1 then g.c:SetVisibility(false,false);if g.kind=='niagara'or g.kind=='particle'then g.c:Deactivate()end;g.active=-1 end
    else destroy(g);M.ghosts[id]=nil end
   end end
  end);M.rendering=false;if not ok then destroy(M.creating);M.creating=nil;M.retryAt=M.now+1;M.lastError=tostring(err);log('VFX error '..tostring(err))end
 end
 function M.resetRemote(travel)
  M.borrowed={}
  for _,g in pairs(M.ghosts)do if travel then releaseBones(g)else destroy(g)end end;M.ghosts={};M.timeline:reset()
 end
 function M.reset(travel)
  admission:reset()
  M.resetRemote(travel);M.selectionList=nil;M.sources={};M.actors={};M.actorRoots={};M.pending={};M.pendingComponents={};M.excluded={};M.lastRaw=nil;M.nextScan=0;M.nextWork=0;M.nextProjectileScan=0;M.nextSend=0;M.connected=false;M.player=nil;M.enemyScanDue={};M.enemyCursor=0;M.captureQueue=nil;M.captureIndex=1
 end
 function M.suspend()
  M.connected=false;M.player=nil
  for _,h in ipairs(M.hooks)do UnregisterHook(h[1],h[2],h[3])end
  M.hooks={};M.installed=false
 end
 function M.status()
  write('effects-admission.txt',string.format('accepted=%d filtered=%d discovery=%d coalesced=%d overflow=%d pending=%d\n',admission.accepted,admission.filtered,admission.discovered,admission.coalesced,admission.overflow,admission.count))
  local n,g=0,0;for _ in pairs(M.sources)do n=n+1 end;for _ in pairs(M.ghosts)do g=g+1 end
  local unknown={};for k in pairs(M.unknown)do unknown[#unknown+1]=k end;for k in pairs(M.paramUnknown)do unknown[#unknown+1]='parameter:'..k end
  local rows={};for _,s in pairs(M.sources)do if s.enemy then rows[#rows+1]='S|'..s.id..'|'..s.enemy.id..'|'..s.asset..'|'..full(s.c)end end;for id,v in pairs(M.ghosts)do if(v.entity or 0)>0 then rows[#rows+1]='G|'..id..'|'..v.entity..'|'..v.asset..'|'..v.anchor..'|'..full(v.c)end end;write('enemy-effects-bindings.txt',table.concat(rows,'\n'))
  return string.format('sources=%d ghosts=%d births=%d ends=%d frames=%d received=%d created=%d overflow=%d deferred=%d\nerror=%s\nunknown=%s\n',n,g,M.births,M.ends,M.seq,M.received,M.rendered,M.overflow,M.deferred,M.lastError or'',table.concat(unknown,';'))
 end
 M.isLocalSource=scope.wrap(function(o)return not enemyOf(o)and owned(o)end)
 local originalStatus=M.status;function M.status()return originalStatus()..Validation.status():gsub('presentWeak','fxWeak')..'\n'end
 M.update=scope.wrap(M.update,Validation);M.apply=scope.wrap(M.apply,Validation);M.status=scope.wrap(M.status)
 return M
end

