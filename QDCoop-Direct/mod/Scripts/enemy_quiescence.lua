-- Only known authority/visibility-scheduling components are stopped. Damage,
-- original death presentation and audio components retain their own lifetimes.
return function(valid)
 local M={}
 function M.pause(e)
  local s={e=e,disabled=e.bIsAIDisabled,components={}}
  e.bIsAIDisabled=true
  for _,name in ipairs({'ActorUROManager','EnemyLineOfSight'})do
   local c=e[name]
   if valid(c)then
    local v={c=c,tick=c:IsComponentTickEnabled()}
    if name=='EnemyLineOfSight'then v.enabled=c.bEnabled;c.bEnabled=false end
    c:SetComponentTickEnabled(false);s.components[#s.components+1]=v
   end
  end
  return s
 end
 function M.restore(s)
  if not s then return end
  if valid(s.e)then s.e.bIsAIDisabled=s.disabled end
  for _,v in ipairs(s.components)do if valid(v.c)then
   if v.enabled~=nil then v.c.bEnabled=v.enabled end
   v.c:SetComponentTickEnabled(v.tick)
  end end
 end
 return M
end
