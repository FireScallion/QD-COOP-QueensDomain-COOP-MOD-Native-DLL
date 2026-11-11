-- Observe resolved native animation state. Never execute remote combat notifies.
return function(P,valid,full)
 local M={serial=0,lastWeapon='None',lastMontage='None',lastPosition=0}
 local function asset(o)
  if not valid(o)then return 'None'end
  local path=full(o):match(' (/Game/[^ ]+)$');return path and P.asset(path) and path or 'None'
 end
 function M.capture(player)
  local weapon=player.CurrentInHandWeapon
  local weaponPath=valid(weapon) and asset(weapon:GetClass()) or 'None'
  local montagePath,position,rate,section='None',0,1,'None'
  local arms=player.Arms
  if valid(arms)then local anim=arms:GetAnimInstance();if valid(anim)then
   local montage=anim:GetCurrentActiveMontage()
   if valid(montage)then
    montagePath=asset(montage);position=anim:Montage_GetPosition(montage);rate=anim:Montage_GetPlayRate(montage)
    section=anim:Montage_GetCurrentSection(montage):ToString():gsub('[|\r\n]','_'):sub(1,80)
   end
  end end
  if weaponPath~=M.lastWeapon or montagePath~=M.lastMontage or position<M.lastPosition-0.10 then M.serial=M.serial+1 end
  M.lastWeapon=weaponPath;M.lastMontage=montagePath;M.lastPosition=position
  return string.format('A|%d|%s|%s|%.5f|%.5f|%s',M.serial,weaponPath,montagePath,rate,position,section),{serial=M.serial,weapon=weaponPath,montage=montagePath,rate=rate,position=position,section=section}
 end
 return M
end
