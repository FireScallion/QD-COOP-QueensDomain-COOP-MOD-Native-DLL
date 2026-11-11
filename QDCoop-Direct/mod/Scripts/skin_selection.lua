-- Browsing never changes the equipped skin. Persist only an explicit selection.
return function(catalog,file)
 local function read(path)local f=io.open(path,'rb');if not f then return end;local v=f:read(64);f:close();return v and v:match('^QDSKIN1\n([a-z]+)\n$')end
 local id=read(file);if not catalog.byId[id]then id=read(file..'.bak')end
 local M={selected=catalog.get(id).id};M.preview=M.selected
 function M.browse(step)local s=catalog.get(M.preview);M.preview=catalog.list[(s.index-1+step)%#catalog.list+1].id;return catalog.get(M.preview)end
 function M.open()M.preview=M.selected end
 function M.changed()return M.preview~=M.selected end
 function M.select()
  if not M.changed()then return true end
  local tmp=file..'.new';local f=io.open(tmp,'wb');if not f then return false end
  local ok=f:write('QDSKIN1\n'..M.preview..'\n');local closed=f:close();if not ok or not closed then os.remove(tmp);return false end
  local had=io.open(file,'rb');if had then had:close();os.remove(file..'.bak');if not os.rename(file,file..'.bak')then os.remove(tmp);return false end end
  if not os.rename(tmp,file)then if had then os.rename(file..'.bak',file)end;return false end
  M.selected=M.preview;return true
 end
 return M
end
