-- The shrine spawner calls OpenDoor/CloseDoor directly. Those animation calls
-- do not consistently update Door_Base.Opened (the ordinary saved-door flag).
return function(valid)
 local M={};local classes={
  {'/Game/Props/SwordShrine/SwordShrineGate_BP.SwordShrineGate_BP_C','Timeline_0'},
  {'/Game/OtherWorld/CastleBridge/OW_BridgeDoor.OW_BridgeDoor_C','OpenDOors'},
  {'/Game/OtherWorld/Castle_Ext/CastleFrontDoor_BP.CastleFrontDoor_BP_C','DoorOpen'},
 }
 function M.kind(a)
  for i,c in ipairs(classes)do if not valid(c.cls)then c.cls=StaticFindObject(c[1])end;if valid(c.cls)and a:IsA(c.cls)then return i end end
 end
 function M.is(a)
  return M.kind(a)~=nil
 end
 function M.timeline(a)return a[classes[assert(M.kind(a))][2]]end
 function M.open(a,save)
  local kind=M.kind(a);if not kind then return a.Opened end
  local killed=FName('ShrineBossKilled')
  if kind==1 and save.NamedSaveData:Contains(killed)and save.NamedSaveData:Find(killed):get()>0 then return true end
  local t=M.timeline(a)
  -- SetToOpenState can set geometry without moving the native timeline.
  if not t:IsPlaying()and kind==2 then return math.abs(a.LeftDoor.RelativeLocation.X+a.OpenWidth)<1 end
  if not t:IsPlaying()and kind==3 then return math.abs(a.AquaDoor_00_SM.RelativeLocation.Y-a.doorLFinalState.Translation.Y)<1 end
  if t:IsPlaying()then return not t:IsReversing()end
  return t:GetPlaybackPosition()>=t:GetTimelineLength()-.001
 end
 function M.apply(a,opened)
  local kind=M.kind(a);local t=M.timeline(a)
  if opened then
   a:SetToOpenState();a.Opened=true
   t:Stop();t:SetPlaybackPosition(t:GetTimelineLength(),false,true)
  else
   a.Opened=false
   if kind==1 then a.ClosedBox:SetCollisionEnabled(3)end
   if kind==3 then t:Stop();t:SetPlaybackPosition(0,false,true)else a:CloseDoor()end
  end
 end
 return M
end
