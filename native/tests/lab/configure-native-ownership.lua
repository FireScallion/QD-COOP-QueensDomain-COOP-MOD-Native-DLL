local M=assert(M1)
local function valid(o)return o and o:IsValid()end
local function find(path)local o=StaticFindObject(path);assert(valid(o),path);return o end
local function fields(c)local r={};while valid(c)do c:ForEachProperty(function(p)local n=p:GetFName():ToString();if not r[n]then r[n]=p end end);c=c:GetSuperStruct()end;return r end
local names={'/Script/Engine.Actor:IsActorTickEnabled','/Script/Engine.Actor:GetActorEnableCollision','/Script/Engine.Actor:SetActorEnableCollision','/Script/Engine.ActorComponent:IsComponentTickEnabled','/Script/Engine.Pawn:GetController','/Script/Engine.MovementComponent:StopMovementImmediately','/Script/Engine.Controller:StopMovement','/Script/AIModule.BrainComponent:StopLogic','/Script/AIModule.BrainComponent:RestartLogic','/Script/AIModule.BrainComponent:IsRunning'}
local args={};for _,n in ipairs(names)do args[#args+1]=find(n):GetAddress()end
local enemy=assert(M.entities[2]).e;local los=enemy.EnemyLineOfSight:GetClass();local ai=find('/Script/AIModule.AIController')
for _,c in ipairs({find('/Script/Engine.ActorComponent'),find('/Script/Engine.MovementComponent'),ai,find('/Script/AIModule.BrainComponent'),los})do args[#args+1]=c:GetAddress()end
local props=fields(enemy:GetClass())
for _,name in ipairs({'CharacterMovement','ActorUROManager','EnemyLineOfSight','bIsAIDisabled'})do args[#args+1]=assert(props[name],name):GetAddress()end
args[#args+1]=assert(fields(ai).BrainComponent):GetAddress();args[#args+1]=assert(fields(los).bEnabled):GetAddress()
local audio=find('/Script/Engine.AudioComponent');args[#args+1]=audio:GetAddress();args[#args+1]=find('/Script/Engine.AudioComponent:SetPaused'):GetAddress();args[#args+1]=find('/Script/Engine.AudioComponent:GetPlayState'):GetAddress()
assert(#args==24);assert(M.native('C '..table.concat(args,' '))==1,'native ownership metadata rejected')
M.takeNativeOwnership=function(id)
 local r=assert(M.entities[id]);local components={}
 for _,entry in ipairs(r.e:K2_GetComponentsByClass(audio))do local c=entry:get();if valid(c)then components[#components+1]=c:GetAddress()end end
 assert(#components<=16,'too many audio components')
 local a={id,r.generation or M.generation,#components};for _,address in ipairs(components)do a[#a+1]=address end
 return M.native('F '..table.concat(a,' '))
end
return 'native ownership functions and property layouts configured'


