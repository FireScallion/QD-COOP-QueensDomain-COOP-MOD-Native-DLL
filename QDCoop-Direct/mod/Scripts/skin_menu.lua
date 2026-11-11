-- One-shot, transparent portraits of the actual bound meshes in one neutral pose.
-- No live preview actors, animation ticks, scene captures, input hooks or network work.
return function(C,L,catalog,selection,create,button,label,valid,log)
 local M={};local root,name,image,choose,chooseLabel,lastLanguage
 local function refresh()
  local s=catalog.get(selection.preview);name:SetText(FText(s.nameKey and L.get(s.nameKey)or s.name))
  local k=StaticFindObject('/Script/Engine.Default__KismetSystemLibrary')
  local texture=k:LoadAsset_Blocking(k:Conv_SoftObjPathToSoftObjRef(k:MakeSoftObjectPath(s.portrait)));assert(valid(texture),'Missing skin portrait '..s.id)
  image:SetBrushFromTexture(texture,false);choose:SetVisibility(selection.changed()and 0 or 2)
  chooseLabel:SetText(FText(L.get('skin_select')));lastLanguage=L.revision
 end
 function M.close()if valid(root)then root:RemoveFromParent()end;root,name,image,choose,chooseLabel=nil,nil,nil,nil,nil end
 function M.open(panel,frame,template)
  M.close();selection.open()
  frame.Slot:SetPosition({X=-265,Y=0})
  root=create('UMG.VerticalBox',panel)
  local slot=frame:GetParent():AddChildToCanvas(root);slot:SetAnchors({Minimum={X=.5,Y=.5},Maximum={X=.5,Y=.5}});slot:SetAlignment({X=.5,Y=.5});slot:SetPosition({X=280,Y=0});slot:SetSize({X=400,Y=640})
  local row=create('UMG.HorizontalBox',panel);root:AddChildToVerticalBox(row):SetHorizontalAlignment(2)
  local function arrow(key,step)
   local b=button(key,function()selection.browse(step);refresh()end,template,nil)
   local size=create('UMG.SizeBox',panel);size:SetWidthOverride(55);size:SetHeightOverride(45);size:AddChild(b);row:AddChildToHorizontalBox(size);return b
  end
  arrow('skin_previous',-1)
  name=label('skin_prince',template,panel)
  local ns=create('UMG.SizeBox',panel);ns:SetWidthOverride(260);ns:SetHeightOverride(45);local nslot=ns:AddChild(name);nslot:SetHorizontalAlignment(2);nslot:SetVerticalAlignment(2);row:AddChildToHorizontalBox(ns)
  arrow('skin_next',1)
  image=create('UMG.Image',panel);image:SetVisibility(4)
  local pic=create('UMG.SizeBox',panel);pic:SetWidthOverride(390);pic:SetHeightOverride(520);pic:AddChild(image);root:AddChildToVerticalBox(pic):SetHorizontalAlignment(2)
  choose,chooseLabel=button('skin_select',function()
   if selection.select()then C.Skin=selection.selected;refresh()else chooseLabel:SetText(FText(L.get('skin_save_error')));log('Skin preference could not be saved')end
  end,template,nil)
  local size=create('UMG.SizeBox',panel);size:SetWidthOverride(260);size:SetHeightOverride(45);size:AddChild(choose);root:AddChildToVerticalBox(size):SetHorizontalAlignment(2)
  refresh()
 end
 function M.tick()if valid(root)and lastLanguage~=L.revision then refresh()end end
 return M
end
