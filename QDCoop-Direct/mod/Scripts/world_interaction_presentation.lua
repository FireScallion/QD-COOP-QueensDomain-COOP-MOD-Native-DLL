-- Recreate only presentation after the native save restore. Never call Interact/Open reward paths.
return function(valid,gs)
 local M={};local time=StaticFindObject('/Script/Engine.Default__KismetSystemLibrary');local moves={};local maths,sparks,lightSound
 local function buttonPose(a,t)
  -- DaggerButton_DamageTarget's native QuickTimeline: 0.5s, easing enum 12.
  -- Reproduce only its mesh/material writes, never Hit() or its activations.
  if not valid(maths)then maths=StaticFindObject('/Script/Engine.Default__KismetMathLibrary')end
  local x=maths:Ease(0,1,t,12,2,2);local mesh=a.StaticMesh
  mesh:K2_SetRelativeLocation({X=a.ButtonPushinPos*x,Y=0,Z=0},false,{},false)
  mesh:SetScalarParameterValueOnMaterials(FName('Color_Brightness'),1-.8*x)
  mesh:SetScalarParameterValueOnMaterials(FName('Cubemap_Intensity'),1-.9*x)
  mesh:SetScalarParameterValueOnMaterials(FName('Emissive_Brightness'),.3-.25*x)
 end
 function M.tick()
  for k,v in pairs(moves)do
   if not valid(v.a)then moves[k]=nil else local t=math.min(1,math.max(0,(time:GetGameTimeInSeconds(v.a)-v.at)/.5));buttonPose(v.a,t);if t>=1 then moves[k]=nil end end
  end
 end
 function M.reset()for _,v in pairs(moves)do if valid(v.a)then buttonPose(v.a,1)end end;moves={}end
 function M.before(a)
  if a:IsA('/Game/RuinsLevel/Models/DaggerButton/DaggerButton_DamageTarget.DaggerButton_DamageTarget_C')then return {kind='daggerButton',hit=a.AlreadyHit}
  elseif a:IsA('/Game/Enemies/Gargler_00/Gargoyle_Switch.Gargoyle_Switch_C')then return {kind='gargoyle',lit=a.Lit}
  elseif a:IsA('/Game/LD45/Blueprints/Objects/Gate/Lever.Lever_C')then
   return {kind='lever',on=a.ToggledOn}
  elseif a:IsA('/Game/Blueprints/Door_Base.Door_Base_C')then
   local timelines={};for _,v in ipairs(a:K2_GetComponentsByClass(StaticFindObject('/Script/Engine.TimelineComponent')))do local t=v:get();if valid(t)then timelines[#timelines+1]={t=t,position=t:GetPlaybackPosition()}end end
   return {kind='door',opened=a.Opened,timelines=timelines}
  elseif a:IsA('/Game/Blueprints/SecretWall.SecretWall_C')then return {kind='secret',opened=a.Opened,running=a.AnimRunning}
  elseif a:IsA('/Game/Props/Chest_00/Chest.Chest_C')then return {kind='chest',opened=a.InteractBox:GetCollisionEnabled()==0,running=a.OpenLid:IsPlaying()}end
 end
 function M.after(a,b)
  if not b then return end
  if b.kind=='daggerButton'then
   if not b.hit and a.AlreadyHit then
    moves[a:GetAddress()]={a=a,at=time:GetGameTimeInSeconds(a)};buttonPose(a,0)
    if valid(a.SoundOnHit)then gs:PlaySoundAtLocation(a,a.SoundOnHit,a:K2_GetActorLocation(),{Pitch=0,Yaw=0,Roll=0},1,1,0,nil,nil,nil)end
    if not valid(sparks)then sparks=LoadAsset('/Game/RuinsLevel/Models/DaggerButton/DaggerButton_Hitspakrs.DaggerButton_Hitspakrs')end
    if valid(sparks)then StaticFindObject('/Script/Niagara.Default__NiagaraFunctionLibrary'):SpawnSystemAtLocation(a,sparks,a:K2_GetActorLocation(),a:K2_GetActorRotation(),{X=1,Y=1,Z=1},true,true,1,true)end
   end
  elseif b.kind=='gargoyle'then
   if not b.lit and a.Lit then
    a.Timeline_0:PlayFromStart()
    if not valid(lightSound)then lightSound=LoadAsset('/Game/Enemies/Gargler_00/Audio/Gargoyle_Switch_Light.Gargoyle_Switch_Light')end
    if valid(lightSound)then gs:SpawnSoundAttached(lightSound,a.SkeletalMesh,FName('None'),{X=0,Y=0,Z=0},{Pitch=0,Yaw=0,Roll=0},0,false,1,1,0,nil,nil,true)end
   end
  elseif b.kind=='lever'then
   if a.ToggledOn~=b.on then
    if a.ToggledOn then a:AnimateLeverPull()else a:AnimateLeverReset()end
    if a.ToggledOn and valid(a.ActivateSound)then gs:PlaySoundAtLocation(a,a.ActivateSound,a:K2_GetActorLocation(),{Pitch=0,Yaw=0,Roll=0},1,1,0,nil,nil,nil)end
   end
  elseif b.kind=='door'then if a.Opened~=b.opened then
   -- RestoreFromSave seeks the original timeline to its endpoint. Rewind only
   -- its presentation before letting the native Open/Close event resume it.
   for _,v in ipairs(b.timelines)do if valid(v.t)then v.t:SetPlaybackPosition(v.position,false,true)end end
   if a.Opened then a:OpenDoor()else a:CloseDoor()end
  end
  elseif b.kind=='chest'then
   if not b.opened and not b.running and a.InteractBox:GetCollisionEnabled()==0 then
    a.OpenLid:PlayFromStart()
    if valid(a.OpenSound)then gs:PlaySoundAtLocation(a,a.OpenSound,a:K2_GetActorLocation(),{Pitch=0,Yaw=0,Roll=0},1,1,0,nil,nil,nil)end
   end
  elseif b.kind=='secret'and a.Opened~=b.opened and not b.running then
   a.AnimStartTime=time:GetGameTimeInSeconds(a);a.AnimRunning=true;a:SetActorTickEnabled(true)
   local sound=valid(a.OpenSoundOverride)and a.OpenSoundOverride or a.OpenSound
   if valid(sound)then
    a.OpeningSoundComp=gs:SpawnSoundAttached(sound,a.DefaultSceneRoot,FName('None'),{X=0,Y=0,Z=0},{Pitch=0,Yaw=0,Roll=0},0,false,1,1,0,nil,nil,true)
    if valid(a.OpeningSoundComp)then a.OpeningSoundComp:FadeIn(.2,1,0,0)end
   end
  end
 end
 return M
end
