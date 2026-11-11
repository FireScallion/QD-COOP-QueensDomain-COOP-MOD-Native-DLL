return function(dir,enabled,inline)
 local M={enabled=true,checks=0,registered=0,expired=0,fallbacks=0};local handles=setmetatable({},{__mode='k'});local native
 local ok,err=pcall(function()assert(enabled~=false,'disabled');native=assert(package.loadlib(dir..'qdread.dll','qdread'));assert(native('T')=='A\0B','binary ABI')end)
 if not ok then native=nil;M.error=tostring(err)end;M.available=native~=nil
 M.inline=native~=nil and inline~=false and native('U')=='weak-inline-64'
 function M.checkToken(token)
  if not native or not M.enabled or not token then return end
  M.checks=M.checks+1;local v=native(M.inline and'L'or'V',token)
  if v==1 then return true elseif v==0 then M.expired=M.expired+1;return false end
  M.fallbacks=M.fallbacks+1
 end
 function M.check(o)return M.checkToken(handles[o])end
 -- Only call after the existing fresh path/address/engine validity checks.
 function M.register(o)
  if not native or not M.enabled or handles[o]then return end
  local token=native(M.inline and'W'or'R',o:GetAddress());if type(token)=='number'and token>0 then handles[o]=token;M.registered=M.registered+1 else M.fallbacks=M.fallbacks+1 end
 end
 -- Capture only inside a native callback or after fresh engine validation.
 function M.capture(o)M.register(o);return handles[o]end
 function M.status()return string.format('presentWeak=%s presentWeakEnabled=%s presentWeakChecks=%d presentWeakRegistered=%d presentWeakExpired=%d presentWeakFallbacks=%d presentWeakError=%s presentWeakMode=%s',tostring(M.available),tostring(M.enabled),M.checks,M.registered,M.expired,M.fallbacks,(M.error or'none'):gsub('[\r\n ]','_'):sub(1,120),M.inline and'inline'or'pooled')end
 return M
end
