local M={}
local function finite(n,lo,hi)return type(n)=='number'and n==n and n>=lo and n<=hi end
local function tag(s)return type(s)=='string'and #s<=160 and s:match('^Damage%.[%w_.]+$')~=nil end
function M.encode(d)
 if not finite(d.amount,0,10000)then return nil end
 local tags,data={},{};for _,v in ipairs(d.tags or{})do if not tag(v)then return nil end;tags[#tags+1]=v end
 for k,v in pairs(d.data or{})do if not tag(k)or not finite(v,-100000,100000)then return nil end;data[#data+1]=k..'='..string.format('%.8g',v)end
 if #tags>40 or #data>40 then return nil end;table.sort(tags);table.sort(data)
 local g=d.guid or{0,0,0,0};for i=1,4 do if not finite(g[i],-2147483648,4294967295)or g[i]%1~=0 then return nil end end
 local s=string.format('%.8g',d.amount)..'|'..table.concat(tags,',')..'|'..table.concat(data,',')..'|'..table.concat(g,',')
 return #s<=5000 and s or nil
end
function M.decode(amount,tags,data,guid)
 local d={amount=tonumber(amount),tags={},data={},guid={}}
 if type(tags)~='string'or type(data)~='string'or type(guid)~='string'or #tags+#data>4900 then return end
 for s in tags:gmatch('[^,]+')do d.tags[#d.tags+1]=s end
 for s in data:gmatch('[^,]+')do local k,v=s:match('^([^=]+)=([^=]+)$');if not k or d.data[k]then return end;d.data[k]=tonumber(v);if not d.data[k]then return end end
 for s in guid:gmatch('[^,]+')do d.guid[#d.guid+1]=tonumber(s)end
 if #d.guid~=4 or not M.encode(d)then return end;return d
end
return M
