-- Native game startup. Never wait on a process or a socket on the game thread.
local dir=assert(debug.getinfo(1,'S').source:gsub('^@',''):match('^(.*[/\\])'))
local lookup=assert(package.loadlib(dir..'qdread.dll','qdread'))
local boot=assert(package.loadlib(dir..'qdboot.dll','qdboot'))
assert(lookup('T')=='A\0B','QDCoop native lookup ABI unavailable')
local owner,attempts,handle=nil,0,nil
local configPath=dir..'../../../../../../../QDCoop-Direct/Local/native/config.lua'
handle=LoopInGameThreadWithDelay(100,function()
 if lookup('H')~=1 then return end
 if not owner then
  local ks=StaticFindObject('/Script/Engine.Default__KismetSystemLibrary')
  if not ks or not ks:IsValid() then return end
  local saved=ks:GetProjectSavedDirectory():ToString():gsub('\\','/'):gsub('/+$','')
  owner=boot(saved)
  if not owner or owner:find('ERROR',1,true)then CancelDelayedAction(handle);print('[QDCoop] '..tostring(owner)..'\n');return end
 end
 attempts=attempts+1
 local ok,C=pcall(dofile,configPath)
 if ok and type(C)=='table'and C.RunOwner==owner then
  CancelDelayedAction(handle)
  _G.QDCoopNativeConfig=C
  local started,err=pcall(dofile,dir..'runtime_main.lua')
  if not started then print('[QDCoop] Native startup failed: '..tostring(err)..'\n')end
 elseif attempts>=1200 then
  CancelDelayedAction(handle)
  print('[QDCoop] Session initialization failed. See QDCoop-Direct/Local/native/bootstrap.log. Only one game per install can use co-op.\n')
 end
end)