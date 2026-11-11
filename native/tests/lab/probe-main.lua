local src=debug.getinfo(1,'S').source:gsub('^@','')
local dir=assert(src:match('^(.*[/\\])'))
local C=dofile(dir..'config.lua')
local pending=false
local n=0
local function gameThread(fn)fn()end
local requestEnv=setmetatable({ExecuteWithDelay=function(ms,fn)ExecuteInGameThreadWithDelay(ms,fn)end},{__index=_G})
local function valid(o) return o and o:IsValid() end
local function log(s) print('[QDCoopProbe] '..tostring(s)..'\n') end
local function put(name,s) local f=assert(io.open(C.Root..'/'..name,'wb'));f:write(s);f:close() end
LoopInGameThreadWithDelay(500,function()
 if pending then return end
 pending=true
 gameThread(function()
  local ok,err=pcall(function()
   n=n+1
   if n==1 then
    local lines={}
    for _,p in ipairs({'/Script/Engine.Default__KismetSystemLibrary','/Script/Engine.Default__BlueprintPathsLibrary','/Script/OnlineSubsystemUtils.IpNetDriver','/Script/OnlineSubsystemSteam.SteamNetDriver'}) do
     local o=StaticFindObject(p);lines[#lines+1]=p..' valid='..tostring(valid(o))
     if valid(o) and p:find('Default__') then
      o:GetClass():ForEachFunction(function(fn) local name=fn:GetFullName();if name:lower():find('dir') or name:lower():find('commandline') then lines[#lines+1]=name end end)
     end
    end
    local k=StaticFindObject('/Script/Engine.Default__KismetSystemLibrary')
    for _,fn in ipairs({'GetProjectSavedDirectory','GetProjectDirectory','GetCommandLine'}) do
     local ok,v=pcall(function() return k[fn](k):ToString() end)
     lines[#lines+1]=fn..' => '..tostring(ok)..' '..tostring(v)
    end
    put('isolation.txt',table.concat(lines,'\n')); log(table.concat(lines,'\n'))
   end
   local f=io.open(C.Root..'/request.lua','rb')
   if f then
    local s=f:read('a');f:close();os.remove(C.Root..'/request.lua')
    local fn,e=load(s,'LocalCoopProbe','t',requestEnv);local ok,result=false,e
    if fn then ok,result=pcall(fn) end
    put('response.txt',tostring(ok)..'\n'..tostring(result));log('request '..tostring(ok)..' '..tostring(result))
   end
   if n%10==0 then put('heartbeat.txt',tostring(n)) end
  end)
  if not ok then log(err) end
  pending=false
 end)
end)
log('isolated probe started')
