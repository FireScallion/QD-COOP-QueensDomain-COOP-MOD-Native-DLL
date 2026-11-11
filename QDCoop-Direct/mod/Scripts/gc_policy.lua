local M={mode='engine',enabled=false}
function M.apply(enabled,log)
 if enabled==false then if log then log('Lua GC uses engine defaults')end;return M end
 -- One startup change in this Mod's Lua state. Do not force a full collection
 -- per frame or move engine-owned finalizers onto a background thread.
 local ok,previous=pcall(collectgarbage,'generational',20,100)
 if ok then M.enabled=true;M.mode='generational';M.previous=previous
 else M.error=tostring(previous)end
 if log then log('Lua GC mode='..M.mode..' previous='..tostring(M.previous)..' error='..tostring(M.error or 'none'))end
 return M
end
return M
