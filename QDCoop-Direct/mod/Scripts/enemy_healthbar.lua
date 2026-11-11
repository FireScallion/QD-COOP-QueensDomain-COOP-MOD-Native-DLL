-- Presentation-only adapter for the game's EnemyHealthBar_base widget.
-- Accepted HP snapshots bypass ReportTookDamage, so notify the original bar
-- directly rather than replaying gameplay damage/delegates/rewards.
return function(C,valid,full,recordFor)
 local M={};local bars={}
 local base='/Game/UI/HUD/Widgets/EnemyHealthBar/EnemyHealthBar_base.EnemyHealthBar_base_C'
 local function hide(bar)
  bar:StopAllAnimations()
  bar.CurrentHP=0;bar.DisappearTimer=0;bar.FadedOut=true
  bar.Fill:GetDynamicMaterial():SetScalarParameterValue(FName('Fill'),0)
  bar.DamagedSegments:ClearChildren();bar.CurrentlyRunningDamageSegments={}
  bar:SetRenderOpacity(0);bar.Enemy=nil
 end
 function M.update(r,hp,removed)
  if C.Role~='client'or not valid(r.e)then return end
  local name=full(r.e)
  for key,bar in pairs(bars)do
   if not valid(bar)then bars[key]=nil
   elseif valid(bar.Enemy)and full(bar.Enemy)==name then
    if removed or hp<=0 then hide(bar)
    elseif hp<bar.CurrentHP-.000001 then bar:Damaged(hp,false)
    elseif hp>bar.CurrentHP+.000001 then
     -- Healing/correction is not a backwards damage segment.
     bar.CurrentHP=hp
     if bar.MaxHP>0 then bar.Fill:GetDynamicMaterial():SetScalarParameterValue(FName('Fill'),hp/bar.MaxHP)end
    end
   end
  end
 end
 function M.install(hooks)
  for _,bar in ipairs(FindAllOf('EnemyHealthBar_base_C')or{})do if valid(bar)then bars[full(bar)]=bar end end
  for _,name in ipairs({'Construct','EnemyWasDamaged'})do
   local path=base..':'..name
   local a,b=RegisterHook(path,function(ctx)
    if C.Role~='client'then return end
    local bar=ctx:get();if not valid(bar)then return end;bars[full(bar)]=bar
    local r=valid(bar.Enemy)and recordFor(bar.Enemy)
    -- A late local hit notification must not bring back a confirmed dead bar.
    if r and(r.dead or r.hp and r.hp<=0)then hide(bar)end
   end);hooks[#hooks+1]={path,a,b}
  end
 end
 function M.reset()bars={}end
 return M
end
