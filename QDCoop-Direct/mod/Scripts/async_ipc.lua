-- Only owned strings cross the native worker boundary. No async Lua VM access.
return function(dir,root,channel)
 local Counts=dofile(dir..'event_counts.lua');local emitted={}
 local call=assert(package.loadlib(dir..'qdipc.dll','qdipc'))
 assert(call('S'..root..(channel and channel~=''and('\n'..channel)or''))=='1','QDCoop asynchronous mailbox failed to start')
 local cache,pending,watched={},{},{}
 local function queue()return {head=1,tail=0,bytes=0,peak=0,dropped=0}end
 local critical,cosmetic=queue(),queue()
 local function pop(q)local e=q[q.head];q[q.head]=nil;q.head=q.head+1;q.bytes=q.bytes-#e[2];if q.head>q.tail then q.head=1;q.tail=0 end end
 local M={}
 local function watch(name)
  local s=call('R'..name)
  if s=='='or(s and s:sub(1,1)=='1')then
   watched[name]=true
   if s:sub(1,1)=='1'then cache[name]=s:sub(2)end
  end
 end
 for _,name in ipairs({'status.txt','in.state','in.visual','in.enemy','in.roster','in.world','session.ready','session.error'})do watch(name)end
 function M.read(name)
  if not watched[name]then watch(name)end
  return cache[name]or''
 end
 function M.write(name,s,add)
  if add and name=='out.events'then for line in s:gmatch('[^\n]+')do Counts.add(emitted,line)end end
  -- Only the same expiring SND/VFX class used by Bridge bypasses the journal.
  -- VFX stop (R), unknown events and all gameplay transactions stay durable.
  if channel and channel~=''and add and name=='out.events'and(s:sub(1,4)=='SND|'or s:sub(1,4)=='VFX|')then
   local op=s:match('^[^|]*|[^|]*|[^|]*|([^|\n]+)');if op and op~='R'and #s<=1001 then name='out.cosmetic'end
  end
  if add then
   local q=name=='out.cosmetic'and cosmetic or critical
   -- Only expiring cosmetic observations can be evicted. Gameplay events keep
   -- FIFO ownership until native admission succeeds; a full visual lane never
   -- blocks them. No synchronous file or network wait is introduced.
   if q==cosmetic and q.tail-q.head+1>=256 then pop(q);q.dropped=q.dropped+1 end
   q.tail=q.tail+1;q[q.tail]={name,s};q.bytes=q.bytes+#s;q.peak=math.max(q.peak,q.bytes)
  else cache[name]=s;pending[name]=s end
 end
 function M.flush()
  -- Calls only try-lock the memory queue; a busy worker is retried next frame.
  local submitted=0
  for _,q in ipairs({critical,cosmetic})do
   while q.head<=q.tail and submitted<32 do
    local e=q[q.head];if call('A'..e[1]..'\n'..e[2])~='1'then break end
    pop(q);submitted=submitted+1
   end
  end
  local n=0;for name,s in pairs(pending)do if call('W'..name..'\n'..s)=='1'then pending[name]=nil end;n=n+1;if n>=12 then break end end
  -- One native boundary for all newly delivered snapshots. No engine objects
  -- cross this boundary; unknown/read-once files register lazily in M.read.
  local batch=call('B');if batch and batch~='='and batch~='0'then
   local at=1;while at<=#batch do local e=batch:find('\n',at,true);assert(e,'IPC batch name');local name=batch:sub(at,e-1);local h=batch:find('\n',e+1,true);assert(h,'IPC batch length');local len=tonumber(batch:sub(e+1,h-1));assert(len and len>=0 and h+len<=#batch,'IPC batch bounds');if pending[name]==nil then cache[name]=batch:sub(h+1,h+len)end;at=h+len+1 end
  end
 end
 function M.events()local s=call('E');return s~='='and s~='0'and s or''end
 function M.status()return call('T')..string.format(' criticalPending=%d criticalBytes=%d criticalPeak=%d cosmeticPending=%d cosmeticDropped=%d',critical.tail-critical.head+1,critical.bytes,critical.peak,cosmetic.tail-cosmetic.head+1,cosmetic.dropped)..' '..Counts.status('eventEmit',emitted)end
 return M
end
