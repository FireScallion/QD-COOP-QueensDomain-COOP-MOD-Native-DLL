-- Optional, bounded callback-body attribution. Native entry/exit marshalling is
-- outside these timers; do not label the sums as total engine/UE4SS overhead.
return function(clock,read,write,perf)
 local T={enabled=false,registered=0,active=0};local stats,stack={},nil;local live={};local inside,outside=0,0
 local run=os.date('!%Y%m%dT%H%M%SZ');local at,nextPoll,flushAt,part,windows=nil,0,nil,0,{}
 local function flush(now)
  if not at then return end
  local header=string.format('WINDOW run=%s qpc=%.6f elapsed=%.6f registered=%d activeCallbacks=%d units=ms inclusive=true exclusive=subtract-nested-traced-hooks schema=2 outerInsideCallbackMs=%.6f outerOutsideCallbackMs=%.6f nativeEntryMeasured=false',run,now,now-at,T.registered,T.active,inside,outside)
  local rows={}
  for label,s in pairs(stats)do rows[#rows+1]=string.format('%s calls=%d inclusiveMs=%.6f selfMs=%.6f maxMs=%.6f errors=%d',label,s.n,s.total,s.self,s.max,s.errors)end
  table.sort(rows);rows[#rows+1]='ENDWINDOW';windows[#windows+1]=header..'\n'..table.concat(rows,'\n')
  write(string.format('perf-hooks-%02d.txt',part),table.concat(windows,'\n')..'\n')
  if #windows>=10 then windows={};part=(part+1)%12 end
  stats={};inside=0;outside=0;at=now;flushAt=now+5
 end
 function T.poll(now)
  if now<nextPoll then return end;nextPoll=now+1
  local expires=tonumber(read('perf-hooks-request.txt'))or 0
  -- A killed recorder cannot leave expensive diagnostics on indefinitely.
  local active=expires>os.time()and expires<=os.time()+120
  if T.enabled and(not active or now>=(flushAt or 0))then flush(now)end
  if active and not T.enabled then stats={};inside=0;outside=0;at=now;flushAt=now+5 end
  T.enabled=active
 end
 function T.wrap(fn,label)
  if not fn then return nil end
  return function(...)
   if not T.enabled then return fn(...)end
   local parent=stack;local call={child=0};stack=call;local inCallback=perf and perf.callbackActive
   local start=clock();local result=table.pack(pcall(fn,...));local elapsed=(clock()-start)*1000
   stack=parent;if parent then parent.child=parent.child+elapsed elseif inCallback then inside=inside+elapsed else outside=outside+elapsed end
   local s=stats[label];if not s then s={n=0,total=0,self=0,max=0,errors=0};stats[label]=s end
   s.n=s.n+1;s.total=s.total+elapsed;s.self=s.self+math.max(0,elapsed-call.child);s.max=math.max(s.max,elapsed)
   if not result[1]then s.errors=s.errors+1;error(result[2],0)end
   return table.unpack(result,2,result.n)
  end
 end
 function T.install()
  local original,remove=RegisterHook,UnregisterHook
  local function key(path,a,b)return path..':'..tostring(a)..':'..tostring(b)end
  RegisterHook=function(path,pre,post)
   local a,b=original(path,T.wrap(pre,path..'/pre'),T.wrap(post,path..'/post'))
   local n=(pre and 1 or 0)+(post and 1 or 0);local k=key(path,a,b)
   T.registered=T.registered+n;T.active=T.active+n-(live[k]or 0);live[k]=n;return a,b
  end
  UnregisterHook=function(path,a,b)
   local result=table.pack(remove(path,a,b));local k=key(path,a,b)
   if live[k]then T.active=T.active-live[k];live[k]=nil end
   return table.unpack(result,1,result.n)
  end
 end
 return T
end
