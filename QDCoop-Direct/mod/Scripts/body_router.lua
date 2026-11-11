-- Native spear pilot. Unsupported actions retain the complete legacy renderer.
return function(dir,valid,full,C)
 C=C or{}
 local Q=dofile(dir..'pose_math.lua')
 local weapons=dofile(dir..'weapon_visuals.lua')(dir,Q,valid,full,C)
 local legacy=dofile(dir..'body_legacy.lua')(dir,valid,full,C,weapons)
 local native=dofile(dir..'body_native.lua')(dir,valid,full,C,weapons)
 local M={native=native,legacy=legacy,effectTarget=weapons.effectTarget}
 local transport=dofile(dir..'body_transport.lua')(native.catalog,function()return C.NativeSpear~=false and native.ready and not native.failed end,function()return C.NativePoseSource~=false end)
 M.transport=transport
 function M.observe(frame,now)transport.receive(frame,now)end
 function M.captureMode(session,a,down,now)return transport.capture(session,a,down,now)end
 function M.preload(gi)native.preload(gi)end
 function M.capture(player,lite)return legacy.capture(player,lite)end
 function M.usesNative(view,now,downId)
  return native.eligibleView(view)
 end
 function M.reset()
  native.releasePresentation();legacy.releasePresentation();legacy.reset();transport.reset();M.lastFullBones=nil
  M.pose=nil;M.driver=nil;M.mode=nil;M.risingUntil=nil;M.wasDown=false
 end
 function M.apply(actor,base,snapshot,now,downId)
  if snapshot.bones then M.lastFullBones=snapshot.bones end
  M.sourceAction=snapshot.view and snapshot.view.action;M.sourceTime=snapshot.view and snapshot.view.actionTime;M.sourceDown=downId
  if M.wasDown and (downId or 0)==0 then M.risingUntil=now+.50 end;M.wasDown=(downId or 0)>0
  local useNative=native.eligible(snapshot)
  local function fallbackSnapshot()
   if snapshot.bones or not M.lastFullBones then return snapshot end
   local copy={};for k,v in pairs(snapshot)do copy[k]=v end;copy.bones=M.lastFullBones;return copy
  end
  local mode=useNative and 'native' or 'legacy'
  if M.mode~=mode then
   if M.mode=='native'then native.releasePresentation()elseif M.mode=='legacy'then legacy.releasePresentation()end
   M.mode=mode
  end
  local renderer=useNative and native or legacy
  if useNative then
   local ok,err=pcall(renderer.apply,actor,base,snapshot,now,downId)
   if not ok then
    native.failed=true;native.error=tostring(err);native.releasePresentation();M.mode='legacy';renderer=legacy
    print('[QDCoop] Native spear fell back: '..native.error..'\n');renderer.apply(actor,base,fallbackSnapshot(),now,downId)
   end
  else renderer.apply(actor,base,fallbackSnapshot(),now,downId)end
  M.pose=renderer.pose;M.driver=renderer.driver;M.state=renderer.state;M.pitch=renderer.pitch
  M.boneWrites=renderer.boneWrites or 0
 end
 function M.status()return 'bodyMode='..tostring(M.mode)..' '..native.status()..' '..legacy.status()..' '..transport.status()end
 return M
end
