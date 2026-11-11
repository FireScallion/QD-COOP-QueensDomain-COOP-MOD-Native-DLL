-- Data only. No load/loadstring, arbitrary property names or remote code.
local M={limit=1048576}
local dir=debug.getinfo(1,'S').source:gsub('^@',''):match('^(.*[/\\])')
local Gates=dofile(dir..'encounter_gates.lua')
local personalNames=dofile(dir..'personal_progress_names.lua')
M.fields={SavableActorStates='gi',SavableActorItems='go',NamedSaveData='ni',UnlockedTeleporters='ao',SavePointsVisited='ao',SavePointID='o',LastTouchedCheckpoint='o',SavedArea='o',PlayingMusicTrack='o',SaveLevel='n',CurrentLockInID='n',MaxProgressionLevel='i',DoIntro='b',DoWakeOnBeach='b'}
function M.personalName(n)return personalNames[n]or n:match('^QDCoopPersonal~')or n:match('^TotalAchievement')or n:match('^ReadNote_')or n:match('GuideGranted$')or n=='ShopAmountSpent'or n=='ShopSessionSpend'or n=='TiniScytheHammerAP'or n=='CompletedChallenges'end
function M.split(s)local r={};for v in(s..'|'):gmatch('(.-)|')do r[#r+1]=v end;return r end
function M.escape(s)return(s:gsub('[^%w_/%-%.: ]',function(c)return string.format('%%%02X',c:byte())end))end
function M.unescape(s)if #s>1200 or s:find('%%[^%x]')or s:find('%%.$')then return nil end;return(s:gsub('%%(%x%x)',function(x)return string.char(tonumber(x,16))end))end
function M.number(s,lo,hi)local n=tonumber(s);if not n or n~=n or n<lo or n>hi then return nil end;return n end
function M.integer(s,lo,hi)local n=M.number(s,lo or -2147483648,hi or 2147483647);return n and n%1==0 and n or nil end
function M.asset(s)return s=='' or (#s<=300 and s:match('^/Game/[%w_/%-%.]+$')~=nil)end
function M.guid(s)local a={};for n in(s..','):gmatch('(.-),')do local v=M.integer(n);if not v then return nil end;a[#a+1]=v end;if #a==4 then return {A=a[1],B=a[2],C=a[3],D=a[4]}end end
local function value(t,s)
 if t=='o'then return M.asset(s)and s or nil elseif t=='n' then return #s<=900 and not s:find('[%z\r\n|]')and s or nil elseif t=='i'then return M.integer(s)elseif t=='b'then if s=='0'or s=='1'then return s=='1'end end
end
function M.decode(raw)
 if #raw>M.limit or not raw:match('\nEND\n$')then return nil end
 local out={fields={},streams={{},{}},doors={},levers={},gates={}};local header=false;local count=0
 for line in raw:gmatch('[^\n]+')do
  local f=M.split(line);local tag=f[1];count=count+1;if count>20000 then return nil end
  if tag=='QDW1'then if header or #f~=4 or not f[2]:match('^[%w_%-]+$')or #f[2]>80 then return nil end;header=true;out.session=f[2];out.revision=M.integer(f[3],1,1000000000);out.travel=M.integer(f[4],0,1000000000);if not out.revision or not out.travel then return nil end
  elseif tag=='W'then if #f~=2 or out.world or not M.asset(f[2])or f[2]==''then return nil end;out.world=f[2]
  elseif tag=='MU'then if #f~=2 or out.music~=nil or not M.asset(f[2])then return nil end;out.music=f[2]
  elseif tag=='R'then if #f~=2 or out.mapRevision then return nil end;out.mapRevision=M.integer(f[2],0,1000000000);if not out.mapRevision then return nil end
  elseif tag=='P'then if #f~=5 or out.pos then return nil end;local a={};for i=2,5 do a[i]=M.number(f[i],-10000000,10000000);if not a[i]then return nil end end;out.pos={X=a[2],Y=a[3],Z=a[4]};out.yaw=a[5]
  elseif tag=='F'then local t=M.fields[f[2]];local s=M.unescape(f[3]or'');if not t or #t~=1 or #f~=3 or s==nil or out.fields[f[2]]~=nil then return nil end;local v=value(t,s);if v==nil then return nil end;out.fields[f[2]]=v
  elseif tag=='C'then local t=M.fields[f[2]];if #f~=2 or not t or #t~=2 or out.fields[f[2]]then return nil end;out.fields[f[2]]={}
  elseif tag=='E'then local t=M.fields[f[2]];local dest=out.fields[f[2]];if not t or #t~=2 or type(dest)~='table'then return nil end
   local k=M.unescape(f[3]or'');if not k then return nil end
   if t=='ao'then if #f~=3 or not M.asset(k)or k==''or #dest>=4096 then return nil end;dest[#dest+1]=k
   else local v=M.unescape(f[4]or'');if #f~=4 or not v or dest[k]~=nil then return nil end;if t:sub(1,1)=='g'and not M.guid(k)then return nil end;if t:sub(1,1)=='n'and not value('n',k)then return nil end;v=value(t:sub(2),v);if v==nil then return nil end;dest[k]=v end
  -- Native intro streaming uses priority 99999. Preserve the signed int32
  -- value instead of rejecting legitimate level lists above an arbitrary cap.
  elseif tag=='L'then local n=M.integer(f[2],1,2);local p=M.integer(f[4]);if #f~=4 or not n or not p or not M.asset(f[3])or f[3]==''or #out.streams[n]>=64 then return nil end;out.streams[n][#out.streams[n]+1]={path=f[3],priority=p}
  elseif tag=='D'then local k=f[2];local b=M.integer(f[3],0,1);local n=M.integer(f[4],0,10000);if #f~=4 or not M.guid(k)or not b or not n or out.doors[k]then return nil end;out.doors[k]={open=b==1,count=n}
  elseif tag=='J'then local k=f[2];local b=M.integer(f[3],0,1);if #f~=3 or not M.guid(k)or not b or out.levers[k]~=nil then return nil end;out.levers[k]=b==1
  elseif tag=='G'then local v=Gates.decode(f,M);if not v then return nil end;local key=v.path..'|'..v.component;if out.gates[key]then return nil end;out.gates[key]=v
  elseif tag~='END'then return nil end
 end
 for n in pairs(M.fields)do if out.fields[n]==nil then return nil end end
 return header and out.world and out.pos and out.mapRevision and out or nil
end
return M
