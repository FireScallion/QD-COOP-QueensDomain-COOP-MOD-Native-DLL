local dir=assert(debug.getinfo(1,'S').source:gsub('^@',''):match('^(.*[/\\])'))
local C=dofile(dir..'audio_catalog.lua');local M={catalog=C}
local function number(s,lo,hi,int)local n=tonumber(s);if not n or n~=n or n<lo or n>hi or(int and n%1~=0)then return nil end;return int and math.floor(n)or n end
function M.parse(s)
 if type(s)~='string'or #s>900 or s:find('[\r\n]')then return end
 local f={};for v in(s..'|'):gmatch('(.-)|')do f[#f+1]=v end
 local ranges
 if f[1]=='A'and #f==16 then ranges={{1,1e7,true},{1,#C.sounds,true},{0,1e9},{0,1,true},{-1e7,1e7},{-1e7,1e7},{-1e7,1e7},{0,8},{.1,4},{0,600},{0,#C.attenuations,true},{0,1e7,true},{0,64,true},{.01,600},{0,1,true}}
 elseif f[1]=='U'and #f==8 then ranges={{1,1e7,true},{0,1e9},{-1e7,1e7},{-1e7,1e7},{-1e7,1e7},{0,8},{.1,4}}
 elseif f[1]=='R'and #f==4 then ranges={{1,1e7,true},{0,1e9},{0,10}}
 else return end
 for i,r in ipairs(ranges)do f[i+1]=number(f[i+1],r[1],r[2],r[3]);if not f[i+1]then return end end
 return f
end
function M.encode(f)local a={};for i,v in ipairs(f)do a[i]=type(v)=='number'and string.format('%.6f',v)or v end;local s=table.concat(a,'|');assert(M.parse(s),'Invalid local audio record');return s end
-- A voice is an event, never a per-frame replay. End-before-start is retained.
function M.new()
 local T={voices={},seen={},count=0,dropped=0,duplicate=0}
 function T:push(f)
  local id=f[2];local v=self.voices[id]
  if f[1]=='A'then
   if self.seen[id]then self.duplicate=self.duplicate+1;return false end
   self.seen[id]=f[4];if not v then if self.count>=96 then self.dropped=self.dropped+1;return false end;v={};self.voices[id]=v;self.count=self.count+1 end
   v.start=f;v.last=math.max(v.last or 0,f[4]);return true
  end
  if not v then if self.count>=96 then self.dropped=self.dropped+1;return false end;v={};self.voices[id]=v;self.count=self.count+1 end
  if f[1]=='R'then if not v.stop or f[3]<v.stop[3]then v.stop=f end;v.last=math.max(v.last or 0,f[3])
  elseif not v.update or f[3]>v.update[3]then v.update=f;v.last=math.max(v.last or 0,f[3])end;return true
 end
 function T:forget(id)if self.voices[id]then self.voices[id]=nil;self.count=self.count-1 end end
 function T:prune(time)for id,v in pairs(self.voices)do if time-(v.last or 0)>35 then self:forget(id)end end;for id,t in pairs(self.seen)do if time-t>60 then self.seen[id]=nil end end end
 return T
end
return M
