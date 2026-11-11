-- The return owns its float data; no engine pointer or userdata escapes.
return function(dir,bones,valid,enabled)
 local M={calls=0,reads=0,fallbacks=0};local native,group;local tokens=setmetatable({},{__mode='k'})
 local ok,err=pcall(function()
  assert(enabled~=false,'disabled');native=assert(package.loadlib(dir..'qdread.dll','qdread'));assert(native('T')=='A\0B','binary ABI')
  local function fields(o)local p={};o:ForEachProperty(function(f)p[f:GetFName():ToString()]=f:GetAddress()end);return p end
  local fn=StaticFindObject('/Script/Engine.SceneComponent:GetSocketTransform');local p=fields(fn);local t=fields(StaticFindObject('/Script/CoreUObject.Transform'))
  local args={fn:GetAddress(),StaticFindObject('/Script/Engine.SceneComponent'):GetAddress(),p.InSocketName,p.TransformSpace,p.ReturnValue,t.Rotation,t.Translation,t.Scale3D};assert(#args==8,'socket reflected layout')
  assert(native('I'..table.concat(args,' '))==1,'socket initialization')
  local names={#bones};for _,name in ipairs(bones)do local n=FName(name);local index=n:GetComparisonIndex();local prefix,suffix=name:match('^(.*)_(%d+)$');names[#names+1]=index;names[#names+1]=prefix and FName(prefix):GetComparisonIndex()==index and tonumber(suffix)+1 or 0 end
  group=native('G'..table.concat(names,' '));assert(type(group)=='number'and group>0,'socket group')
 end)
 if not ok then native=nil;M.error=tostring(err)end;M.available=native~=nil
 function M.read(component)
  if not native then M.fallbacks=M.fallbacks+1;return end
  local token=tokens[component]
  if not token then if not valid(component)then return end;token=native('R',component:GetAddress());if type(token)~='number'or token<=0 then M.fallbacks=M.fallbacks+1;return end;tokens[component]=token end
  local data=native('S',token,group)
  if type(data)=='string'and #data==#bones*40 then M.calls=M.calls+1;M.reads=M.reads+#bones;return data end
  M.fallbacks=M.fallbacks+1
 end
 function M.status(prefix)return string.format('%sBatch=%s %sBatches=%d %sBones=%d %sFallbacks=%d %sError=%s',prefix,tostring(M.available),prefix,M.calls,prefix,M.reads,prefix,M.fallbacks,prefix,(M.error or'none'):gsub('[\r\n ]','_'):sub(1,160))end
 return M
end
