local M=assert(M1)
local function valid(o)return o and o:IsValid()end
local function find(path)local o=StaticFindObject(path);if not valid(o)then o=LoadAsset(path)end;assert(valid(o),path);return o end
local function fields(c)local r={};while valid(c)do c:ForEachProperty(function(p)local n=p:GetFName():ToString();if not r[n]then r[n]=p end end);c=c:GetSuperStruct()end;return r end
local lizard=find('/Game/Enemies/Lizer_00/Default_Lizer_01.Default_Lizer_01_C')
local kill='/Game/Enemies/EnemyBase.EnemyBase_C:Kill'
local args={find(kill):GetAddress(),find('/Script/Engine.Actor:SetActorTickEnabled'):GetAddress(),find('/Script/Engine.ActorComponent:SetComponentTickEnabled'):GetAddress(),assert(fields(find('/Script/Engine.Actor')).CustomTimeDilation):GetAddress(),lizard:GetAddress(),assert(fields(lizard).DefaultEnemyMode):GetAddress()}
assert(M.native('D '..table.concat(args,' '))==1,'death metadata rejected')
if not M.deathHook then
 M.deathObserved={}
 M.deathHook,M.deathPostHook=RegisterHook(kill,function(context)
  local e=context:get();if not valid(e)then return end
  local address=e:GetAddress()
  for id,r in ipairs(M.entities)do
   if valid(r.e)and r.e:GetAddress()==address then
    M.deathObserved[id]=(M.deathObserved[id]or 0)+1
    if M.role=='host'then assert(M.native('K '..address)==1,'native death enqueue failed')end
    break
   end
  end
 end)
end
return 'death metadata and temporary Kill event bridge ready'
