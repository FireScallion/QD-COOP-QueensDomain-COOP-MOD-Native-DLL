-- Shared serial-checked identities. Lua wrappers never pin engine objects.
-- Only successful fixed class/default-object lookups are cached; no negative cache.
return function(dir,C,raw)
 local V=dofile(dir..'weak_validation.lua')(dir,true,true)
 -- Mod construction can run on UE4SS's loader thread. First weak registration
 -- must wait for the engine callback; qdread binds its owning thread on first use.
 V.enabled=false
 local cache={};local M={validation=V,hits=0,misses=0,expired=0,deferred=0,resolved=0,discarded=0,queueFallbacks=0}
 function M.activate()
  if not M.started then M.started=true;V.enabled=C.NativeObjectReferences~=false end
 end
 function M.find(path)
  local eligible=type(path)=='string'and not path:find(':',1,true)and(path:sub(1,8)=='/Script/'or path:sub(-2)=='_C')
  if not V.enabled or not V.available or not eligible then return StaticFindObject(path)end
  local old=cache[path]
  if old then local live=V.check(old);if live==true then M.hits=M.hits+1;return old end;cache[path]=nil;M.expired=M.expired+1 end
  M.misses=M.misses+1
  local o=StaticFindObject(path)
  -- StaticFindObject just returned this object on the game thread, so its
  -- pointer is fresh. Never create an identity from an unvalidated retained pointer.
  if raw(o)then V.register(o);if V.check(o)==true then cache[path]=o end end
  return o
 end
 function M.captureFresh(o)
  if V.enabled and raw(o)then local token=V.capture(o);if token then M.deferred=M.deferred+1;return token end end
 end
 function M.resolve(entry)
  if V.enabled and entry.weak then
   local live=V.checkToken(entry.weak)
   if live==true then M.resolved=M.resolved+1;return entry.c
   elseif live==false then M.discarded=M.discarded+1;return nil end
  end
  M.queueFallbacks=M.queueFallbacks+1
  local o=entry.path and StaticFindObject(entry.path)
  if raw(o)and o:GetAddress()==entry.address then return o end
 end
 function M.status()
  return string.format('objectRefs=%s fixedHits=%d fixedMisses=%d fixedExpired=%d deferredTokens=%d resolvedTokens=%d discardedTokens=%d queueFallbacks=%d ',tostring(V.enabled and V.available),M.hits,M.misses,M.expired,M.deferred,M.resolved,M.discarded,M.queueFallbacks)..V.status():gsub('presentWeak','objectWeak')
 end
 return M
end
