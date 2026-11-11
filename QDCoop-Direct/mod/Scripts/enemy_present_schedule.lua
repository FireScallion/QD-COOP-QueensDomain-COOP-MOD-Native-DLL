-- Plain snapshot metadata only: no UObject caching and no cadence reduction.
local M={}
function M.prepare(replicas,ordered,own,now)
 local count,roster=0,0
 for _,r in pairs(replicas)do
  roster=roster+1
  if not r.dead and now>=(r.nextPresent or 0)then
   local f=r.presentFrame;local d=f and((f[10]-own.X)^2+(f[11]-own.Y)^2+(f[12]-own.Z)^2)or 1e20
   r.presentNear=d<1800^2
   -- A bounded head start, not an absolute priority: sufficiently overdue
   -- distant objects still run, preventing starvation when both explore.
   local important=r.presentNear or(d<3500^2 and f and(f.action or 0)~=0)
   r.presentRank=(r.nextPresent or now)-(important and .04 or 0)
   count=count+1;ordered[count]=r
  end
 end
 for i=#ordered,count+1,-1 do ordered[i]=nil end
 table.sort(ordered,function(a,b)if a.presentRank==b.presentRank then return a.id<b.id end;return a.presentRank<b.presentRank end)
 return roster
end
function M.wake(r,f,now)r.presentFrame=f;r.nextPresent=math.min(r.nextPresent or now,now)end
function M.wait(r,now)r.nextPresent=now+.1 end
return M
