local M={}
function M.parse(s)
 local ip,port=s:match('^(%d+%.%d+%.%d+%.%d+)|(%d+)%s*$');if not ip then return end
 for n in ip:gmatch('%d+')do if tonumber(n)>255 then return end end
 local p=tonumber(port);if p<1024 or p>65535 then return end;return ip,tostring(p)
end
function M.load(file)local f=io.open(file,'rb');if not f then return end;local s=f:read(256);f:close();return M.parse(s or'')end
function M.save(file,ip,port)
 if not M.parse(ip..'|'..port)then return false end
 local f=io.open(file,'wb');if not f then return false end;local ok=f:write(ip..'|'..port);f:close();return ok~=nil
end
return M
