local dir=debug.getinfo(1,'S').source:gsub('^@',''):match('^(.*[/\\])');local C=dofile(dir..'enemy_catalog.lua');local A=dofile(dir..'enemy_animation_catalog.lua');local P={catalog=C}
function P.parse(s)
 if type(s)~='string'or #s>6900 then return end;local f={};for v in(s..'|'):gmatch('(.-)|')do f[#f+1]=v end
 if #f<25 or f[1]~='N'or not f[2]:match('^[%w_]+$')or #f[2]>80 or not f[3]:match('^/Game/[%w_/%.-]+$')then return end
 for i=4,#f do local n=tonumber(f[i]);if not n or n~=n or math.abs(n)>1e8 then return end;f[i]=n end
 if f[4]<1 or f[4]>100000 or f[4]%1~=0 or f[5]<0 or not C.classes[f[6]]or not C.meshes[f[7]]or f[8]<0 or f[9]<0 or f[9]>2 or f[9]%1~=0 or f[24]<0 or f[24]>140 or f[24]%1~=0 then return end
 local life
 if #f>=7 and f[#f-6]==-901 then
  life={};for i=1,6 do local v=f[#f-6+i];if v%1~=0 then return end;life[i]=v end
  if life[1]<1 or life[1]>5 or life[2]<0 or life[2]>7 or life[6]<0 or life[6]>3 then return end
  for i=1,7 do f[#f]=nil end
 end
 local n=24+f[24]*10
 if f[24]==0 then
  if #f<35 then return end
  local a,m=A.profiles[f[29]],A.montages[f[30]]
  if not a or A.meshes[C.meshes[f[7]]]~=a.skeleton or(f[30]~=0 and(not m or m.skeleton~=a.skeleton))or f[31]<0 or f[31]%1~=0 or f[32]<0 or f[32]>3600 or f[33]<0 or f[33]>10 or f[34]<0 or f[34]>31 or f[34]%1~=0 or math.abs(f[35])>360 then return end
  if #f~=35+a.count and #f~=37+a.count then return end
  if #f==37+a.count then f.pitch=f[#f-1];f.roll=f[#f]end
  local at=36;for _,input in ipairs(a.inputs)do local v=f[at];local k=input.kind
   if k=='b'and v~=0 and v~=1 then return end
   if(k=='i'or k=='a')and v%1~=0 then return end
   if k=='a'and v~=0 and(not A.sequences[v]or A.sequences[v].skeleton~=a.skeleton)then return end
   at=at+((k=='r'or k=='v')and 3 or 1)
  end
 elseif #f~=n and #f~=n+4 and #f~=n+6 then return end
 if f[24]>0 and #f==n+6 then f.pitch=f[#f-1];f.roll=f[#f]end
 if math.abs(f.pitch or 0)>360 or math.abs(f.roll or 0)>360 then return end
 if #f>=n+4 then for i=n+1,n+3 do if math.abs(f[i])>20000 then return end end;if f[n+4]<0 or f[n+4]>3 or f[n+4]%1~=0 then return end end
 for _,base in ipairs({13})do local q=0;for i=base+4,base+7 do q=q+f[i]^2 end;if q<.8 or q>1.2 then return end end
 for bone=0,f[24]-1 do local base=24+bone*10;local q=0;for j=4,7 do q=q+f[base+j]^2 end;if q<.8 or q>1.2 then return end end
 f.life=life;return f
end
function P.encode(f)local a={};for i,v in ipairs(f)do a[i]=type(v)=='number'and string.format((i==4 or i==6 or i==7 or i==9 or i==24)and '%d'or(i==32 and f[24]==0 and '%.6f'or '%.3f'),v)or v end;return table.concat(a,'|')end
return P
