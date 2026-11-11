-- Bounded counters at original submission, journal receipt and handler attempt.
-- These are not acknowledgements of gameplay side effects or applied outcomes.
local M={}
local known={VFX=true,SND=true,NPC=true,ER=true,RST=true,THT=true,WC=true,WB=true,DC=true,CP=true,RV=true,IT=true,HIT=true,HELLO=true}
function M.kind(line,incoming)
 if incoming then local at=line:find('|',1,true);if at then line=line:sub(at+1)end end
 local kind=line:match('^([^|\n]+)')
 if not known[kind]then return 'OTHER'end
 if kind=='VFX'then
  local op=line:match('^[^|]*|[^|]*|[^|]*|([^|\n]+)')
  if op=='B'or op=='F'or op=='R'or op=='P'or op=='S'then return kind..'.'..op end
 end
 return kind
end
function M.add(counts,line,incoming)local k=M.kind(line,incoming);counts[k]=(counts[k]or 0)+1 end
function M.status(prefix,counts)
 local keys,out={},{};for k in pairs(counts)do keys[#keys+1]=k end;table.sort(keys)
 for _,k in ipairs(keys)do out[#out+1]=prefix..'.'..k..'='..counts[k]end
 return table.concat(out,' ')
end
return M
