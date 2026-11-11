local M={}
local dir=debug.getinfo(1,'S').source:gsub('^@',''):match('^(.*[/\\])')
local catalog=dofile(dir..'weapon_catalog.lua')
local bodyCatalog=dofile(dir..'body_catalog.lua')
local skins=dofile(dir..'skin_catalog.lua')
function M.split(s)
 local out={};for field in (s..'|'):gmatch('(.-)|') do out[#out+1]=field end;return out
end
function M.number(s,lo,hi)
 local n=tonumber(s);if not n or n~=n or n==math.huge or n==-math.huge or n<lo or n>hi then return nil end;return n
end
function M.id(s) return type(s)=='string' and #s<=120 and s:match('^[%w_%-]+$')~=nil end
function M.world(s) return type(s)=='string' and #s<=250 and s:match('^/Game/[%w_/%.-]+$')~=nil end
function M.asset(s)return s=='None' or M.world(s)end
local function integer(s,lo,hi)local n=M.number(s,lo,hi);return n and n%1==0 and n or nil end
local function transform(p,start)
 local t={};for i=1,10 do t[i]=M.number(p[start+i-1],i>=8 and -100 or -10000,i>=8 and 100 or 10000);if not t[i]then return nil end end
 local n=0;for i=4,7 do n=n+t[i]^2 end;if n<.8 or n>1.2 then return nil end;return t
end
function M.frame(s)
 if type(s)~='string' or #s>7000 or s:sub(1,6)~='QDCP1\n' or s:sub(-5)~='\nEND\n' then return nil end
 local out={view={pitch=0,running=0,walkSpeed=400,weapon=0,parts={}}}
 for line in s:gmatch('[^\n]+') do
  local p=M.split(line)
  if p[1]=='Y' then
   -- Unsupported cosmetic versions/IDs gracefully use Prince; gameplay stays valid.
   if not out.skinSeen then out.skinSeen=true;out.view.skin=(#p==3 and p[2]=='1')and skins.get(p[3]).id or 'prince' end
  elseif p[1]=='J' then
   local cap=integer(p[4],0,1);local lite=integer(p[6],0,1)
   if #p~=6 or p[2]~='1' or out.J or not M.id(p[3])or not cap or not lite or not M.id(p[5])then return nil end
   out.J={hash=p[3],cap=cap,ack=p[5],lite=lite}
  elseif p[1]=='R' then
   local root=integer(p[4],0,#bodyCatalog.clips);local left=integer(p[5],0,#bodyCatalog.clips);local ready=integer(p[6],0,1)
   if #p~=6 or p[2]~='1' or out.R or not M.id(p[3])or not root or not left or not ready then return nil end
   out.R={hash=p[3],root=root,left=left,ready=ready==1,lanes={}}
  elseif p[1]=='U' then
   local group=integer(p[2],1,7);local id=integer(p[3],1,#bodyCatalog.clips);local serial=integer(p[4],0,1000000000)
   local pos=M.number(p[5],0,100000);local rate=M.number(p[6],-100,100);local section=integer(p[7],0,100);local playing=integer(p[8],0,1)
   if #p~=8 or not out.R or not group or not id or not serial or not pos or not rate or not section or not playing or out.R.lanes[group]then return nil end
   local clip=bodyCatalog.clips[id];if out.R.hash==bodyCatalog.hash and (not clip.montage or clip.group~=group or section>#clip.sections)then return nil end
   out.R.lanes[group]={id=id,serial=serial,position=pos,rate=rate,section=section,playing=playing}
  elseif p[1]=='N' then
   local cap=integer(p[3],0,1);local lite=integer(p[5],0,1)
   if #p~=5 or p[2]~='1' or out.N or not cap or not lite or not M.id(p[4]) then return nil end
   out.N={cap=cap,ack=p[4],lite=lite}
  elseif p[1]=='L' then
   local pitch=M.number(p[2],-45,45);local run=integer(p[3],0,1);local speed=M.number(p[4],1,10000)
   if #p~=4 or out.L or not pitch or not run or not speed then return nil end
   out.L=true;out.view.pitch=pitch;out.view.running=run;out.view.walkSpeed=speed
  elseif p[1]=='W' then
   local id=integer(p[2],0,#catalog.weapons);if #p~=2 or out.W or not id then return nil end;out.W=true;out.view.weapon=id
  elseif p[1]=='V' then
   local slot=integer(p[2],1,4);local mesh=integer(p[3],1,#catalog.meshes);local hand=integer(p[4],1,2);local visible=integer(p[5],0,1);local t=transform(p,6)
   if #p~=15 or not slot or not mesh or not hand or not visible or not t or out.view.parts[slot] then return nil end
   out.view.parts[slot]={mesh=mesh,hand=hand,visible=visible,transform=t,bones={},materials={}}
  elseif p[1]=='K' then
   local slot=integer(p[2],1,4);local v=slot and out.view.parts[slot];local bone=v and integer(p[3],1,#catalog.meshes[v.mesh].bones);local t=transform(p,4)
   if #p~=13 or not v or not bone or not t or v.bones[bone]then return nil end;v.bones[bone]=t
  elseif p[1]=='D' then
   local slot=integer(p[2],1,4);local v=slot and out.view.parts[slot];local index=integer(p[3],0,7);local id=integer(p[4],1,#catalog.materials)
   if #p~=4 or not v or not index or not id or v.materials[index]then return nil end;v.materials[index]=id
  elseif p[1]=='B' then
   if #p~=152 or out.B then return nil end;local b={}
   for i=1,151 do b[i]=M.number(p[i+1],-1000,1000);if not b[i] then return nil end end
   for k=1,37 do local start=k<=36 and (k-1)*4+1 or 148;local norm=0;for j=0,3 do norm=norm+b[start+j]^2 end;if norm<.8 or norm>1.2 then return nil end end
   out.B=b
  elseif p[1]=='A' then
   local serial=M.number(p[2],0,1000000000);local rate=M.number(p[5],-100,100);local position=M.number(p[6],0,100000)
   if #p~=7 or out.A or not serial or serial%1~=0 or not M.asset(p[3]) or not M.asset(p[4]) or not rate or not position or #p[7]>80 or p[7]:find('\r') then return nil end
   out.A={serial=serial,weapon=p[3],montage=p[4],rate=rate,position=position,section=p[7]}
  elseif p[1]=='T' then
   local seq=M.number(p[3],0,1000000000);local time=M.number(p[4],0,1000000000)
   if #p~=4 or not M.id(p[2]) or not seq or seq%1~=0 or not time or out.T then return nil end
   out.T={session=p[2],seq=seq,time=time}
  elseif p[1]=='M' then
   if #p~=9 or out.M then return nil end
   local m={};for i,k in ipairs({'vx','vy','vz','mode','falling','dash','jumps','lands'})do local v=M.number(p[i+1],-100000,100000);if not v then return nil end;m[k]=v end
   if m.mode<0 or m.mode>6 or m.mode%1~=0 or (m.falling~=0 and m.falling~=1) or (m.dash~=0 and m.dash~=1) or m.jumps<0 or m.jumps%1~=0 or m.lands<0 or m.lands%1~=0 then return nil end
   out.M=m
  elseif p[1]=='G' then
   local id=integer(p[2],1,1000000000)
   if #p~=2 or not id or out.downed then return nil end
   out.downed=id
  end
  if p[1]=='P' or p[1]=='E' then
   if not M.world(p[2]) or not M.id(p[3]) then return nil end
   local row={kind=p[1],world=p[2],id=p[3]}
   for i,name in ipairs({'x','y','z','yaw','hp','action','flag'}) do
    local n=M.number(p[i+3],-10000000,10000000);if not n then return nil end;row[name]=n
   end
   if #p~=10 then return nil end
   out[p[1]]=row
  end
 end
 if out.N and out.N.lite==1 and (out.B or not out.A or not out.T or not out.M or not out.L or out.downed)then return nil end
 if out.J and out.J.lite==1 and (out.B or not out.R or not out.R.ready or not out.A or not out.T or not out.M or not out.L)then return nil end
 return out.P and out or nil
end
function M.row(kind,world,id,pos,yaw,hp,action,flag)
 return string.format('%s|%s|%s|%.4f|%.4f|%.4f|%.4f|%.4f|%d|%d',kind,world,id,pos.X,pos.Y,pos.Z,yaw,hp,action,flag)
end
return M
