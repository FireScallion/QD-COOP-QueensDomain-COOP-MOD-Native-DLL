-- Engine-owned scratch structs avoid UE4SS's unsafe nested TArray/TMap marshalling.
return function(P)
 local U=StaticFindObject('/Script/FBAttributes.Default__FB_DamageAttributeUtilities')
 local function get(v)if type(v)=='userdata'and v.get then return v:get()end;return v end
 local M={extraObjects=0,emptyData=0}
 function M.capture(native)
  local c=U:AddTagsToDamage(native,{})
  local d={amount=c.DamageAmount,tags={},data={},guid={c.SourceGUID.A,c.SourceGUID.B,c.SourceGUID.C,c.SourceGUID.D}}
  for _,v in pairs(c.DamageTags.GameplayTags or{})do d.tags[#d.tags+1]=get(v).TagName:ToString()end
  for k,v in pairs(c.DataTags or{})do
   local name,value=get(k).TagName:ToString(),get(v)
   -- Original weapon damage can carry a default FGameplayTag mapped to zero.
   -- It has no gameplay meaning. Do not reject the entire valid attack for
   -- this engine-generated placeholder; retain validation of every real tag.
   if(name=='None'or name=='')and value==0 then M.emptyData=M.emptyData+1 else d.data[name]=value end
  end
  for _ in pairs(c.AdditionalData or{})do M.extraObjects=M.extraObjects+1 end
  return d,P.encode(d)
 end
 function M.build(scratch,d)
  scratch:ResetDamage();scratch.DamageScratch.DamageAmount=d.amount
  for _,t in ipairs(d.tags)do scratch:AddDamageTag({TagName=FName(t)})end
  for k,v in pairs(d.data)do scratch:AddDamageData({TagName=FName(k)},v)end
  local g=scratch.DamageScratch.SourceGUID;g.A=d.guid[1];g.B=d.guid[2];g.C=d.guid[3];g.D=d.guid[4]
  return scratch.DamageScratch
 end
 return M
end
