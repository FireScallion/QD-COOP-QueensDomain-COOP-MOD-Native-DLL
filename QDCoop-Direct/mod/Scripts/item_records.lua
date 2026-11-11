-- Item custody is stored alongside each native save, so a checkpoint rollback
-- restores inventories, receipts and ground items from the same generation.
local M={prefix='QDCoopDrop~',personal='QDCoopPersonal~'}
function M.split(s)local a={};for v in (s..'~'):gmatch('(.-)~')do a[#a+1]=v end;return a end
function M.id(s)return type(s)=='string' and #s<=80 and s:match('^[%w_%-]+$')end
function M.decode(s)
 local a=M.split(s);if #a~=9 or not M.id(a[1])or not M.id(a[2])or not M.id(a[3])then return end
 if #a[4]>250 or not a[4]:match('^/Game/Items/[%w_/%.-]+$')or #a[5]>200 or not a[5]:match('^/Game/[%w_/%.-]+$')then return end
 local r={id=a[1],owner=a[2],room=a[3],item=a[4],map=a[5],raw=s}
 for i,n in ipairs({'x','y','z','yaw'})do local v=tonumber(a[i+5]);if not v or v~=v or math.abs(v)>10000000 then return end;r[n]=v end
 return r
end
function M.encode(id,owner,room,item,map,p,yaw)
 return table.concat({id,owner,room,item,map,math.floor(p.X),math.floor(p.Y),math.floor(p.Z),math.floor(yaw)},'~')
end
function M.get(save,key)local p=save.NamedSaveData:Find(FName(key));return p and p:get()or nil end
function M.set(save,key,value)save.NamedSaveData:Add(FName(key),value)end
function M.scan(save)
 local drops,escrow,receipts,winners={},{},{},{}
 local identity,room
 save.NamedSaveData:ForEach(function(k,v)
  local n=k:get():ToString();local value=v:get()
  if n:sub(1,#M.prefix)==M.prefix then local r=M.decode(n:sub(#M.prefix+1));if r then r.state=value;drops[r.id]=r end
  elseif n:match('^QDCoopPersonal~place~')then local r=M.decode(n:match('^QDCoopPersonal~place~(.+)$'));if r and value==1 then escrow[r.id]=r end
  elseif n:match('^QDCoopPersonal~got~')then receipts[n:match('^QDCoopPersonal~got~(.+)$')]=true
  elseif n:match('^QDCoopPersonal~id~')then identity=n:match('^QDCoopPersonal~id~(.+)$')
  elseif n:sub(1,11)=='QDCoopRoom~'then room=n:sub(12)
  elseif n:sub(1,13)=='QDCoopWinner~'then local id,who=n:match('^QDCoopWinner~([^~]+)~([^~]+)$');if id and value==1 then winners[id]=who end end
 end)
 return drops,escrow,receipts,winners,identity,room
end
return M
