return function(E)
 local M={};M.__index=M
 function M.new()return setmetatable({tracks={},session=nil,lastSeq=-1,drops=0},M)end
 function M:reset(session)self.tracks={};self.session=session;self.lastSeq=-1;self.watermark=nil end
 function M:add(v)
  if self.watermark and v.time<self.watermark-3 then return false end
  self.watermark=math.max(self.watermark or v.time,v.time)
  local t=self.tracks[v.id];if not t then local n=0;for _ in pairs(self.tracks)do n=n+1 end;if n>=512 then self.drops=self.drops+1;return false end;t={frames={}};self.tracks[v.id]=t end
  if t.ended and v.time>=t.ended then return false end
  local f=t.frames;for i=#f,1,-1 do if f[i].time==v.time then f[i]=v;return true elseif f[i].time<v.time then table.insert(f,i+1,v);while #f>32 do table.remove(f,1)end;return true end end
  table.insert(f,1,v);while #f>32 do table.remove(f,1)end;return true
 end
 function M:push(f)
  if self.session~=f.session then self:reset(f.session)end;if f.seq<=self.lastSeq then return false end;self.lastSeq=f.seq
  for _,v in pairs(f.records)do self:add(v)end;return true
 end
 function M:finish(id,time)local t=self.tracks[id];if not t then local n=0;for _ in pairs(self.tracks)do n=n+1 end;if n>=512 then self.drops=self.drops+1;return false end;t={frames={}};self.tracks[id]=t end;t.ended=t.ended and math.min(t.ended,time)or time;return true end
 function M:sample(playerTime,enemyTime)
  local out={}
  for id,t in pairs(self.tracks)do local f=t.frames;local time=#f>0 and(f[#f].entity or 0)>0 and(enemyTime or playerTime)or playerTime
   if t.ended and time>=t.ended or (#f>0 and time-f[#f].time>(f[#f].mode==1 and 2 or .7))then
    if time-(t.ended or f[#f].time)>3 then self.tracks[id]=nil end
   elseif #f>0 and time>=f[1].time then
    while #f>2 and f[2].time<=time do table.remove(f,1)end
    local a,b=f[1],f[2];if b and time>=b.time then a=b;b=nil end;if t.pairA~=a or t.pairB~=b then t.pairA=a;t.pairB=b;t.constant=b and E.sameState(a,b)or false end
    out[id]=t.constant and a or E.interpolate(a,b,time)
   end
  end;return out
 end
 return M
end

