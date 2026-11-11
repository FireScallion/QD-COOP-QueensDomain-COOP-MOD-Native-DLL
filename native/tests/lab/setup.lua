local M=assert(M1);local gs=StaticFindObject('/Script/Engine.Default__GameplayStatics');local ks=StaticFindObject('/Script/Engine.Default__KismetSystemLibrary')
local p=FindFirstOf('FS_PlayerChar_C');assert(p and p:IsValid());M.player=p
gs:SetGamePaused(p,false);p.bCanBeDamaged=false
local pc=p:GetController();pc:SetIgnoreMoveInput(true);pc:SetIgnoreLookInput(true)
p.CharacterMovement:StopMovementImmediately();p.CharacterMovement:SetMovementMode(5,0);p.CharacterMovement:SetComponentTickEnabled(false)
p:K2_SetActorLocation({X=16200,Y=-6600,Z=65},false,{},true);pc:SetControlRotation({Pitch=-5,Yaw=-24,Roll=0})
ks:ExecuteConsoleCommand(p,'r.VSync 0',pc);ks:ExecuteConsoleCommand(p,M.role=='host'and't.MaxFPS 60'or't.MaxFPS 0',pc)
local loaded={};gs:GetAllActorsOfClass(p,StaticFindObject('/Game/Enemies/EnemyBase.EnemyBase_C'),loaded)
local class
for _,entry in ipairs(loaded)do local e=entry:get();if e:IsValid()then
 if e:GetClass():GetFullName():find('Cocker_Base_BP_C',1,true)then class=e:GetClass()end
 e:SetActorTickEnabled(false);e:SetActorEnableCollision(false);e.CharacterMovement:StopMovementImmediately();e.CharacterMovement:SetComponentTickEnabled(false)
 local c=e:GetController();if c and c:IsValid()then c:StopMovement();c:SetActorTickEnabled(false);local brain=c.BrainComponent;if brain and brain:IsValid()then brain:StopLogic('QD M1 isolated measurement')end end
end end
assert(class,'Cocker class missing')
local function transform(x,y,z)return {Translation={X=x,Y=y,Z=z},Rotation={X=0,Y=0,Z=0,W=1},Scale3D={X=1,Y=1,Z=1}}end
M.count=24;M.pattern=false;M.legacy=false;M.native('M 0')
for i=1,M.count do
 local x=16800+((i-1)%6)*140;local y=-7200+math.floor((i-1)/6)*140;local z=55
 local t=transform(x,y,z);local e=gs:BeginDeferredActorSpawnFromClass(p,class,t,1,nil);assert(e:IsValid());gs:FinishSpawningActor(e,t)
 e:SetActorEnableCollision(false);e:SetActorTickEnabled(false);e.CharacterMovement:StopMovementImmediately();e.CharacterMovement:SetComponentTickEnabled(false)
 local c=e:GetController();if c and c:IsValid()then c:StopMovement();c:SetActorTickEnabled(false);local brain=c.BrainComponent;if brain and brain:IsValid()then brain:StopLogic('QD M1 deterministic source')end end
 e.Mesh:SetComponentTickEnabled(true);e.HP=100+i;e.bIsChasing=true
 assert(M.native('R',i,M.generation,e:GetAddress(),e.Mesh:GetAddress())==1,'register '..i)
 M.entities[i]={e=e,x=x,y=y,z=z,id=i}
end
M.start=M.native('N');M.pattern=true
-- Same deterministic engine movement in both arms. This is test input, not the
-- sync implementation. All host/guest actors are genuine game EnemyBase actors.
LoopInGameThreadWithDelay(0,function()
 if not M.pattern or M.role~='host'then return end
 local t=M.native('N');if t<(M.nextMotion or 0)then return end;M.nextMotion=t+1/60
 for i,r in ipairs(M.entities)do if r.e:IsValid()and not r.e:IsActorBeingDestroyed()then
  local a=t*1.3+i*.31;local x=r.x+math.sin(a)*90;local y=r.y+math.cos(a)*70
  r.e:K2_SetActorLocationAndRotation({X=x,Y=y,Z=r.z},{Pitch=0,Yaw=(a*40)%360,Roll=0},false,{},true)
  r.e.HP=100+i+(math.floor(t)%5);r.e.bIsAttacking=math.floor(t+i)%4==0;r.e.bInRecoveryTime=false;r.e.bIsChasing=true
 end end
end)
assert(M.native('M 1')==1)
return 'registered '..#M.entities..' real enemy actors; '..M.native('S')
