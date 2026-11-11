-- The current authority streams one region. Do not silently create a second AI world.
return function(dir,C,valid)
 local N=dofile(dir..'encounter_native.lua')(dir,valid);local M={};local nextCheck=0
 local paths={
 '/Game/Player/FS_PlayerChar.FS_PlayerChar_C:DoTeleporterWingStone',
 '/Game/UI/Menus/Teleporter/TeleporterUI.TeleporterUI_C:GoToDestination',
 '/Game/UI/Menus/SaveMenu.SaveMenu_C:BndEvt__SavedPopup_ShopButton_1_K2Node_ComponentBoundEvent_2_CommonButtonBaseClicked__DelegateSignature'}
 function M.poll(connected)
  if C.Role~='client'or not connected then if M.active then N.restore();M.active=false end;return end
  if os.clock()<nextCheck then return end;nextCheck=os.clock()+1;M.active=true
  for _,p in ipairs(paths)do N.guard(p)end
 end
 function M.reset()N.restore();M.active=false;nextCheck=0 end
 return M
end
