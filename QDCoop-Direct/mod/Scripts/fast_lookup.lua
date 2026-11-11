-- Private QDCoop lookup environment. Never replace another mod's globals.
return function(dir,parent)
 local native=assert(package.loadlib(dir..'qdread.dll','qdread'))
 assert(native('T')=='A\0B','QDCoop native lookup ABI unavailable')
 local empty=parent.CreateInvalidObject()
 local M={calls=0,found=0,missing=0,unavailable=0,unsupported=0,fullScans=0}
 function M.find(path,...)
  M.calls=M.calls+1
  -- QDCoop uses complete paths only. Unsupported overloads never silently scan.
  if select('#',...)~=0 or type(path)~='string' or #path==0 or #path>=8191 or path:find('\0',1,true) then
   M.unsupported=M.unsupported+1;return empty
  end
  local value=native('Q'..path)
  if type(value)=='userdata' or type(value)=='table' then M.found=M.found+1;return value end
  if value==0 then M.missing=M.missing+1 else M.unavailable=M.unavailable+1 end
  return empty
 end
 function M.status()
  return string.format('fastLookup=true calls=%d found=%d missing=%d unavailable=%d unsupported=%d fullScanFallbacks=0',M.calls,M.found,M.missing,M.unavailable,M.unsupported)
 end
 local env=setmetatable({StaticFindObject=M.find,QDFastLookup=M},{__index=parent})
 env._G=env
 env.dofile=function(path)return assert(loadfile(path,'t',env))()end
 return env
end
