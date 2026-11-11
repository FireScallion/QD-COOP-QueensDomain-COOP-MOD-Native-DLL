local M=assert(M1)
local function valid(o)return o and o:IsValid()end
local function fields(c)local r={};while valid(c)do c:ForEachProperty(function(p)local n=p:GetFName():ToString();if not r[n]then r[n]=p end end);c=c:GetSuperStruct()end;return r end
local player=assert(M.player);local gi=FindFirstOf('FS_GameInstance_C');local save=gi.ActiveSaveSlot
local fn=StaticFindObject('/Game/Player/FS_PlayerChar.FS_PlayerChar_C:AddXP')
local a={player:GetClass():GetAddress(),gi:GetClass():GetAddress(),save:GetClass():GetAddress(),fn:GetAddress(),assert(fields(gi:GetClass()).ActiveSaveSlot):GetAddress(),assert(fields(save:GetClass()).XPAmount):GetAddress(),player:GetAddress(),gi:GetAddress(),save:GetAddress()}
assert(M.native('w '..table.concat(a,' '))==1,'native reward metadata rejected')
local helper=StaticFindObject('/Game/Blueprints/SaveSystem/SaveFunctions.Default__SaveFunctions_C')
local kill=StaticFindObject('/Game/Blueprints/SaveSystem/SaveFunctions.SaveFunctions_C:RegisterEnemyKill')
local dataClass=StaticFindObject('/Game/Enemies/Shared/EnemyData.EnemyData_C')
assert(M.native('b '..helper:GetAddress()..' '..kill:GetAddress()..' '..dataClass:GetAddress()..' '..assert(fields(save:GetClass()).KilledEnemyCounts):GetAddress())==1,'native bestiary metadata rejected')
M.rewardXPBefore=save.XPAmount
if M.role=='client'and not M.rewardResourceLoop then
 M.rewardResourceLoop=true
 M.rewardResources=dofile(M.root..'legacy/audio_resources.lua')(valid,function(o)return o:GetFullName()end,function()end)
 LoopInGameThreadWithDelay(200,function()
  local path=M.native('r');if type(path)~='string'or path==''then return end
  local ok,err=pcall(function()local object=M.rewardResources.load(path);assert(object:IsA('/Game/Enemies/Shared/EnemyData.EnemyData_C')or object:IsA('/Script/QueensDomain.FS_Item'));assert(M.native('a '..object:GetAddress()..' '..path)==1,'native reward asset rejected')end)
  M.rewardResourceError=not ok and tostring(err)or nil
 end)
end
if M.role=='host'and not M.rewardHook then
 M.rewardSourceCount=0
 M.bestiarySourceCount=0
 M.rewardPending={}
 local function flush()
  for _=1,8 do
   local command=M.rewardPending[1];if not command then return end
   if M.native(command)~=1 then return end
   table.remove(M.rewardPending,1)
  end
 end
 -- Temporary in-memory producer backlog: a full native outbox must not erase
 -- an opportunity that the game's original reward helper already produced.
 LoopInGameThreadWithDelay(200,function()if #M.rewardPending>0 then flush()end end)
 M.rewardHook,M.rewardPostHook=RegisterHook('/Game/Blueprints/SaveSystem/SaveFunctions.SaveFunctions_C:RegisterEnemyKill',function(_,data,setOne,onlyZero,context)
  local e=context:get();if not valid(e)or not e:IsA('/Game/Enemies/EnemyBase.EnemyBase_C')then return end
  -- Temporary event bridge; the native core owns delivery and engine execution.
  -- Bestiary and XP opportunities come from this original helper, never from
  -- an HP snapshot or from the death presentation packet.
  local path=data:get():GetFullName():match(' (/Game/[%w_/%.-]+)$');assert(path,'enemy reward data path')
  local flags=(setOne:get()and 1 or 0)+(onlyZero:get()and 2 or 0)
  M.rewardPending[#M.rewardPending+1]='k '..flags..' '..path;M.bestiarySourceCount=M.bestiarySourceCount+1
  if not setOne:get()and not onlyZero:get()and not e:ShouldSkipXPReward()then
   local xp=e:GetXPReward();if xp>0 then M.rewardPending[#M.rewardPending+1]='q '..xp;M.rewardSourceCount=M.rewardSourceCount+1 end
  end
  flush()
 end)
end
return 'native XP/bestiary metadata and authority event bridge ready; XP='..save.XPAmount
