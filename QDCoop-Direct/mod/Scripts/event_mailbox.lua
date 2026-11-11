-- Append-only event journal. Read only the unconsumed tail, with a per-tick
-- byte/record/time budget. An incomplete final line stays for the next poll.
return function(path,consume,clock,report,receive,publish,Counts)
 local M={offset=0,processed=0,errors=0,pending=0,quarantined=0};local buffer='';local retries,failed={},{}
 local head,tail,size,bytes=1,0,0,0;local maxRecords,maxBytes=4096,4*1024*1024
 local function push(r)tail=tail%maxRecords+1;assert(not retries[tail]);retries[tail]=r;size=size+1;bytes=bytes+#r.line end
 local function pop()local r=retries[head];retries[head]=nil;head=head%maxRecords+1;size=size-1;bytes=bytes-#r.line;return r end
 local incoming,attempts,retryCounts,exceptions={},{},{},{}
 local function attempt(line)
  if Counts then Counts.add(attempts,line,true)end
  local ok,result=pcall(consume,line)
  if ok and result=='retry'then if Counts then Counts.add(retryCounts,line,true)end;return false end
  if not ok then
   if Counts then Counts.add(exceptions,line,true)end
   M.errors=M.errors+1;M.quarantined=M.quarantined+1;report('event',result)
   failed[#failed+1]=string.format('t=%.3f error=%s event=%s',clock(),tostring(result):gsub('[\r\n]',' '),line)
   if #failed>64 then table.remove(failed,1)end
   if publish then publish('event-failures.txt','Not replayed automatically: the handler may already have produced a side effect.\n'..table.concat(failed,'\n')..'\n')end
  end
  M.processed=M.processed+1;return true
 end
 function M.poll()
  if receive then if #buffer<65536 then buffer=buffer..receive()end end
  local s=buffer
  local start=clock();local at=1;local count=0
  -- Retry only when the handler explicitly confirms it is safe. Arbitrary
  -- exceptions are retained for diagnosis, never blindly replayed as attacks.
  -- Bounded round-robin inspection: an unresolved resource cannot monopolize
  -- the retry budget or stop unrelated arrivals at the old 128-entry threshold.
  for i=1,math.min(size,64)do local r=pop()
   if clock()>=r.due then
    count=count+1
    if not attempt(r.line)then r.failures=r.failures+1;r.due=clock()+math.min(2,.1*2^math.min(5,r.failures));push(r)end
    if count>=8 or clock()-start>=.001 then break end
   else push(r)end
  end
  while count<32 do
   -- Actual capacity exhaustion applies backpressure, never silently discards
   -- a reliable transaction. Leave ownership in the buffer/native journal.
   if size>=maxRecords or bytes+1200>maxBytes or clock()-start>=.003 then break end
   local finish=s:find('\n',at,true);if not finish then break end
   local line=s:sub(at,finish-1);M.offset=M.offset+finish-at+1;at=finish+1;count=count+1
   if Counts then Counts.add(incoming,line,true)end
   if #line<=1200 then
    if not attempt(line)then push({line=line,due=clock()+.1,failures=0,born=clock()})end
   else M.errors=M.errors+1 end
   if clock()-start>=.003 then break end
  end
  buffer=s:sub(at)
  M.pending=size;M.retryBytes=bytes;M.bufferBytes=#buffer;M.saturated=size>=maxRecords or bytes+1200>maxBytes
 end
 function M.status()
  local oldest=0;local t=clock();for _,r in pairs(retries)do oldest=math.max(oldest,t-r.born)end
  local status=string.format('eventRetryPending=%d eventRetryBytes=%d eventBufferBytes=%d eventOldestRetrySeconds=%.3f eventBackpressure=%s',size,bytes,#buffer,oldest,tostring(M.saturated==true))
  if not Counts then return status end
  return status..' '..Counts.status('eventRecv',incoming)..' '..Counts.status('eventAttempt',attempts)..' '..Counts.status('eventRetry',retryCounts)..' '..Counts.status('eventException',exceptions)
 end
 return M
end
