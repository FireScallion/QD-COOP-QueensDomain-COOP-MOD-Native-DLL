-- Two alternating, checksummed snapshots. A process killed during a write leaves
-- the preceding committed copy intact. No executable Lua is read from disk.
local M={}
function M.checksum(s)
 local a,b=1,0
 for i=1,#s do a=(a+s:byte(i))%65521;b=(b+a)%65521 end
 return string.format('%.0f',b*65536.0+a)
end
local function escape(s)return(tostring(s):gsub('([^%w_%.:%-~])',function(c)return string.format('%%%02X',c:byte())end))end
local function unescape(s)
 if s:find('%%[^%x]')or s:find('%%$')or s:find('%%%x$')then return end
 return(s:gsub('%%(%x%x)',function(h)return string.char(tonumber(h,16))end))
end
function M.encode(generation,state)
 local keys={};for k in pairs(state)do keys[#keys+1]=k end;table.sort(keys)
 local lines={'QDR2',tostring(generation)}
 for _,k in ipairs(keys)do assert(type(state[k])=='string'and #k<=200 and #state[k]<=1000,'Invalid rescue record');lines[#lines+1]=escape(k)..'|'..escape(state[k])end
 local body=table.concat(lines,'\n')..'\n';assert(#body<=262144,'Rescue journal capacity exceeded')
 return body..'END|'..#body..'|'..M.checksum(body)..'\n'
end
function M.decode(text)
 if not text or #text>263000 then return end
 local body,len,hash=text:match('^(QDR2\n.-\n)END|(%d+)|(%d+)\n$')
 if not body or tonumber(len)~=#body or M.checksum(body)~=hash then return end
 local gen=tonumber(body:match('^QDR2\n(%d+)\n'));if not gen or gen<1 or gen>2e9 then return end
 local state={};local first=true
 for line in body:sub(6):gmatch('[^\n]+')do
  if first then first=false else
   local k,v=line:match('^([^|]+)|([^|]*)$');if not k then return end;k,v=unescape(k),unescape(v)
   if not k or not v or #k>200 or #v>1000 or state[k]~=nil then return end;state[k]=v
  end
 end
 return gen,state
end
local function read(path)
 local f,err,code=io.open(path,'rb')
 if not f then assert(code==2,err);return end
 local s,why=f:read(263001);f:close();assert(s,why);return s
end
local function write(path,text)
 local f=assert(io.open(path,'wb'));local ok,err=f:write(text)
 if ok then ok,err=f:flush()end;local closed,why=f:close();assert(ok,err);assert(closed,why)
end
function M.open(path,initial,fs)
 fs=fs or{read=read,write=write};local S={generation=0,state={},uncertain=false}
 function S.refresh()
  local a,b=fs.read(path..'.a'),fs.read(path..'.b');local ga,sa=M.decode(a);local gb,sb=M.decode(b)
  if not ga and not gb then
   assert(a==nil and b==nil,'Both rescue journal copies are invalid; preserving files for recovery')
   S.generation=0;S.state={};for k,v in pairs(initial or{})do S.state[k]=tostring(v)end
  elseif ga and(not gb or ga>=gb)then S.generation,S.state=ga,sa
  else S.generation,S.state=gb,sb end
  S.uncertain=false
 end
 function S.get()return S.state end
 function S.ensure()if S.uncertain then S.refresh()end end
 function S.change(fn)
  S.ensure();local old=S.get();local next={};for k,v in pairs(old)do next[k]=v end;fn(next)
  local generation=S.generation+1;local data=M.encode(generation,next);local file=path..(generation%2==1 and'.a'or'.b')
  -- If a write or verification throws after reaching disk, reload before the
  -- next transition rather than assuming it either definitely failed or worked.
  S.uncertain=true;fs.write(file,data);local g=M.decode(fs.read(file));assert(g==generation,'Rescue journal readback failed')
  S.generation,S.state,S.uncertain=g,next,false;return next
 end
 S.refresh();return S
end
return M
