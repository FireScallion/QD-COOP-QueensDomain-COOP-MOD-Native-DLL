-- A single native crossing gathers one actor's live scalar/vector state.
-- Weak engine handles are checked on every gather. Metadata is rebuilt after
-- each world reset; unsupported layouts retain the existing Lua capture path.
return function(dir,enabled,valid)
 local M={calls=0,actors=0,registrations=0,rejected=0,fallbacks=0,expired=0,refreshes=0};local native,initialized
 local ok,err=pcall(function()assert(enabled~=false,'disabled by configuration');native=assert(package.loadlib(dir..'qdcapture.dll','qdcapture'))end)
 if not ok then M.error=tostring(err)end;M.available=native~=nil
 local function fields(o)
  local out={};while valid(o)do o:ForEachProperty(function(p)local name=p:GetFName():ToString();if not out[name]then out[name]=p end end);o=o:GetSuperStruct()end;return out
 end
 local function address(p,kind)
  assert(p and p:IsValid(),'missing capture property');assert(p:GetFullName():match('^'..kind..' '),p:GetFullName());return p:GetAddress()
 end
 local function initialize()
  local base=StaticFindObject('/Game/Enemies/EnemyBase.EnemyBase_C');local mesh=StaticFindObject('/Script/Engine.SkeletalMeshComponent');assert(valid(base)and valid(mesh),'capture classes not loaded')
  local f=fields(base);local args={base:GetAddress(),mesh:GetAddress(),address(f.HP,'FloatProperty'),address(f.Mesh,'ObjectProperty')}
  for _,name in ipairs({'bIsAttacking','bInRecoveryTime','bIsChasing','bHidden'})do assert(f[name],'missing '..name);args[#args+1]=address(f[name],'BoolProperty')end
  for _,path in ipairs({'/Script/Engine.Actor:K2_GetActorLocation','/Script/Engine.Actor:K2_GetActorRotation','/Script/Engine.Actor:GetVelocity','/Script/Engine.Actor:IsActorBeingDestroyed','/Script/Engine.SceneComponent:K2_GetComponentToWorld','/Script/Engine.SceneComponent:IsVisible'})do
   local fn=StaticFindObject(path);assert(valid(fn),path);local p=fields(fn);args[#args+1]=fn:GetAddress();args[#args+1]=address(p.ReturnValue,path:find(':Is',1,true)and'BoolProperty'or'StructProperty')
  end
  local transform=fields(StaticFindObject('/Script/CoreUObject.Transform'));for _,name in ipairs({'Rotation','Translation','Scale3D'})do args[#args+1]=address(transform[name],'StructProperty')end
  local result=native('I'..table.concat(args,' '));assert(result==1,'capture initialization '..tostring(result));initialized=true
 end
 function M.register(r)
  if not native then return false end
  if not initialized then
   local ok,err=pcall(initialize)
   if not ok then M.error=tostring(err);M.available=false;native=nil;return false end
  end
  local expired=false
  local ok,err=pcall(function()
   assert(valid(r.e),'invalid capture actor');local mesh=r.e.Mesh;assert(valid(mesh),'invalid capture mesh')
   local token=native('R',r.e:GetAddress(),mesh:GetAddress());if token==0 then expired=true;return end
   assert(type(token)=='number'and token>1,'capture registration '..tostring(token));r.stateCaptureToken=token;r.stateCaptureMesh=mesh;M.registrations=M.registrations+1
  end)
  -- A streamed-out actor or replaced component is local to this record. Do not
  -- disable the entire native path for the rest of the session because of it.
  if not ok then M.rejected=M.rejected+1;M.lastRejection=tostring(err);return false end
  if expired then return false,true end;return true
 end
 -- Native returns owned scalar values. Reuse the record's numeric snapshot,
 -- retaining exact result-count checks before touching the previous snapshot.
 local function store(r,a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,s,t,u,v,w,x,y,z,last)
  local out=r.stateCaptureValues;if not out then out={};r.stateCaptureValues=out end
  out[1],out[2],out[3],out[4],out[5],out[6],out[7],out[8],out[9],out[10]=a,b,c,d,e,f,g,h,i,j
  out[11],out[12],out[13],out[14],out[15],out[16],out[17],out[18],out[19],out[20]=k,l,m,n,o,p,q,s,t,u
  out[21],out[22],out[23],out[24],out[25],out[26]=v,w,x,y,z,last
  M.actors=M.actors+1;return out
 end
 local function accept(r,...)
  local status=(...)
  if status==1 and select('#',...)==26 then return store(r,...)end
  if status==0 then M.expired=M.expired+1;return false end
  if status==-2 then M.refreshes=M.refreshes+1;r.stateCaptureToken=nil;r.stateCaptureMesh=nil;return nil end
  M.error='capture result '..tostring(status);M.fallbacks=M.fallbacks+1;return nil
 end
 function M.capture(r)
  if not native then M.fallbacks=M.fallbacks+1;return nil end
  if not r.stateCaptureToken then
   local registered,expired=M.register(r)
   if not registered then if expired then M.expired=M.expired+1;return false end;M.fallbacks=M.fallbacks+1;return nil end
  end
  M.calls=M.calls+1;return accept(r,native('S',r.stateCaptureToken))
 end
 function M.forget(r)if native and r.stateCaptureToken then native('F',r.stateCaptureToken)end;r.stateCaptureToken=nil;r.stateCaptureMesh=nil end
 function M.reset()if native then native('X')end;initialized=nil end
 function M.status()return string.format('captureBatch=%s captureCalls=%d captureActors=%d captureRegistrations=%d captureRejected=%d captureFallbacks=%d captureExpired=%d captureRefreshes=%d captureError=%s',tostring(M.available),M.calls,M.actors,M.registrations,M.rejected,M.fallbacks,M.expired,M.refreshes,(M.error or'none'):gsub('[\r\n ]','_'):sub(1,240))end
 return M
end
