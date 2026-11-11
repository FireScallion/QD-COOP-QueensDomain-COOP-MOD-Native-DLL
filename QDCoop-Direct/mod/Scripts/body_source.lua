-- Observe action state at the existing capture cadence. No remote gameplay calls.
return function(dir,valid,full)
 local cat=dofile(dir..'body_catalog.lua');local M={serial=0,active={},scan=1,assets={},byPath={},direct=0,proxy=0}
 for _,c in ipairs(cat.clips)do if c.path~=''then M.byPath[c.path]=c end end
 local function path(o)return valid(o)and full(o):match(' (/Game/[^ ]+)$')or nil end
 local function id(o)local c=M.byPath[path(o)];return c and c.id or 0 end
 function M.observe(anim,montage)
  if not valid(montage)then return end
  local p=path(montage);if not p then return end
  local c=M.byPath[p];M.serial=M.serial+1
  M.active[p]={asset=montage,clip=c,serial=M.serial}
 end
 function M.install(hooks)
  local p='/Script/Engine.AnimInstance:Montage_Play'
  -- UE4SS native post hooks insert the return value immediately after context.
  local a,b=RegisterHook(p,function()end,function(ctx,returnValue,montage)
   if not M.address then return end
   local anim=ctx:get();if anim:GetAddress()~=M.address then return end
   M.direct=M.direct+1;M.observe(anim,montage:get())
  end);hooks[#hooks+1]={p,a,b}
  -- Blueprint PlayMontage uses this proxy and calls Montage_Play in C++ directly,
  -- bypassing the reflected AnimInstance entry above. Observe both actual routes.
  p='/Script/AnimGraphRuntime.PlayMontageCallbackProxy:CreateProxyObjectForPlayMontage'
  a,b=RegisterHook(p,function()end,function(_,returnValue,mesh,montage)
   if not M.meshAddress then return end
   local component=mesh:get();if component:GetAddress()~=M.meshAddress then return end
   M.proxy=M.proxy+1;M.observe(nil,montage:get())
  end);hooks[#hooks+1]={p,a,b}
 end
 function M.reset()M.address=nil;M.meshAddress=nil;M.active={};M.scan=1 end
 function M.capture(player)
  local anim=player.Arms:GetAnimInstance();if not valid(anim)then return nil end
  local address=anim:GetAddress()
  if M.address~=address then M.reset();M.address=address;M.meshAddress=player.Arms:GetAddress()end
  -- Seed actions already in flight before hooks/room activation, once per pawn.
  local stop=math.min(M.scan+7,#cat.clips)
  for i=M.scan,stop do local c=cat.clips[i];if c.montage then
   local a=StaticFindObject(c.path)
   if valid(a)and anim:Montage_IsActive(a)then M.observe(anim,a)end
  end end;M.scan=stop+1
  local current=anim:GetCurrentActiveMontage();local p=path(current)
  if p and not M.active[p]then M.observe(anim,current)end
  local root=id(anim.RootAnim);local left=id(anim.LeftRootAnim)
  local state={root=root,left=left,lanes={},ready=root>0 and left>0 and M.scan>#cat.clips}
  for p,r in pairs(M.active)do
   if not valid(r.asset)or not anim:Montage_IsActive(r.asset)then M.active[p]=nil
   elseif not r.clip or r.clip.group==0 then state.ready=false
   else
    local c=r.clip;local prior=state.lanes[c.group]
    if not prior or r.serial>prior.serial then
     local section=anim:Montage_GetCurrentSection(r.asset):ToString();local si=0
     for i,s in ipairs(c.sections)do if s.name==section then si=i;break end end
     state.lanes[c.group]={id=c.id,serial=r.serial,position=anim:Montage_GetPosition(r.asset),rate=anim:Montage_GetPlayRate(r.asset),section=si,playing=anim:Montage_IsPlaying(r.asset)and 1 or 0}
    end
   end
  end
  local rows={string.format('R|1|%s|%d|%d|%d',cat.hash,root,left,state.ready and 1 or 0)}
  for g=1,7 do local r=state.lanes[g];if r then rows[#rows+1]=string.format('U|%d|%d|%d|%.5f|%.5f|%d|%d',g,r.id,r.serial,r.position,r.rate,r.section,r.playing)end end
  return state,table.concat(rows,'\n')
 end
 return M
end
