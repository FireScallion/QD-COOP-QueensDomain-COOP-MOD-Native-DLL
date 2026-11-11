return function(dir,rig,valid,C)
 C=C or{}
 local buffer={}
 local M={calls=0,fallbacks=0};local native,headers
 local ok,err=pcall(function()
  native=assert(package.loadlib(dir..'qdpose.dll','qdpose'))
  local fn=StaticFindObject('/Script/Engine.PoseableMeshComponent:SetBoneTransformByName')
  local cls=StaticFindObject('/Script/Engine.PoseableMeshComponent')
  local transform=StaticFindObject('/Script/CoreUObject.Transform')
  local properties={};fn:ForEachProperty(function(p)properties[p:GetFullName():match('([^:]+)$')]=p:GetAddress()end)
  transform:ForEachProperty(function(p)properties[p:GetFullName():match('([^:]+)$')]=p:GetAddress()end)
  local result=native('I'..table.concat({fn:GetAddress(),cls:GetAddress(),properties.BoneName,properties.InTransform,properties.BoneSpace,properties.Rotation,properties.Translation,properties.Scale3D},' '));assert(result=='1',result)
  headers={};for i,b in ipairs(rig.target)do
   local n=FName(b.name);local index=n:GetComparisonIndex();local prefix,suffix=b.name:match('^(.*)_(%d+)$');local number=0
   -- Ask the engine whether it split a numeric suffix (leading-zero names
   -- are not all treated the same). Never infer the FName from spelling alone.
   if prefix and FName(prefix):GetComparisonIndex()==index then number=tonumber(suffix)+1 end
   headers[i]={index,number}
  end
 end)
 if not ok then native=nil;M.error=tostring(err)end;M.available=native~=nil
 function M.apply(pose,indices,positions,rotations)
  if not native or not valid(pose)then M.fallbacks=M.fallbacks+1;return false end
  local values=C.ReuseBodyPose~=false and buffer or{};local at=0
  for _,i in ipairs(indices)do local p,q,h=positions[i],rotations[i],headers[i]
   values[at+1]=h[1];values[at+2]=h[2];values[at+3]=p[1];values[at+4]=p[2];values[at+5]=p[3];values[at+6]=q[1];values[at+7]=q[2];values[at+8]=q[3];values[at+9]=q[4];at=at+9
  end
  -- The reused buffer may have a longer prior pose. Only submit this call's
  -- populated prefix, so a shorter pose cannot include stale bone values.
  local success=native('P',pose:GetAddress(),#indices,table.unpack(values,1,at))=='1'
  if success then M.calls=M.calls+1 else M.fallbacks=M.fallbacks+1 end
  return success
 end
 return M
end
