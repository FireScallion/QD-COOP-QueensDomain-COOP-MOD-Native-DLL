local M=assert(M1);assert(M.role=='client','receipt lab uses receiving character')
local function valid(o)return o and o:IsValid()end
local function find(path)local o=StaticFindObject(path);assert(valid(o),path);return o end
local function fields(c)local r={};while valid(c)do c:ForEachProperty(function(p)local n=p:GetFName():ToString();if not r[n]then r[n]=p end end);c=c:GetSuperStruct()end;return r end
local save=FindFirstOf('FS_GameInstance_C').ActiveSaveSlot
local addresses={find('/Game/Blueprints/SaveSystem/FS_SaveSlot.FS_SaveSlot_C:ReadNamedData'),find('/Script/Engine.Default__GameplayStatics'),find('/Script/Engine.GameplayStatics:SaveGameToSlot'),find('/Script/Engine.GameplayStatics:LoadGameFromSlot'),assert(fields(save:GetClass()).NamedSaveData)}
for i,o in ipairs(addresses)do addresses[i]=o:GetAddress()end
-- Isolated test profile only. Production slot/character selection is separate.
local command='j '..table.concat(addresses,' ')..' |World1|'..M.root..'client/profile/Saved/SaveGames/World1.sav|'..(M.receiptJournalDirectory or M.root..'../../m6/journals')
assert(M.native(command)==1,'receipt configuration failed: '..M.native('S'))
return 'native receipt journal bound to isolated World1; '..M.native('S')
