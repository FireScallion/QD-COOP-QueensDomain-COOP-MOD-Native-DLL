-- Bounded rolling diagnostics, asynchronously published. Durations are inclusive:
-- parent/child rows MUST NOT be added together. QPC wall clock where available; os.clock fallback.
return function(write)
 local dir=debug.getinfo(1,'S').source:gsub('^@',''):match('^(.*[/\\])');local timer,clockError=package.loadlib(dir..'qdpose.dll','qdclock');local clock=timer or os.clock
 local M={clock=clock,timer=timer and 'QPC'or'os.clock',clockError=clockError,callbackActive=false};local frameStart;local stats,calls,frame,spikes={},{},{},{}
 local threadIdentity=package.loadlib(dir..'qdpose.dll','qdthread')
 local threadPublished=false
 local frameInterval=0
 local rawFrames={}
 local rawPart=0
 local rawWindows=0
 local last,flushAt,windowAt,context
 local edges={.1,.25,.5,1,2,3,4,6,8,10,12,16,20,25,33,50,75,100,200,500,1000}
 local history,index,part={},0,0
 local run=os.date('!%Y%m%dT%H%M%SZ')
 local function publishFrames(now)
  local lines={};for i,r in ipairs(rawFrames)do
   -- Rows are complete before publication. Format each once within the bounded ring.
   r.formatted=r.formatted or string.format('%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f',table.unpack(r));lines[i]=r.formatted
  end
  local header='# run='..run..' utc='..os.date('!%Y-%m-%dT%H:%M:%SZ')..' qpc='..string.format('%.6f',now)
  write(string.format('perf-frames-%02d.csv',rawPart),header..'\nqpc,engineIntervalMs,modMs,controlMs,serviceMs,presentationMs,ipcMs,callbackMeasuredMs,diagnosticsFinishMs\n'..table.concat(lines,'\n')..'\n')
 end
 function M.add(name,ms)
  local s=stats[name];if not s then s={n=0,total=0,max=0,bins={}};stats[name]=s end
  s.n=s.n+1;s.total=s.total+ms;s.max=math.max(s.max,ms)
  local bucket=#edges+1;for i,e in ipairs(edges)do if ms<=e then bucket=i;break end end;s.bins[bucket]=(s.bins[bucket]or 0)+1
  frame[name]=(frame[name]or 0)+ms
 end
 function M.begin(now)
  frameStart=now;M.callbackActive=true
  frameInterval=last and math.max(0,now-last)*1000 or 0
  if last then M.add('engineFrame',frameInterval)end
  if not threadPublished then
   threadPublished=true
   local ok,identity=pcall(threadIdentity or function()return 'unavailable'end)
   write('perf-thread.txt',string.format('run=%s utc=%s qpc=%.6f pid_tid=%s\n',run,os.date('!%Y-%m-%dT%H:%M:%SZ'),now,ok and tostring(identity)or'unavailable'))
  end
  last=now;flushAt=flushAt or now+5;windowAt=windowAt or now;for key in pairs(frame)do frame[key]=nil end
 end
 function M.measure(fn,label)
  local function finish(at,...)M.add(label,(clock()-at)*1000);return ... end
  return function(...)
   local at=clock();return finish(at,fn(...))
  end
 end
 function M.wrap(object,key,label)object[key]=M.measure(object[key],label or key)end
 function M.hook(fn,label)
  local count=0
  local sampleLabel=label..'.sample'
  local function finish(at,...)M.add(sampleLabel,(clock()-at)*1000);return ... end
  return function(...)
   calls[label]=(calls[label]or 0)+1;count=count+1
   if count%16~=1 then return fn(...)end
   local at=clock();return finish(at,fn(...))
  end
 end
 function M.setContext(fn)context=fn end
 local function finishWork(now)
  -- Bounded even with uncapped, extremely high frame rates.
  if #rawFrames>=8192 then publishFrames(now);rawFrames={};rawWindows=0;rawPart=(rawPart+1)%12 end
  if(frame.total or 0)>=12 then
   local row={time=now,cost=frame.total,parts={}}
   for _,key in ipairs({'control','service','presentation','ipc','enemy.threat','enemy.spawn','world.applySnapshot','world.cloneSave','events.poll'})do
    if frame[key]then row.parts[#row.parts+1]=string.format('%s=%.3f',key,frame[key])end
   end
   spikes[#spikes+1]=row;table.sort(spikes,function(a,b)return a.cost>b.cost end);if #spikes>8 then table.remove(spikes)end
  end
  if not flushAt or now<flushAt then return end;flushAt=now+5
  local elapsed=math.max(.001,now-windowAt);local rows={}
  for name,s in pairs(stats)do
   local p50,p95,p99,sum=nil,nil,nil,0
   for i=1,#edges+1 do sum=sum+(s.bins[i]or 0);if not p50 and sum>=s.n*.50 then p50=edges[i]or 9999 end;if not p95 and sum>=s.n*.95 then p95=edges[i]or 9999 end;if not p99 and sum>=s.n*.99 then p99=edges[i]or 9999 end end
   local unit=name:match('^enemyCount%.')and'count'or name:match('^enemyMemory%.')and'KB'or'ms'
   if unit~='ms'then rows[#rows+1]=string.format('%s samples=%d mean=%.3f max=%.3f unit=%s',name,s.n,s.total/s.n,s.max,unit)else rows[#rows+1]=string.format('%s count=%d meanMs=%.3f p50Ms<=%.2f p95Ms<=%.2f p99Ms<=%.2f maxMs=%.3f measuredMsPerSecond=%.3f',name,s.n,s.total/s.n,p50,p95,p99,s.max,s.total/elapsed)end
  end
  publishFrames(now)
  rawWindows=rawWindows+1
  if rawWindows>=10 then rawFrames={};rawWindows=0;rawPart=(rawPart+1)%12 end
  table.sort(rows);write('frame-cost.txt',table.concat(rows,'\n')..'\n')
  local ctx='';if context then local ok,v=pcall(context);ctx=ok and tostring(v)or 'contextError='..tostring(v)end
  local out={string.format('WINDOW run=%s utc=%s elapsed=%.3f monotonic=%.3f inclusive=true hookSampling=1/16 timer=%s schema=2 totalScope=workBeforeFinish callbackScope=throughDiagnosticsFinish',run,os.date('!%Y-%m-%dT%H:%M:%SZ'),elapsed,now,M.timer),ctx,table.concat(rows,'\n')}
  local names={};for name in pairs(calls)do names[#names+1]=name end;table.sort(names)
  for _,name in ipairs(names)do out[#out+1]=name..' calls='..calls[name]end
  for _,s in ipairs(spikes)do out[#out+1]=string.format('SPIKE t=%.3f totalMs=%.3f %s',s.time,s.cost,table.concat(s.parts,' '))end
  out[#out+1]='ENDWINDOW\n';history[#history+1]=table.concat(out,'\n');index=index+1
  write(string.format('perf-history-%02d.txt',part),table.concat(history,'\n'))
  if index==10 then history={};index=0;part=(part+1)%12 end
  stats={};calls={};spikes={};windowAt=now
 end
 function M.finish(now)
  local row={now,frameInterval,frame.total or 0,frame.control or 0,frame.service or 0,frame.presentation or 0,frame.ipc or 0,0,0}
  local started=clock();finishWork(now)
  -- Publish completed rows. Diagnostic context, formatting and publication
  -- belong to this callback; only final counter/field assignments are outside.
  rawFrames[#rawFrames+1]=row
  local ended=clock();row[8]=(ended-(frameStart or now))*1000;row[9]=(ended-started)*1000
  M.callbackActive=false
  M.add('callback.measured',row[8]);M.add('diagnostics.finish',row[9])
 end
 return M
end
