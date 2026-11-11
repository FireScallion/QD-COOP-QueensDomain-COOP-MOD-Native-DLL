local dir=debug.getinfo(1,'S').source:gsub('^@',''):match('^(.*[/\\])');local C=dofile(dir..'effect_catalog.lua');local P=dofile(dir..'protocol.lua');local Q=dofile(dir..'pose_math.lua')
local extra=dofile(dir..'enemy_effect_catalog.lua');local known={};for _,v in ipairs(C.assets)do known[v.path]=true end;for _,v in ipairs(extra.assets)do if not known[v.path]then C.assets[#C.assets+1]=v;known[v.path]=true end end
known={};for _,v in ipairs(C.variables)do known[v]=true end;for _,v in ipairs(extra.variables)do if not known[v]then C.variables[#C.variables+1]=v;known[v]=true end end
-- Preserve exact material parameter names: Niagara User.Opacity and material
-- Opacity are different names, although the legacy FX API accepts the alias.
for _,name in ipairs({'Opacity','DeathFade'})do if not known[name]then C.variables[#C.variables+1]=name;known[name]=true end end
local M={catalog=C,enemyCatalog=extra}
local function int(s,a,b)local n=P.number(s,a,b);return n and n%1==0 and n or nil end
function M.record(line)
 local p=P.split(line);if p[1]~='F'or(#p~=17 and #p~=20)then return nil end
 local id=int(p[2],1,10000000);local asset=int(p[3],1,#C.assets);local hand=int(p[4],0,2);local active=int(p[5],0,3);local mode=int(p[6],0,1);local time=P.number(p[7],0,1e9);if not(id and asset and hand and active and mode and time)or C.assets[asset].kind=='material'then return nil end
 local t={};for i=1,10 do t[i]=P.number(p[7+i],i>=8 and -100 or -1e7,i>=8 and 100 or 1e7);if not t[i]then return nil end end
 local n=0;for i=4,7 do n=n+t[i]^2 end;if n<.8 or n>1.2 then return nil end
 local entity,anchor,bone=0,0,0;if #p==20 then entity=int(p[18],0,100000);anchor=int(p[19],0,3);bone=int(p[20],0,139);if not(entity and anchor and bone)or(entity==0 and(anchor~=0 or bone~=0))or(entity>0 and hand~=0)then return nil end end
 return {id=id,asset=asset,hand=hand,active=active,mode=mode,time=time,t=t,entity=entity,anchor=anchor,bone=bone,params={},materials={}}
end
function M.encode(v)
 local p={'F',v.id,v.asset,v.hand,v.active,v.mode,string.format('%.6f',v.time)};for _,x in ipairs(v.t)do p[#p+1]=string.format('%.5f',x)end;p[#p+1]=v.entity or 0;p[#p+1]=v.anchor or 0;p[#p+1]=v.bone or 0;return table.concat(p,'|')
end
function M.block(v)
 local rows={M.encode(v)};for key,p in pairs(v.params or{})do local row={'N',v.id,key,p.kind};for _,x in ipairs(p.values)do row[#row+1]=string.format('%.5f',x)end;rows[#rows+1]=table.concat(row,'|')end
 for slot,id in pairs(v.materials or{})do rows[#rows+1]='D|'..v.id..'|'..slot..'|'..id end;return table.concat(rows,'\n')
end
function M.frame(s)
 if type(s)~='string'or #s>7000 or s:sub(1,6)~='QDVF1\n'or s:sub(-5)~='\nEND\n' then return nil end
 local out={records={}};local count=0
 for l in s:gmatch('[^\n]+')do local p=P.split(l)
  if p[1]=='H'then if out.session or #p~=5 or not P.id(p[2])or not P.world(p[3])then return nil end;out.session=p[2];out.world=p[3];out.seq=int(p[4],0,1e9);out.time=P.number(p[5],0,1e9);if not(out.seq and out.time)then return nil end
  elseif p[1]=='F'then local v=M.record(l);if not v or out.records[v.id]then return nil end;out.records[v.id]=v;count=count+1;if count>32 then return nil end
  elseif p[1]=='N'then local id=int(p[2],1,1e7);local v=id and out.records[id];local key=int(p[3],1,#C.variables);local kind=p[4];local n=({f=1,b=1,i=1,v=3,c=4,s=1})[kind];if not(v and key and n)or #p~=4+n or v.params[key]then return nil end;local values={};for i=1,n do values[i]=P.number(p[4+i],-1e7,1e7);if not values[i]then return nil end end;if kind=='b'and values[1]~=0 and values[1]~=1 then return nil end;v.params[key]={kind=kind,values=values}
  elseif p[1]=='D'then local id=int(p[2],1,1e7);local v=id and out.records[id];local slot=int(p[3],0,7);local asset=int(p[4],1,#C.assets);if #p~=4 or not(v and slot and asset)or C.assets[asset].kind~='material'then return nil end;v.materials[slot]=asset
  elseif l~='QDVF1'and l~='END'then return nil end
 end
 return out.session and out or nil
end
-- Exact state equivalence excludes time only. Timelines cache this once per
-- received pair; a moving attachment still interpolates every engine frame.
function M.sameState(a,b)
 for _,k in ipairs({'id','asset','hand','active','mode','entity','anchor','bone'})do if a[k]~=b[k]then return false end end
 for i=1,10 do if a.t[i]~=b.t[i]then return false end end
 for k,v in pairs(a.materials or{})do if(b.materials or{})[k]~=v then return false end end
 for k in pairs(b.materials or{})do if(a.materials or{})[k]==nil then return false end end
 for k,p in pairs(a.params or{})do local q=(b.params or{})[k];if not q or q.kind~=p.kind or #q.values~=#p.values then return false end;for i,v in ipairs(p.values)do if q.values[i]~=v then return false end end end
 for k in pairs(b.params or{})do if not(a.params or{})[k]then return false end end
 return true
end
function M.interpolate(a,b,time)
 if not b or a.asset~=b.asset or a.hand~=b.hand or(a.entity or 0)~=(b.entity or 0)or(a.anchor or 0)~=(b.anchor or 0)or(a.bone or 0)~=(b.bone or 0)or b.time<=a.time then return a end
 local u=Q.clamp((time-a.time)/(b.time-a.time),0,1);local x={id=a.id,asset=a.asset,hand=a.hand,active=u<1 and a.active or b.active,mode=a.mode,time=time,entity=a.entity,anchor=a.anchor,bone=a.bone,params={},materials=u<1 and a.materials or b.materials};x.t=Q.blendTransform(a.t,b.t,u)
 for key,p in pairs(u<1 and a.params or b.params)do local q=b.params[key];local r={kind=p.kind,values={}};for i,value in ipairs(p.values)do r.values[i]=q and q.kind==p.kind and p.kind~='b'and p.kind~='i'and(value+(q.values[i]-value)*u)or value end;x.params[key]=r end
 return x
end
return M
