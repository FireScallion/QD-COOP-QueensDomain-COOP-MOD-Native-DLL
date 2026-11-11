-- A missing cosmetic object or transient mailbox error must not disable all
-- co-op or terminate the native game. Retry only the failed subsystem.
return function(log,write,clock)
 local M={states={}}
 function M.report(key,err)
  local s=M.states[key]or{failures=0,total=0};M.states[key]=s
  s.failures=s.failures+1;s.total=s.total+1;s.next=clock()+math.min(2,.1*2^math.min(5,s.failures-1));s.error=tostring(err)
  if not s.logged or clock()-s.logged>=5 then
   s.logged=clock();log('RECOVERABLE '..key..' '..s.error)
   pcall(write,'recoverable-error.txt',key..' failures='..s.total..' '..s.error..'\n')
  end
 end
 function M.call(key,fn,...)
  local s=M.states[key];if s and clock()<s.next then return false end
  local ok,value=pcall(fn,...)
  if not ok then M.report(key,value);return false end
  if s then s.failures=0;s.next=0 end
  return true,value
 end
 return M
end
