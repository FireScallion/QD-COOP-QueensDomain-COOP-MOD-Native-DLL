-- Coalesce repeated checks only within one synchronous game-thread operation.
-- No boolean validity result survives a call. Optional weak identities survive
-- frames, but UE checks their index/serial on every use, including nested hooks.
return function(raw,full,defaultValidator)
 local ks=StaticFindObject('/Script/Engine.Default__KismetSystemLibrary')
 local paths=setmetatable({},{__mode='k'});local checked,names;local backend=defaultValidator;local generation=0
 local M={}
 -- Reuse storage, never validity results. Separate levels preserve reentrant hooks.
 local pool,depth={},0
 local function empty(t)if t then for k in pairs(t)do t[k]=nil end end end
 function M.clear()generation=generation+1;empty(checked);empty(names)end
 function M.full(o)
  if not names then return full(o)end
  if not raw(o)then return ''end
  local address=o:GetAddress();local name=names[address]
  if name==nil then name=full(o);names[address]=name end
  return name
 end
 function M.valid(o)
  if backend then
   local result=backend.check(o)
   if result~=nil then return result end
  end
  if not raw(o)then return false end
  local address=o:GetAddress()
  if checked and checked[address]then return true end
  local path=paths[o]
  if path then local live=StaticFindObject(path);if not raw(live)or live:GetAddress()~=address then return false end
  else path=M.full(o):match(' (.+)$');if path then paths[o]=path end end
  local ok=raw(o:GetClass())and ks:IsValid(o)
  if ok and checked then checked[address]=true end
  if ok and backend then backend.register(o)end
  return ok
 end
 local function finish(ok,...)
  local context=pool[depth]
  empty(checked);empty(names)
  checked,names,backend=context.previous,context.previousNames,context.previousBackend
  if context.started~=generation then empty(checked);empty(names)end
  context.previous=nil;context.previousNames=nil;context.previousBackend=nil
  depth=depth-1
  if not ok then error((...),0)end
  return ...
 end
 function M.wrap(fn,validator)
  return function(...)
   depth=depth+1;local context=pool[depth]
   if not context then context={checked={},names={}};pool[depth]=context end
   context.previous,context.previousNames,context.previousBackend,context.started=checked,names,backend,generation
   checked,names,backend=context.checked,context.names,validator or defaultValidator
   return finish(pcall(fn,...))
  end
 end
 return M
end
