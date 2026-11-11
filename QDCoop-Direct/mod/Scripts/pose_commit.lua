-- Compare with the last COMMITTED pose, never the last input. Small movement
-- therefore accumulates and cannot be lost forever by the tolerance check.
return function()
 local sent={};local M={}
 function M.reset()sent={}end
 function M.changed(id,p,q)
  local old=sent[id];local changed=not old
  if old then
   for i=1,3 do if math.abs(p[i]-old[i])>0.0001 then changed=true;break end end
   if not changed then for i=1,4 do if math.abs(q[i]-old[i+3])>0.000001 then changed=true;break end end end
  end
  return changed
 end
 function M.commit(id,p,q)sent[id]={p[1],p[2],p[3],q[1],q[2],q[3],q[4]}end
 return M
end
