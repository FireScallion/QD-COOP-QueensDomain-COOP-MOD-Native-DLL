-- Pure Lua admission queue. It never dereferences retained UObject wrappers.
-- Known wire parameters bypass this queue and retain immediate processing.
local M={}
function M.new(limit,ttl)
 local Q={limit=limit,ttl=ttl,queue={},head=1,tail=0,count=0,pending={},recent={},accepted=0,filtered=0,discovered=0,coalesced=0,overflow=0}
 function Q:defer(c,address,name,kind,now)
  self.filtered=self.filtered+1
  local entry=self.pending[address]
  if entry then entry.names[name]=true;entry.mesh=entry.mesh or kind=='s';self.coalesced=self.coalesced+1;return end
  local seen=self.recent[address]
  if seen and now-seen.at<self.ttl and seen.names[name]then self.coalesced=self.coalesced+1;return end
  if self.count>=self.limit then self.overflow=self.overflow+1;return end
  if now>=(self.pruneAt or 0)then
   for id,v in pairs(self.recent)do if now-v.at>=self.ttl then self.recent[id]=nil end end
   self.pruneAt=now+self.ttl
  end
  entry={c=c,address=address,names={[name]=true},mesh=kind=='s'}
  self.tail=self.tail+1;self.queue[self.tail]=entry;self.pending[address]=entry;self.count=self.count+1;self.recent[address]={at=now,names=entry.names}
  return entry
 end
 function Q:pop()
  if self.count==0 then return end
  local entry=self.queue[self.head];self.queue[self.head]=nil;self.head=self.head+1;self.count=self.count-1;self.pending[entry.address]=nil
  if self.count==0 then self.head=1;self.tail=0 end
  self.discovered=self.discovered+1;return entry
 end
 function Q:reset()self.queue={};self.pending={};self.recent={};self.head=1;self.tail=0;self.count=0;self.pruneAt=nil end
 return Q
end
return M
