-- Only dispatch/marshalling changes. Full scale and the original public K2
-- movement calls are preserved; unsupported layouts retain the old Lua path.
return function(dir,enabled)
 local M={actorCalls=0,meshCalls=0,fallbacks=0};local native
 local ok,err=pcall(function()
  assert(enabled~=false,'disabled by configuration');native=assert(package.loadlib(dir..'qdplacement.dll','qdplacement'))
  local function fields(fn)local t={};fn:ForEachProperty(function(p)t[p:GetFullName():match('([^:]+)$')]=p:GetAddress()end);return t end
  local a=StaticFindObject('/Script/Engine.Actor:K2_SetActorLocationAndRotation');local b=StaticFindObject('/Script/Engine.SceneComponent:K2_SetWorldTransform');local x=fields(a);local y=fields(b);local t=fields(StaticFindObject('/Script/CoreUObject.Transform'))
  local result=native('I'..table.concat({a:GetAddress(),StaticFindObject('/Script/Engine.Actor'):GetAddress(),x.NewLocation,x.NewRotation,x.bSweep,x.SweepHitResult,x.bTeleport,x.ReturnValue,b:GetAddress(),StaticFindObject('/Script/Engine.SceneComponent'):GetAddress(),y.NewTransform,y.bSweep,y.SweepHitResult,y.bTeleport,t.Rotation,t.Translation,t.Scale3D},' '));assert(result=='1',result)
 end)
 if not ok then native=nil;M.error=tostring(err)end;M.available=native~=nil
 function M.actor(e,v)
  if native and native('A',e:GetAddress(),v[1],v[2],v[3],v[5],v[4],v[6])=='1'then M.actorCalls=M.actorCalls+1;return true end
  M.fallbacks=M.fallbacks+1;return false
 end
 function M.mesh(p,v)
  if native and native('M',p:GetAddress(),table.unpack(v,1,10))=='1'then M.meshCalls=M.meshCalls+1;return true end
  M.fallbacks=M.fallbacks+1;return false
 end
 function M.status()return string.format('placementBatch=%s placementActorCalls=%d placementMeshCalls=%d placementFallbacks=%d placementInitError=%s',tostring(M.available),M.actorCalls,M.meshCalls,M.fallbacks,(M.error or'none'):gsub('[\r\n ]','_'):sub(1,180))end
 return M
end
