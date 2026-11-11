-- Native elevators save their DESTINATION before starting the latent movement.
-- RestoreFromSave snaps to that destination and fires arrival delegates: it is
-- for loading a level, never for applying a live multiplayer state update.
return function(valid)
 local M={};local busy={}
 function M.moving(a,on)
  local function update(l)
   if not valid(l)then return end;local key=l:GetFullName()
   if on then if not busy[key]then busy[key]={path=key:match(' (.+)$'),previous=l.DisableLockedMessage}end;l.DisableLockedMessage=true
   elseif busy[key]then l.DisableLockedMessage=busy[key].previous;busy[key]=nil end
  end
  update(a.OnboardLever);for _,name in ipairs({'StartLevers','EndLevers'})do a[name]:ForEach(function(_,v)update(v:get())end)end
 end
 function M.reset()
  for _,s in pairs(busy)do local l=StaticFindObject(s.path);if valid(l)then l.DisableLockedMessage=s.previous end end;busy={}
 end
 function M.is(a)return valid(a)and a:IsA('/Game/Blueprints/Elevator/Elevator_Base.Elevator_Base_C')end
 function M.near(a,p)
  local function near(o)if not valid(o)then return false end;local q=o:IsA('/Script/Engine.SceneComponent')and o:K2_GetComponentLocation()or o:K2_GetActorLocation();return(q.X-p.X)^2+(q.Y-p.Y)^2+(q.Z-p.Z)^2<=1200^2 end
  if near(a)or near(a.ElevatorPlatformRoot)or near(a.OnboardLever)then return true end
  for _,name in ipairs({'StartLevers','EndLevers'})do local found=false;a[name]:ForEach(function(_,v)if near(v:get())then found=true end end);if found then return true end end
  return false
 end
 function M.accept(a,index,value)
  -- A second lever cannot reverse a journey already underway. In particular,
  -- an arriving peer must not overwrite the destination of a later journey.
  return index~=0 or not a.Running or (value~=0)==(not a.PlayingInReverse)
 end
 function M.restore(a,save)
  local function value(i)local g=a.GUIDs[i+1];if g and save.SavableActorStates:Contains(g)then return save.SavableActorStates:Find(g):get()end end
  local unlocked=value(1);if unlocked~=nil then a.LockedBecauseNoRide=unlocked<1 end
  local goal=value(0)
  if a.LoopMode==0 and goal~=nil and not a.Running and a.AtEnd~=(goal~=0)then
   a.PlayingInReverse=a.AtEnd;a:StartElevatorTowardsEnd()
  end
  a:UpdateLevers()
 end
 return M
end
