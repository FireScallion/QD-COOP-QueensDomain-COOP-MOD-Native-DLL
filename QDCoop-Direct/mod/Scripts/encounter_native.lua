return function(dir,valid)
 local native=assert(package.loadlib(dir..'qdscene.dll','qdscene'))
 local M={guards={}}
 function M.guard(path)
  local fn=StaticFindObject(path);if not valid(fn)then return false end
  local old=M.guards[path]
  if old and old.address==fn:GetAddress()and native('V',old.token)==1 then return true end
  if old then assert(native('U',old.token)==1);M.guards[path]=nil end
  local token=native('G',fn:GetAddress());assert(token>0,'Encounter guard refused '..path)
  M.guards[path]={token=token,address=fn:GetAddress()};return true
 end
 function M.restore()
  for path,t in pairs(M.guards)do assert(native('U',t.token)==1,'Encounter guard restore failed '..path);M.guards[path]=nil end
 end
 function M.original(path,fn)
  local old=M.guards[path]
  if old then assert(native('U',old.token)==1);M.guards[path]=nil end
  local result=table.pack(pcall(fn))
  if old then M.guard(path)end
  if not result[1]then error(result[2])end
  return table.unpack(result,2,result.n)
 end
 local cls,prop
 function M.take(manager)
  if not valid(cls)then
   cls=StaticFindObject('/Game/Cutscenes/CutsceneManagerBase.CutsceneManagerBase_C');assert(valid(cls))
   cls:ForEachProperty(function(p)if p:GetFName():ToString()=='CutsceneFinished'then prop=p end end)
   assert(prop and prop:GetFullName():match('^MulticastInlineDelegateProperty '),'Unexpected completion delegate layout')
  end
  local t=native('D',manager:GetAddress(),cls:GetAddress(),prop:GetAddress());assert(t>0,'Cutscene completion escrow unavailable')
  local out={token=t,callbacks={}}
  for i=1,native('C',t)do
   local object,fn=native('O',t,i),native('F',t,i)
   if type(object)=='userdata'and type(fn)=='userdata'and valid(object)and valid(fn)then
    out.callbacks[#out.callbacks+1]={object=object,fn=fn:GetFName():ToString(),path=fn:GetFullName():gsub('^Function ','')}
   end
  end
  return out
 end
 function M.discard(t)if t and t.token then assert(native('X',t.token)==1);t.token=nil end end
 function M.putBack(t)if t and t.token then local r=native('R',t.token);assert(r>=0,'Unable to restore completion delegate');t.token=nil end end
 return M
end
