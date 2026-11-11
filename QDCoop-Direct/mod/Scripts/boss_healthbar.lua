-- Mirror the authority's actual boss HUD membership. Local level enemies are
-- frozen and must not remain the health source of a cooperative boss bar.
return function(C,state,valid,full,emit)
 local M={revision=0,received=0,updates=0};local bars={};local wanted,last,nextAt,lastSend,applying
 local base='/Game/UI/HUD/Widgets/EnemyHealthBar/BossHealthBar.BossHealthBar_C'
 local function usable(bar)return valid(bar)and full(bar):find(' /Engine/Transient.',1,true)and valid(bar.Fill)and valid(bar.DamagedSegments)end
 local function targets(bar)local t={};bar.Targets:ForEach(function(_,v)local e=v:get();if valid(e)then t[#t+1]=e end end);return t end
 local function hide(bar)
  if bar.CurrentHP==0 and bar.FadedOut and bar:GetRenderOpacity()==0 then return end
  bar:StopAllAnimations();bar.CurrentHP=0;bar.FadedOut=true;bar.DisappearTimer=0
  local mat=bar.Fill:GetDynamicMaterial();if valid(mat)then mat:SetScalarParameterValue(FName('Fill'),0)end
  bar.DamagedSegments:ClearChildren();bar.CurrentlyRunningDamageSegments={};bar:SetRenderOpacity(0)
 end
 function M.health()
  if C.Role~='client'or not wanted then return end
  local hp=0;for _,id in ipairs(wanted.ids)do local r=state.replicas[id];hp=hp+(r and math.max(0,r.hp or 0)or 0)end
  for _,bar in pairs(bars)do if usable(bar)and bar==M.bound then
   if not wanted.active or hp<=0 then hide(bar)
   elseif bar.MaxHP>0 and math.abs(bar.CurrentHP-hp)>.000001 then bar:Damaged(hp);M.updates=M.updates+1 end
  end end
 end
 function M.install(hooks)
  for _,bar in ipairs(FindAllOf('BossHealthBar_C')or{})do if usable(bar)then bars[full(bar)]=bar end end
  for _,name in ipairs({'Construct','AddTarget'})do local path=base..':'..name
   local a,b=RegisterHook(path,function(ctx)
    local bar=ctx:get();if usable(bar)then bars[full(bar)]=bar end
    if C.Role=='client'and not applying then M.boundKey=nil;nextAt=0 end
   end);hooks[#hooks+1]={path,a,b}
  end
 end
 function M.event(f)
  if C.Role~='client'or f[5]~='B'or #f~=9 then return false end
  local rev=tonumber(f[6]);if not rev or rev%1~=0 or rev<=M.received or rev>1e9 then return true end
  if f[7]~='0'and f[7]~='1'then return true end
  local group=f[8];if group~=''and(#group>300 or not group:match('^/Game/[%w_/%.-]+$'))then return true end
  local ids,seen={},{};if #f[9]>512 then return true end
  for value in f[9]:gmatch('[^,]+')do local id=tonumber(value);if not id or id%1~=0 or id<1 or id>100000 or seen[id]or #ids>=32 then return true end;seen[id]=true;ids[#ids+1]=id end
  wanted={active=f[7]=='1',group=group,ids=ids,key=f[7]..'|'..group..'|'..f[9]};M.received=rev;nextAt=0;return true
 end
 function M.tick(now)
  if now<(nextAt or 0)then return end;nextAt=now+.2
  local bar;for k,v in pairs(bars)do if usable(v)then bar=v else bars[k]=nil end end;if not bar then return end
  if C.Role=='host'then
   local ids={};for _,e in ipairs(targets(bar))do local r=state.byActor[full(e)];if r then ids[#ids+1]=r.id else return end end;table.sort(ids)
   local group=valid(bar.CurrentGroup)and full(bar.CurrentGroup):match(' (/Game/[%w_/%.-]+)$')or''
   local active=not bar.FadedOut and #ids>0;local body=(active and'1'or'0')..'|'..group..'|'..table.concat(ids,',')
   if body~=last or now>=(lastSend or 0)+2 then M.revision=M.revision+1;emit('B|'..M.revision..'|'..body);last=body;lastSend=now end
  elseif C.Role=='client'and wanted then
   local actors={};for _,id in ipairs(wanted.ids)do local r=state.replicas[id];if not r or not valid(r.e)then if wanted.active then return end else actors[#actors+1]=r.e end end
   local actual=targets(bar);local matches=#actual==#actors
   if matches then local names={};for _,e in ipairs(actors)do names[full(e)]=true end;for _,e in ipairs(actual)do if not names[full(e)]then matches=false;break end end end
   if not matches then
    applying=true
    local ok,err=pcall(function()
     for _,e in ipairs(targets(bar))do bar:Deactivate(bar.CurrentGroup,e)end
     bar.Targets={};bar.CurrentGroup=nil
     local group=wanted.group~=''and StaticFindObject(wanted.group)or nil;if not valid(group)then group=nil end
     for _,e in ipairs(actors)do bar:AddTarget(e,group)end
    end)
    applying=false;if not ok then error(err)end
   end
   M.bound=bar;M.boundKey=wanted.key
   M.health()
  end
 end
 function M.reset()wanted=nil;last=nil;nextAt=nil;lastSend=nil;M.bound=nil;M.boundKey=nil;M.received=0;M.revision=0 end
 return M
end
