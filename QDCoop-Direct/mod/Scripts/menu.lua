-- Native title entry, OptionsMenu frame and complete native save/new-game flow.
return function(C,P,read,write,log,worldSync,L)
 local steam=dofile(debug.getinfo(1,'S').source:gsub('^@',''):match('^(.*[/\\])')..'steam_menu.lua')(debug.getinfo(1,'S').source:gsub('^@',''):match('^(.*[/\\])'),C,read,log,L)
 local M={};local title,entry,entryLabel,panel,box,heading,frame,message,ip,port
 local divider,dividerPadding
 local phase='home';local queued;local hooks={};local optionsHook=false;local suppressClose=false
 local joiningSteam=false
 local controls,labels={},{};local request='';local generation,started=0,0
 local joinAddress,joinPort,hostPort='127.0.0.1','47771','47771'
 local preferences=dofile(debug.getinfo(1,'S').source:gsub('^@',''):match('^(.*[/\\])')..'join_preferences.lua')
 local preferencesFile=C.Ipc..'/../last-join.txt'
 local savedAddress,savedPort=preferences.load(preferencesFile);if savedAddress then joinAddress,joinPort=savedAddress,savedPort end
 local lastConnected=0;local lastHostFrame,lastHostFrameAt;local statusKey,statusArgs;local revision=-1
 local titlePath='/Game/UI/Title/TitleScreenUI.TitleScreenUI_C:'
 local optionsPath='/Game/UI/Menus/OptionsMenu.OptionsMenu_C:'
 local function valid(o)return o and o:IsValid()end
 local function same(a,b)return valid(a)and valid(b)and a:GetFullName()==b:GetFullName()end
 local function hook(path,fn)
  assert(valid(StaticFindObject(path)),'Missing native UI function '..path)
  local a,b=RegisterHook(path,fn);hooks[#hooks+1]={path,a,b}
 end
 local function create(kind,owner)return StaticConstructObject(StaticFindObject('/Script/'..kind),(owner or panel or title).WidgetTree)end
 local function findText(o)
  if o:IsA('/Script/CommonUI.CommonTextBlock')then return o end
  if o:IsA('/Script/UMG.PanelWidget')then for i=0,o:GetChildrenCount()-1 do local t=findText(o:GetChildAt(i));if t then return t end end end
 end
 local function label(key,template,owner)
  -- Match native buttons: center the desired-size widget in its slot. A filled
  -- text layout with Center justification can drift at the game's UI scale.
  local w=create('CommonUI.CommonTextBlock',owner);w:SetStyle(template.Style);w:SetFont(template.Font);w:SetJustification(0);w:SetText(FText(L.get(key)));return w
 end
 local function focus(w)
  if valid(w)then local u=StaticFindObject('/Game/UI/UIUtilFuncs.Default__UIUtilFuncs_C');u['FS Set Focus'](u,w,true,title)end
 end
 local function button(key,fn,template,parent,height)
  local b=StaticFindObject('/Script/UMG.Default__WidgetBlueprintLibrary'):Create(title,template:GetClass(),title:GetOwningPlayer())
  b:SetStyle(template.Style);b.SpecialSounds=template.SpecialSounds
  local l=label(key,assert(findText(template.Subcontent)),b);local center=create('UMG.Border',b)
  center:SetBrushColor({R=1,G=1,B=1,A=0});center:SetPadding(template.Subcontent:GetChildAt(0).Padding);center:SetHorizontalAlignment(2);center:SetVerticalAlignment(2);center:AddChild(l);b.Subcontent:AddChild(center)
  local translation=findText(template.Subcontent).RenderTransform.Translation;l:SetRenderTranslation({X=translation.X,Y=translation.Y})
  if parent then local size=create('UMG.SizeBox');size:SetHeightOverride(height or 45);size:AddChild(b);local s=parent:AddChildToVerticalBox(size);s:SetHorizontalAlignment(0)end
  controls[b:GetFullName()]={b=b,fn=fn};labels[#labels+1]={w=l,key=key};return b,l
 end
 local function addText(key,height)
  local l=label(key,assert(findText(panel.TopLevel_AudioButton.Subcontent)));l:SetAutoWrapText(false);if height and height>50 then l.WrapTextAt=440 end
  local size=create('UMG.SizeBox');if height then size:SetHeightOverride(height)end;local slot=size:AddChild(l);slot:SetHorizontalAlignment(2);slot:SetVerticalAlignment(2)
  box:AddChildToVerticalBox(size):SetPadding({Left=16,Top=4,Right=16,Bottom=4});labels[#labels+1]={w=l,key=key};return l
 end
 local function field(value)
  local e=create('UMG.EditableTextBox');e.WidgetStyle.Font=findText(panel.TopLevel_AudioButton.Subcontent).Font;e:SetJustification(0);e:SetText(FText(value))
  -- Center the input's desired width too, including symmetric click padding.
  -- It grows with the address instead of justifying text across a filled row.
  e.WidgetStyle.Padding.Left=16;e.WidgetStyle.Padding.Right=16
  local size=create('UMG.SizeBox');size:SetHeightOverride(40);local slot=size:AddChild(e);slot:SetHorizontalAlignment(2);slot:SetVerticalAlignment(2);box:AddChildToVerticalBox(size):SetPadding({Left=35,Top=2,Right=35,Bottom=8});return e
 end
 local function status(key,...)
  statusKey=key;statusArgs={...};local s=key and L.get(key,...)or''
  if valid(message)then message:SetText(FText(s))end;write('menu-status.txt',s)
 end
 local function send(role,address,p,slot)
  lastHostFrame,lastHostFrameAt=nil,nil;generation=generation+1;write('in.state','');write('status.txt','')
  request=table.concat({role,address,p,slot,C.RunId..'-'..generation},'|');write('session.request',request)
 end
 local function cancelJoin()worldSync.ended();write('session.request','');C.Role='menu' end
 local function hostFrame()
  if read('session.ready')~=request then return end
  local network=read('status.txt')
  if network:match('^connected|')then lastConnected=os.time()elseif network~=''or os.time()-lastConnected>2 then return end
  local f=P.frame(read('in.state'));if f then lastHostFrame,lastHostFrameAt=f,os.clock();return f end
  if lastHostFrameAt and os.clock()-lastHostFrameAt<2 then return lastHostFrame end
 end
 local dir=debug.getinfo(1,'S').source:gsub('^@',''):match('^(.*[/\\])')
 local skinMenu=dofile(dir..'skin_menu.lua')(C,L,C.SkinCatalog,C.SkinSelection,create,button,function(key,template,owner)return label(key,assert(findText(template.Subcontent)),owner)end,valid,log)
 local show
 local function closePanel()
  skinMenu.close()
  if valid(panel)and panel:IsInViewport()then suppressClose=true;panel:CloseMenu();suppressClose=false end
  panel,box,heading,frame,message,ip,port,divider,dividerPadding=nil,nil,nil,nil,nil,nil,nil,nil,nil;controls={};labels={}
  if valid(entry)then controls[entry:GetFullName()]={b=entry,fn=function()show('choose')end}end
 end
 local function openPanel()
  if valid(panel)and panel:IsInViewport()then return end
  -- The original event binds close/focus restoration and controller hints.
  -- Customize only its fresh instance; ordinary Options remains unchanged.
  title.BndEvt__TitleScreenUI_Options_K2Node_ComponentBoundEvent_4_CommonButtonBaseClicked__DelegateSignature(title,title.Options)
  panel=FindFirstOf('FS_GameInstance_C').CurrentOptionsMenu;assert(valid(panel),'Native options panel missing')
  frame=panel.OptionsSelectMenu:GetChildAt(0);local canvas=frame:GetChildAt(0)
  for i=0,canvas:GetChildrenCount()-1 do local w=canvas:GetChildAt(i);if w:IsA('/Script/UMG.VerticalBox')then box=w elseif w:IsA('/Script/CommonUI.CommonTextBlock')then heading=w end end
  assert(valid(box)and valid(heading),'Native options layout changed')
  divider=box:GetChildAt(0);assert(divider:IsA('/Script/UMG.Image'),'Native options divider changed');local pad=divider.Slot.Padding;dividerPadding={Left=pad.Left,Top=pad.Top,Right=pad.Right,Bottom=pad.Bottom}
  if not optionsHook then hook(optionsPath..'CloseMenu',function(ctx)
   if same(ctx:get(),panel)and not suppressClose then
    if phase=='waiting'or phase=='join_prepare'or phase=='join'then cancelJoin()end
    queued=phase=='choose'and'home'or'choose'
   end
  end);optionsHook=true end
 end
 local function showSaves(role)
  closePanel();phase=role=='host'and'host_save'or'join_save'
  title.BndEvt__TitleScreenUI_LoadGameButton_K2Node_ComponentBoundEvent_0_CommonButtonBaseClicked__DelegateSignature(title,title.LoadGameButton)
  title.SaveScreenTitle:SetText(FText(L.get(role=='host'and'host_save'or'guest_save')))
  status(role=='client'and'connected'or nil)
 end
 local function joinFailed(key)
  cancelJoin();if valid(title)and title.InSaveMenu then title:CloseModal();title:BackToMainMenu()end
  queued=nil;show(joiningSteam and'choose'or'join');status(joiningSteam and'steam_room_error'or key)
 end
 local function readPort(widget)
  local n=tonumber(widget:GetText():ToString());if not n or n%1~=0 or n<1024 or n>65535 then status('invalid_port');return end;return n
 end
 show=function(screen)
  if screen=='home'then closePanel();phase='home';status(nil);focus(entry);return end
  openPanel();phase=screen;controls={};labels={};box:ClearChildren();ip,port=nil,nil
  box:AddChildToVerticalBox(divider):SetPadding(dividerPadding)
  local key=screen=='choose'and'multiplayer'or(screen=='host'and'host'or'join')
  heading:SetText(FText(L.get(key)));labels[#labels+1]={w=heading,key=key};local first
  if screen=='choose'then
   first=button('host',function()show('host')end,panel.TopLevel_VideoButton,box)
   button('join_friends',function()local r=steam.friends();status(r=='ok'and'steam_friends_hint'or(r=='overlay_disabled'and'steam_overlay_disabled'or'steam_unavailable'))end,panel.TopLevel_VideoButton,box)
   button('join_lan',function()show('join')end,panel.TopLevel_VideoButton,box)
   button('back',function()show('home')end,panel.TopLevel_VideoButton,box)
  elseif screen=='host'then
   addText('host_port',30);port=field(hostPort)
   first=button('host',function()local n=readPort(port);if n then hostPort=tostring(n);C.MenuPort=n;showSaves('host')end end,panel.TopLevel_VideoButton,box)
   button('back',function()show('choose')end,panel.TopLevel_VideoButton,box)
  else
   addText('host_address',30);ip=field(joinAddress);addText('port',30);port=field(joinPort)
   first=button('connect',function()
    if phase~='join'then return end
    local addr=ip:GetText():ToString();local count,good=0,true
    for part in addr:gmatch('[^.]+')do count=count+1;if not part:match('^%d+$')or tonumber(part)>255 then good=false end end
    if not good or count~=4 or not addr:match('^%d+%.%d+%.%d+%.%d+$')then status('invalid_ip');return end
    local n=readPort(port);if not n then return end
    joiningSteam=false;joinAddress,joinPort=addr,tostring(n);if not preferences.save(preferencesFile,joinAddress,joinPort)then log('MENU could not persist join address')end;C.Role='client';phase='waiting';started=os.time();send('client',addr,n,0);status('joining')
   end,panel.TopLevel_VideoButton,box)
   addText('address_hint',110);button('back',function()joinAddress=ip:GetText():ToString();joinPort=port:GetText():ToString();cancelJoin();show('choose')end,panel.TopLevel_VideoButton,box)
  end
  message=label('loading',findText(panel.TopLevel_AudioButton.Subcontent));message:SetAutoWrapText(false);message.WrapTextAt=440;message:SetText(FText(''))
  local msgSize=create('UMG.SizeBox');msgSize:SetHeightOverride(110);local msgSlot=msgSize:AddChild(message);msgSlot:SetHorizontalAlignment(2);msgSlot:SetVerticalAlignment(2);box:AddChildToVerticalBox(msgSize):SetPadding({Left=18,Top=5,Right=18,Bottom=5})
  local height=(screen=='choose'or screen=='host')and 440 or 680
  frame.Slot:SetSize({X=516,Y=height});box.Slot:SetAnchors({Minimum={X=0,Y=0},Maximum={X=1,Y=1}});box.Slot:SetAlignment({X=0,Y=0});box.Slot:SetOffsets({Left=0,Top=68,Right=0,Bottom=10})
  skinMenu.open(panel,frame,panel.TopLevel_VideoButton)
  panel.LastSelectedWidget=first;focus(first);status(nil)
 end
 local function mount(t)
  title=t;controls={};labels={};local parent=title.LoadGameButton:GetParent();assert(parent:IsA('/Script/UMG.VerticalBox'),'Native title button list changed')
  entry,entryLabel=button('multiplayer',function()show('choose')end,title.LoadGameButton,nil)
  local children={};for i=0,parent:GetChildrenCount()-1 do local w=parent:GetChildAt(i);children[#children+1]={w=w,padding=w.Slot.Padding,size=w.Slot.Size,h=w.Slot.HorizontalAlignment,v=w.Slot.VerticalAlignment}end
  parent:ClearChildren()
  for _,c in ipairs(children)do
   local s=parent:AddChildToVerticalBox(c.w);s:SetPadding(c.padding);s:SetSize(c.size);s:SetHorizontalAlignment(c.h);s:SetVerticalAlignment(c.v)
   if same(c.w,title.LoadGameButton)then local q=parent:AddChildToVerticalBox(entry);q:SetPadding(c.padding);q:SetSize(c.size);q:SetHorizontalAlignment(c.h);q:SetVerticalAlignment(c.v)end
  end
  hook('/Game/UI/UI_Style/FS_Button.FS_Button_C:BP_OnClicked',function(ctx)
   local c=controls[ctx:get():GetFullName()];if c and c.b:GetIsEnabled()then queued=c.fn end
  end)
  hook(titlePath..'Load Slot',function(ctx,slot)
   if same(ctx:get(),title)and(phase=='host_save'or phase=='join_save')then
    if phase=='join_save'then worldSync.prepareJoin()end
    C.Role=phase=='host_save'and'host'or'client';C.SelectedSlot=slot:get();phase='loading';status('loading');log('MENU '..C.Role..' selected slot '..C.SelectedSlot)
   end
  end)
  hook(titlePath..'BackToMainMenu',function(ctx)
   if same(ctx:get(),title)and(phase=='host_save'or phase=='join_save')then if phase=='join_save'then cancelJoin()end;queued='choose' end
  end)
  phase='home';revision=L.revision;log('MENU mounted native title entry')
 end
 function M.tick(player)
  L.refresh();skinMenu.tick()
  if not steam.disabled then local ok,err=pcall(steam.tick,player);if not ok then steam.disabled=true;log('STEAM UI disabled after error; LAN preserved: '..tostring(err))end end
  if valid(player)then
   if phase=='loading'and C.Role=='host'then send('host','127.0.0.1',C.MenuPort,C.SelectedSlot);status('opening',C.MenuPort);log('MENU host requested room on UDP '..C.MenuPort)end
   if C.Role~='menu'then phase='session'end;return C.Role=='menu'
  end
  local t=valid(title)and title:IsInViewport()and title or nil
  if not t then for _,candidate in ipairs(FindAllOf('TitleScreenUI_C')or{})do if valid(candidate)and candidate:IsInViewport()then t=candidate;break end end end
  if not valid(t)then return true end
  if phase=='session'then C.Role='menu';write('session.request','');write('out.state','');M.ended=true;phase='home'end
  if not same(title,t)then mount(t)end
  local steamJoin=steam.takeJoin()
  if steamJoin then
   cancelJoin();if title.InSaveMenu then title:CloseModal();title:BackToMainMenu()end
   local other=FindFirstOf('FS_GameInstance_C').CurrentOptionsMenu
   if valid(other)and other:IsInViewport()and not same(other,panel)then other:CloseMenu()end
   joiningSteam=true;queued=nil;show('choose');C.Role='client';phase='waiting';started=os.time();send('steam',steamJoin,0,0);status('joining')
  end
  if queued then local fn=queued;queued=nil;if type(fn)=='string'then show(fn)else fn()end end
  if revision~=L.revision then
   revision=L.revision;if valid(entryLabel)then entryLabel:SetText(FText(L.get('multiplayer')))end
   for _,l in ipairs(labels)do if valid(l.w)then l.w:SetText(FText(L.get(l.key)))end end
   if phase=='host_save'or phase=='join_save'then title.SaveScreenTitle:SetText(FText(L.get(phase=='host_save'and'host_save'or'guest_save')))end
   if statusKey then status(statusKey,table.unpack(statusArgs))end
  end
  if phase=='waiting'or phase=='join_prepare'then
   if phase=='join_prepare'and os.time()-started>30 then joinFailed('world_timeout');return true end
   local error=read('session.error');if error~=''then log('MENU connection error '..error);joinFailed('bridge_error');return true end
   local f=hostFrame()
   if f then
    if phase=='waiting'then phase='join_prepare';started=os.time()end
    local ready=worldSync.prepareJoin();if ready then showSaves('client')else status('receiving_world')end
   elseif os.time()-started>(phase=='waiting'and 45 or 30)then joinFailed(phase=='waiting'and'timeout'or'world_timeout')end
  elseif phase=='join_save'then
   if read('status.txt'):match('^connected|')then lastConnected=os.time()end
   if os.time()-lastConnected>10 then joinFailed('disconnected')end
  end
  return true
 end
 function M.resetUI()
  skinMenu.close()
  steam.resetUI()
  for _,h in ipairs(hooks)do pcall(UnregisterHook,h[1],h[2],h[3])end;hooks={};optionsHook=false
  title,entry,entryLabel,panel,box,heading,frame,message,ip,port,divider,dividerPadding=nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil,nil;controls={};labels={};queued=nil
 end
 return M
end
