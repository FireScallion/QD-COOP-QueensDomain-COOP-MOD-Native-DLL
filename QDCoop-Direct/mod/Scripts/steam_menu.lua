-- Steam control UI only. Steam networking lives in the background service.
return function(dir,C,read,log,L)
 local M={};local native=C.SteamOverlay and package.loadlib(dir..'qdsteam.dll','qdsteam')or nil
 if native and C.SteamLibrary then local ok,r=pcall(native,'S'..C.SteamLibrary);log('STEAM game-side init '..tostring(ok)..' '..tostring(r))end
 local nextPoll=0;local lastFile,lastLobby,lastAt='',nil,0;local requested,confirming,accepted
 local pause,button,buttonLabel,buttonSize;local hooks={};local hooked=false;local queuedInvite=false
 local state,lobby='unavailable','0';local revision=-1;local openedAt=0
 local base='/Game/UI/Menus/PauseMenu.PauseMenu_C:'
 local function valid(o)return o and o:IsValid()end
 local function same(a,b)return valid(a)and valid(b)and a:GetFullName()==b:GetFullName()end
 local function hook(path,fn)
  local f=StaticFindObject(path);if not valid(f)then return false end
  local a,b=RegisterHook(path,fn);hooks[#hooks+1]={path,a,b};return true
 end
 local function call(command)if not native then return 'unavailable'end;local ok,r=pcall(native,command);return ok and r or'unavailable'end
 local function receive(id)
  if not id or not id:match('^%d+$')or #id>20 or id=='0'then return end
  if id==lobby and(state=='hosting'or state=='joined')then return end
  if id==lastLobby and os.clock()-lastAt<5 then return end
  lastLobby,lastAt=id,os.clock();requested=id;log('STEAM received room join request')
 end
 function M.available()return native and state~='unavailable'and state~=''end
 function M.friends()return M.available()and call('F')or'unavailable'end
 local function text(o)
  if o:IsA('/Script/CommonUI.CommonTextBlock')then return o end
  if o:IsA('/Script/UMG.PanelWidget')then for i=0,o:GetChildrenCount()-1 do local r=text(o:GetChildAt(i));if r then return r end end end
 end
 local function create(kind,owner)return StaticConstructObject(StaticFindObject('/Script/'..kind),owner.WidgetTree)end
 local function mount(p)
  pause=p;local template=p.ContinueButton;local anchor=template:GetParent();local parent=anchor:GetParent()
  assert(parent:IsA('/Script/UMG.VerticalBox'),'Unexpected pause menu layout')
  local source=assert(text(template.Subcontent));button=StaticFindObject('/Script/UMG.Default__WidgetBlueprintLibrary'):Create(p,template:GetClass(),p:GetOwningPlayer())
  button:SetStyle(template.Style);button.SpecialSounds=template.SpecialSounds
  buttonLabel=create('CommonUI.CommonTextBlock',button);buttonLabel:SetStyle(source.Style);buttonLabel:SetFont(source.Font);buttonLabel:SetJustification(0);buttonLabel:SetText(FText(L.get('invite_partner')))
  local center=create('UMG.Border',button);center:SetBrushColor({R=1,G=1,B=1,A=0});center:SetPadding(template.Subcontent:GetChildAt(0).Padding);center:SetHorizontalAlignment(2);center:SetVerticalAlignment(2);center:AddChild(buttonLabel);button.Subcontent:AddChild(center)
  local tr=source.RenderTransform.Translation;buttonLabel:SetRenderTranslation({X=tr.X,Y=tr.Y})
  buttonSize=create('UMG.SizeBox',p);buttonSize:SetHeightOverride(anchor.HeightOverride);buttonSize:AddChild(button)
  local children={};for i=0,parent:GetChildrenCount()-1 do local w=parent:GetChildAt(i);local s=w.Slot;children[#children+1]={w=w,pad=s.Padding,size=s.Size,h=s.HorizontalAlignment,v=s.VerticalAlignment}end
  parent:ClearChildren()
  for _,c in ipairs(children)do
   local s=parent:AddChildToVerticalBox(c.w);s:SetPadding(c.pad);s:SetSize(c.size);s:SetHorizontalAlignment(c.h);s:SetVerticalAlignment(c.v)
   if same(c.w,anchor)then local b=parent:AddChildToVerticalBox(buttonSize);b:SetPadding(c.pad);b:SetSize(c.size);b:SetHorizontalAlignment(c.h);b:SetVerticalAlignment(c.v)end
  end
  log('STEAM invite button mounted below Continue')
 end
 local function installHooks()
  if hooked then return end
  if not valid(StaticFindObject(base..'ConfirmExitToMenu'))then return end
  hook('/Game/UI/UI_Style/FS_Button.FS_Button_C:BP_OnClicked',function(ctx)if same(ctx:get(),button)then queuedInvite=true end end)
  local function confirm(ctx)if confirming and same(ctx:get(),pause)then accepted=confirming;confirming=nil;requested=nil end end
  hook(base..'ConfirmExitToMenu',confirm);hook(base..'ConfirmSuspendToTitle',confirm)
  hook(base..'BndEvt__OptionsMenu_modalCancelButton_K2Node_ComponentBoundEvent_0_CommonButtonBaseClicked__DelegateSignature',function(ctx)if same(ctx:get(),pause)then confirming=nil;requested=nil end end)
  hooked=true
 end
 function M.tick(player)
  if queuedInvite then queuedInvite=false;if state=='hosting'and C.Role=='host'then local r=call('I'..lobby);if r~='ok'then log('STEAM invite overlay: '..r)end end end
  local now=os.clock();if now<nextPoll then return end;nextPoll=now+.25
  state,lobby=read('steam-ui.txt'):match('^([^|]*)|([^|]*)');state,lobby=state or'unavailable',lobby or'0'
  local r=call('Q');receive(r:match('^join|(%d+)$'))
  local f=read('steam-invite.txt');if f~=''and f~=lastFile then lastFile=f;receive(f:match('|(%d+)$'))end
  if not valid(player)then if accepted then requested=accepted;accepted=nil end;return end
  if not requested and not confirming and not(C.Role=='host'and state=='hosting')then
   if valid(buttonSize)then buttonSize:SetVisibility(1)end;return
  end
  installHooks()
  if not valid(pause)or not pause:IsInViewport()then
   for _,p in ipairs(FindAllOf('PauseMenu_C')or{})do if valid(p)and p:IsInViewport()then if not same(p,pause)then mount(p)end;break end end
  end
  if valid(buttonSize)then
   buttonSize:SetVisibility(C.Role=='host'and state=='hosting'and 0 or 1)
   if revision~=L.revision then revision=L.revision;buttonLabel:SetText(FText(L.get('invite_partner')))end
  end
  if confirming then
   -- Native Cancel/ESC/Continue can close the modal; never keep a stale join
   -- request to hijack a later, unrelated quit-to-title action.
   if now-openedAt>.5 and(not valid(pause)or not pause:IsInViewport()or not pause.ExitConfirmButton:IsVisible())then confirming=nil;requested=nil end
  elseif requested and(not C.SteamJoinAllowed or C.SteamJoinAllowed())and FindFirstOf('FS_GameInstance_C').InCutscene~=true then
   if not valid(pause)or not pause:IsInViewport()then player:OpenPauseMenu(false);return end
   confirming=requested;openedAt=now
   -- Let the original button bind its normal quit/save and cancellation paths.
   pause.BndEvt__PauseMenu_QuitTomenuButton_K2Node_ComponentBoundEvent_8_CommonButtonBaseClicked__DelegateSignature(pause,pause.QuitTomenuButton)
   pause.ConfirmDialogueText:SetText(FText(L.get('steam_leave_confirm')))
  end
 end
 function M.takeJoin()if requested and not confirming then local id=requested;requested=nil;return id end end
 function M.resetUI()
  for _,h in ipairs(hooks)do pcall(UnregisterHook,h[1],h[2],h[3])end;hooks={};hooked=false
  pause,button,buttonLabel,buttonSize=nil,nil,nil,nil;queuedInvite=false;confirming=nil
 end
 return M
end
