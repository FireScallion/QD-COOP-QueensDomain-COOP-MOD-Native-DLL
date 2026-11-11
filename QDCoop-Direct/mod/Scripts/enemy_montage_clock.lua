-- Verified original Boner trap montages: Default loops into itself; End rises once.
-- Do not extrapolate a resting pose into End, or count a natural wrap as a new attack.
return function(catalog)
 local M={};local loops={}
 local verified={Boner_Trap_Montage=.96666664,Boner_Trap_Heavy_Montage=3.3333335,Boner_CasketTrap_Montage=2.4583333}
 for id,m in ipairs(catalog.montages)do
  for name,finish in pairs(verified)do
   if m.path=='/Game/Enemies/Boner_00/Animation/'..name..'.'..name then loops[id]=finish end
  end
 end
 function M.inLoop(id,p)local finish=loops[id];return finish and p>=0 and p<finish or false end
 function M.restart(oldId,id,oldPosition,position)
  if oldId~=id then return true end
  oldPosition=oldPosition or 0
  local before,after=M.inLoop(id,oldPosition),M.inLoop(id,position)
  if before~=after then return true end -- Original ActivateTrap explicitly starts End.
  return not before and position<oldPosition-.04
 end
 function M.advance(id,p,seconds,rate)
  local finish=loops[id];local target=math.max(0,p+math.max(0,seconds)*rate)
  if M.inLoop(id,p)then
   -- Reserve two microseconds so six-decimal wire rounding cannot cross sections.
   return math.min(target%finish,finish-.000002)
  end
  if finish and p>=finish then target=math.max(target,finish+.000002)end
  return target
 end
 function M.drift(id,current,target)
  local finish=loops[id]
  if M.inLoop(id,current)~=M.inLoop(id,target)then return math.huge end
  if finish and M.inLoop(id,target)then return math.abs((current-target+finish/2)%finish-finish/2)end
  return math.abs(current-target)
 end
 return M
end
