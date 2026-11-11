-- Pure, host-owned target selection. Damage is actual HP lost, not hit count.
local M={window=3,hold=1.5,ratio=1.25,margin=.12,damageRatio=1.5,memory=1.0,combatRange=2000,frontCos=.5}
function M.range(base,engaged)return engaged and math.max(base,M.combatRange)or base end
function M.new()return {damage={host={},client={}},seen={},target=nil,since=-1e9,switches=0}end
function M.hit(s,id,amount,now)
 if not s.damage[id]or type(amount)~='number'or amount~=amount or amount<=0 then return end
 local q=s.damage[id];q[#q+1]={at=now,amount=amount}
 -- Bound memory under pathological event floods; equal-time hits aggregate.
 if #q>128 then q[2].amount=q[2].amount+q[1].amount;table.remove(q,1)end
end
function M.recent(s,id,now)
 local q=s.damage[id];local n=0
 for i=#q,1,-1 do local age=now-q[i].at;if age>=M.window then table.remove(q,i)else n=n+q[i].amount*math.max(0,1-age/M.window)end end
 return n
end
function M.choose(s,candidates,now,locked,maxHP)
 local scores,eligible={},{};local damage={};local scale=math.max(1,(maxHP or 100)*.15)
 for _,id in ipairs({'host','client'})do
  local c=candidates[id];damage[id]=M.recent(s,id,now)
  if c and c.alive and c.inRange then
   if c.visible then s.seen[id]=now end
   -- Damage reveals a source behind the enemy, but never grants wall vision.
   local sight=c.los and(c.front or damage[id]>0 or s.target==id)
   local memory=s.target==id and now-(s.seen[id]or -1e9)<M.memory
   eligible[id]=sight or memory
   if eligible[id]then
    local d=damage[id]/(damage[id]+scale)
    scores[id]=1.2*d+.8/(1+math.max(0,c.distance)/500)+(c.visible and .35 or 0)
   end
  end
 end
 local current=s.target;local best=nil
 for _,id in ipairs({'host','client'})do if eligible[id]and(not best or scores[id]>scores[best])then best=id end end
 -- A decisive contribution outranks proximity/score hysteresis, using the same
 -- continuously decaying effective-damage window. Zero versus zero never pulls.
 local damagePull
 if current then
  local other=current=='host'and'client'or'host'
  if eligible[other]and candidates[other].los and damage[other]>0 and damage[other]>=damage[current]*M.damageRatio then damagePull=other;best=other end
 end
 local oldValid=current and candidates[current]and candidates[current].alive and candidates[current].inRange
 -- Preserve an attack's commitment, but a continuous attack/recovery chain
 -- must not lock the first target forever.
 if locked then s.lockSince=s.lockSince or now else s.lockSince=nil end
 if current and oldValid and locked and now-s.lockSince<1.2 then best=current
 elseif not damagePull and current and eligible[current]and best and best~=current then
  if now-s.since<M.hold or scores[best]<=scores[current]*M.ratio+M.margin then best=current end
 end
 if best~=current then s.target=best;s.since=now;s.switches=s.switches+1 end
 s.scores=scores;s.recentDamage=damage;return best,best~=current
end
return M
