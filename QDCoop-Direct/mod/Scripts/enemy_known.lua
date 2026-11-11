-- A discovered actor is already a no-op in the periodic scan. Recognize that
-- same allocation by a serial-checked weak handle, not by a remembered address
-- alone. Unregistered, replaced and revived records keep the original path.
return function(dir,C,rawValid)
 local M={hits=0,misses=0,registered=0,expired=0,fallbacks=0,count=0};local entries={};local native
 local ok,err=pcall(function()native=assert(package.loadlib(dir..'qdread.dll','qdread'));assert(native('T')=='A\0B','binary ABI')end)
 if not ok then native=nil;M.error=tostring(err)end;M.available=native~=nil
 local function enabled()return native and C.NativeKnownEnemyDiscovery~=false end
 local function remove(address)if entries[address]then entries[address]=nil;M.count=M.count-1 end end
 function M.known(o,role,actors,frozen)
  if not enabled()or not rawValid(o)then return false end
  local address=o:GetAddress();local s=entries[address]
  if not s or s.role~=role then M.misses=M.misses+1;return false end
  local records=role=='host'and actors or frozen
  if records[s.name]~=s.record then remove(address);M.misses=M.misses+1;return false end
  local ok,alive=pcall(native,'V',s.token);if not ok then alive=nil end
  if alive==1 then M.hits=M.hits+1;return true end
  remove(address);if alive==0 then M.expired=M.expired+1 else M.fallbacks=M.fallbacks+1 end
  return false
 end
 -- Caller has just passed the original path/address/engine validity checks.
 function M.mark(o,name,record,role)
  if not enabled()or not record then return end
  local address=o:GetAddress();local old=entries[address]
  if old and old.record==record and old.role==role then return end
  if not old and M.count>=4096 then M.fallbacks=M.fallbacks+1;return end
  local ok,token=pcall(native,'R',address)
  if not ok or type(token)~='number'or token<=0 then M.fallbacks=M.fallbacks+1;return end
  if not old then M.count=M.count+1 end
  entries[address]={token=token,name=name,record=record,role=role};M.registered=M.registered+1
 end
 function M.prune(actors,frozen)
  for address,s in pairs(entries)do local records=s.role=='host'and actors or frozen;if records[s.name]~=s.record then remove(address)end end
 end
 function M.reset()entries={};M.count=0 end
 function M.status()return string.format('knownScan=%s knownScanEnabled=%s knownScanEntries=%d knownScanHits=%d knownScanMisses=%d knownScanRegistered=%d knownScanExpired=%d knownScanFallbacks=%d',tostring(M.available),tostring(C.NativeKnownEnemyDiscovery~=false),M.count,M.hits,M.misses,M.registered,M.expired,M.fallbacks)end
 return M
end
