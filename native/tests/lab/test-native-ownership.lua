local M=assert(M1);local r=assert(M.entities[2]);local e=r.e
local controller=e:GetController();local brain=controller.BrainComponent
e.bIsAIDisabled=false;e.CustomTimeDilation=0.75;e:SetActorHiddenInGame(false);e:SetActorEnableCollision(true);e:SetActorTickEnabled(true)
e.Mesh:SetComponentTickEnabled(true);e.Mesh:SetVisibility(true,false);e.CharacterMovement:SetComponentTickEnabled(true)
e.ActorUROManager:SetComponentTickEnabled(false);e.EnemyLineOfSight.bEnabled=true;e.EnemyLineOfSight:SetComponentTickEnabled(true)
controller:SetActorTickEnabled(true);if brain:IsValid()then brain:RestartLogic()end
local audio={};for _,v in ipairs(e:K2_GetComponentsByClass(StaticFindObject('/Script/Engine.AudioComponent')))do local c=v:get();audio[#audio+1]=c end
local function state()
 local s={ai=e.bIsAIDisabled,dilation=e.CustomTimeDilation,hidden=e.bHidden,collision=e:GetActorEnableCollision(),actorTick=e:IsActorTickEnabled(),meshTick=e.Mesh:IsComponentTickEnabled(),meshVisible=e.Mesh:IsVisible(),moveTick=e.CharacterMovement:IsComponentTickEnabled(),controllerTick=controller:IsActorTickEnabled(),brain=brain:IsValid()and brain:IsRunning()or false,uroTick=e.ActorUROManager:IsComponentTickEnabled(),losTick=e.EnemyLineOfSight:IsComponentTickEnabled(),losEnabled=e.EnemyLineOfSight.bEnabled}
 for i,c in ipairs(audio)do s['audio'..i]=c:GetPlayState() end
 return s
end
local before=state();assert(M.takeNativeOwnership(2)==1,'native takeover rejected')
local held=state();assert(held.ai and held.hidden and held.dilation==0 and not held.collision and not held.actorTick and not held.meshTick and not held.meshVisible and not held.moveTick and not held.controllerTick and not held.brain and not held.uroTick and not held.losTick and not held.losEnabled,'native freeze incomplete')
for _,c in ipairs(audio)do assert(c:GetPlayState()==1 or c:GetPlayState()==2,'audio not stopped/paused')end
assert(M.takeNativeOwnership(2)==1,'idempotent takeover rejected')
assert(M.native('U 2')==1,'native release rejected')
local after=state();for k,v in pairs(before)do assert(after[k]==v,'restore mismatch '..k..' before='..tostring(v)..' after='..tostring(after[k]))end
M.ownershipFixture={before=before,after=after,audioCount=#audio}
return 'PASS native freeze, repeated takeover, exact restore of '..#audio..' audio components and AI/brain/movement/visibility/tick/collision/dilation; '..M.native('S')

