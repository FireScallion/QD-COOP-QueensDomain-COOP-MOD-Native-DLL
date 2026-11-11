-- Session-only policy: menu remains usable; simulation keeps running.
-- Do not force global dilation every tick: combat hit-stop uses the same manager.
return function(C,log,read)
 local ids={}
 local function active()
  if C.MenuMode and read('session.request')==''then return false end
  if C.Role~='host' and C.Role~='client' then return false end
  local s=read(C.NativeStartup and 'save-directory.txt' or 'isolation-verified.txt')
  local function norm(v)return v:gsub('\\','/'):gsub('%s+$',''):gsub('/+$',''):lower()end
  return norm(s)==norm(C.ExpectedSaveDirectory)
 end
 local function hook(path,callback)
  local fn=StaticFindObject(path);assert(fn and fn:IsValid(),'Missing pause function: '..path)
  local a,b=RegisterHook(path,callback);ids[#ids+1]={path,a,b}
 end
 -- Native pre-hook: change the requested pause before the engine acts on it.
 hook('/Script/Engine.GameplayStatics:SetGamePaused',function(ctx,world,paused)
  if active() and paused:get() then paused:set(false);log('NO_PAUSE engine request suppressed')end
 end)
 -- Blueprint hooks run after execution. Undo only the specific pause request,
 -- via its original matching false path (which also notifies affected enemies).
 local base='/Game/LD45/Blueprints/Base_LD45_GM.Base_LD45_GM_C:'
 hook(base..'SetPauseTimeDilation',function(ctx,paused)
  if active() and paused:get() then ctx:get():SetPauseTimeDilation(false);log('NO_PAUSE menu time restored')end
 end)
 hook(base..'SetPausedEnemyTimeDilation',function(ctx,paused)
  if active() and paused:get() then ctx:get():SetPausedEnemyTimeDilation(false);log('NO_PAUSE enemy time restored')end
 end)
 local restoring=false
 hook('/Game/Player/FS_PlayerChar.FS_PlayerChar_C:CheckForOverlayPause',function(ctx)
  if not active()or restoring then return end
  local p=ctx:get();local old=p.ForceNoOverlayPause;restoring=true
  local ok,err=pcall(function()p.ForceNoOverlayPause=true;p:CheckForOverlayPause()end)
  p.ForceNoOverlayPause=old;restoring=false;if not ok then error(err)end
 end)
 -- Photo mode has its own global time request. Limit this correction to an
 -- actually visible modal UI so combat time dilation remains available.
 hook('/Script/Engine.GameplayStatics:SetGlobalTimeDilation',function(ctx,w,value)
  if not active()or value:get()>=.01 then return end
  for _,name in ipairs({'PauseMenu_C','GameMenu_C','JournalMenu_C','Guides_Menu_C','Challenges_Menu_C','OptionsMenu_C','PhotoMode_HUD_C'})do
   for _,o in ipairs(FindAllOf(name)or{})do if o:IsValid()and o:IsInViewport()and o:IsVisible()then value:set(1);log('NO_PAUSE modal global time restored '..name);return end end
  end
 end)
 log('NO_PAUSE ready: menu remains open; simulation continues in this multiplayer profile')
 return ids
end
