-- Bounded slow-lookup evidence. No file I/O or further object lookup here.
return function(clock,write)
 local original=StaticFindObject;local rows,order={},{};local M={calls=0,total=0,slow=0,max=0,slowMissing=0,slowFound=0,slowUnknown=0}
 _G.StaticFindObject=function(...)
  local at=clock();local value=original(...);local elapsed=clock()-at
  M.calls=M.calls+1;M.total=M.total+elapsed;M.max=math.max(M.max,elapsed)
  if elapsed>=.008 then
   M.slow=M.slow+1
   local address;local ok=pcall(function()address=value and value:GetAddress()end)
   local result=not ok and'unknown'or(not address or address==0)and'missing'or'found'
   if result=='missing'then M.slowMissing=M.slowMissing+1 elseif result=='found'then M.slowFound=M.slowFound+1 else M.slowUnknown=M.slowUnknown+1 end
   local callers={};for level=2,6 do local info=debug.getinfo(level,'Sl');if not info then break end;callers[#callers+1]=(info.short_src or'?')..':'..(info.currentline or 0)end
   local p=select(1,...);local path=type(p)=='string'and p or tostring(select(4,...))
   local key=path..'|'..result..'|'..table.concat(callers,' <- ')
   local r=rows[key];if not r then if #order>=32 then rows[table.remove(order,1)]=nil end;r={n=0,ms=0,max=0,first=at,last=at};rows[key]=r;order[#order+1]=key end
   r.n=r.n+1;r.ms=r.ms+elapsed*1000;r.max=math.max(r.max,elapsed*1000);r.last=at
  end
  return value
 end
 function M.flush()
  local out={string.format('schema=2 qpc=%.6f calls=%d totalMs=%.3f slow8ms=%d maxMs=%.3f slowMissing=%d slowFound=%d slowUnknown=%d',clock(),M.calls,M.total*1000,M.slow,M.max*1000,M.slowMissing,M.slowFound,M.slowUnknown),'count\ttotalMs\tmaxMs\tfirstQpc\tlastQpc\tpath|result|callers'}
  for _,k in ipairs(order)do local r=rows[k];out[#out+1]=string.format('%d\t%.3f\t%.3f\t%.6f\t%.6f\t%s',r.n,r.ms,r.max,r.first,r.last,k)end
  write('lookup-cost.txt',table.concat(out,'\n'))
  if QDFastLookup then write('fast-lookup-status.txt',QDFastLookup.status())end
 end
 return M
end
