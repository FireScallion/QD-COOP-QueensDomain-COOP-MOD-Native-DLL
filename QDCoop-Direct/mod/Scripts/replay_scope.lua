-- Restore the previous state even when nested native/Blueprint calls throw.
return function()
 local M={active=false}
 function M.run(fn,...)
  local previous=M.active;M.active=true
  local result=table.pack(pcall(fn,...));M.active=previous
  if not result[1]then error(result[2],0)end
  return table.unpack(result,2,result.n)
 end
 return M
end
