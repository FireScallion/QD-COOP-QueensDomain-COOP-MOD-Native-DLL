-- Swaps one display mesh only when the peer's stable skin ID changes.
-- Native and legacy renderers share the existing prince skeleton and weapon sockets.
return function(dir,valid,log)
 local catalog=dofile(dir..'skin_catalog.lua');local M={};local owner,id,mesh
 local function load(path)
  local k=StaticFindObject('/Script/Engine.Default__KismetSystemLibrary')
  return k:LoadAsset_Blocking(k:Conv_SoftObjPathToSoftObjRef(k:MakeSoftObjectPath(path)))
 end
 function M.reset()owner,id,mesh=nil,nil,nil end
 function M.apply(component,requested)
  local entry=catalog.get(requested)
  if owner==component and id==entry.id and valid(mesh)then return end
  local asset=load(entry.mesh)
  if not valid(asset)then
   if log then log('Skin mesh unavailable: '..entry.id..'; displaying Prince')end
   entry=catalog.list[1];asset=load(entry.mesh)
  end
  assert(valid(asset),'Prince display mesh missing')
  component:SetSkeletalMesh(asset,true)
  component.OverrideMaterials={}
  if entry.face then
   local face=LoadAsset(entry.face);assert(valid(face),'skin face unavailable')
   local index=component:GetMaterialIndex(FName('QD_Face'));assert(index>=0,'skin face slot unavailable');component:SetMaterial(index,face)
  end
  owner,id,mesh=component,entry.id,asset
 end
 return M
end
