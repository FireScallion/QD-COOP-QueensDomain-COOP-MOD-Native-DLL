-- Lua wrappers and UObject Outer are not GC ownership. Hold settings/assets in
-- the game's reflected reference array for the lifetime of the audio session.
return function(valid,full,invalidate)
 local M={};local cache,owned={},{};local owner
 function M.hold(o)
  assert(valid(o),'Audio resource unavailable')
  local gi=FindFirstOf('FS_GameInstance_C');assert(valid(gi),'Audio resource owner unavailable')
  if owner and(not valid(owner)or full(owner)~=full(gi))then cache={};owned={}end;owner=gi
  local key=full(o);if owned[key]then return o end
  local refs={};local present=false
  gi.ReferencedObjects:ForEach(function(_,v)local a=v:get();refs[#refs+1]=a;if valid(a)and full(a)==key then present=true end end)
  if not present then refs[#refs+1]=o;gi.ReferencedObjects=refs;owned[key]=true end
  return o
 end
 function M.load(path)
  local a=cache[path];if valid(a)then return a end
  a=StaticFindObject(path)
  if not valid(a)then invalidate();a=LoadAsset(path);invalidate()end
  a=M.hold(a);cache[path]=a;return a
 end
 function M.reset()
  if valid(owner)then local refs={};owner.ReferencedObjects:ForEach(function(_,v)local a=v:get();if valid(a)and not owned[full(a)]then refs[#refs+1]=a end end);owner.ReferencedObjects=refs end
  cache={};owned={};owner=nil;invalidate()
 end
 return M
end
