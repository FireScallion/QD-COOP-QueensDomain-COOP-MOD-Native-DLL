-- Persistent baseline of inspected arena barriers, including actual collision.
-- Applying it never calls Activate/Deactivate and cannot trigger another boss.
local M={classes={
 '/Game/Enemies/Necronomicon/Necro_Boss_Door.Necro_Boss_Door_C',
 '/Game/Enemies/JosamiKnight_00/DesBoss/DesBoss_Gate.DesBoss_Gate_C',
 '/Game/RuinsLevel/Library/BossDoor/Lib_BossDoor_BP.Lib_BossDoor_BP_C',
 '/Game/RuinsLevel/AquaRuins/AquaBossExitDoor.AquaBossExitDoor_C',
 '/Game/RuinsLevel/AquaRuins/GraveyardBoss_Gate.GraveyardBoss_Gate_C',
 '/Game/RuinsLevel/AquaRuins/BossDoor/AquaBossDoor_BP.AquaBossDoor_BP_C',
 '/Game/OtherWorld/CastleBridge/OW_BridgeDoor.OW_BridgeDoor_C',
 '/Game/OtherWorld/Castle_Ext/CastleFrontDoor_BP.CastleFrontDoor_BP_C',
}}
function M.decode(f,W)
 if #f~=8 then return end
 local kind=W.integer(f[2],1,#M.classes);local path=W.unescape(f[3]);local component=W.unescape(f[4]);local hidden=W.integer(f[5],0,1);local collision=W.integer(f[6],0,3);local visible=W.integer(f[7],0,1)
 if not kind or not path or #path>450 or not path:match('^/Game/[%w_/%.-]+:PersistentLevel%.[%w_]+$')or not component or #component>120 or not component:match('^[%w_ ]*$')or not hidden or not collision or not visible then return end
 local values={};for s in(f[8]..','):gmatch('(.-),')do local n=W.number(s,-1e7,1e7);if not n then return end;values[#values+1]=n end
 if #values~=9 then return end
 return {kind=kind,path=path,component=component,hidden=hidden==1,collision=collision,visible=visible==1,values=values}
end
function M.runtime(valid,full,find,gs,W)
 local R={}
 R.count=#M.classes
 function R.capture(player,add,only)
  local primitive=find('/Script/Engine.PrimitiveComponent')
  for i,path in ipairs(M.classes)do local cls=find(path)
   if (not only or only==i)and valid(cls)then local out={};gs:GetAllActorsOfClass(player,cls,out)
    for _,entry in ipairs(out)do local a=entry:get();if valid(a)and not a:IsActorBeingDestroyed()then
     local name=full(a):match(' (/Game/.*)$')
     if name and #name<=450 then
      if i<=6 then add('G',i,W.escape(name),'',a.bHidden and 1 or 0,a:GetActorEnableCollision()and 1 or 0,1,'0,0,0,0,0,0,1,1,1')end
      for _,v in ipairs(a:K2_GetComponentsByClass(primitive))do local c=v:get()
       if valid(c)then local n=c:GetFName():ToString();local l,r,s=c.RelativeLocation,c.RelativeRotation,c.RelativeScale3D
        -- The two bridge doors already use native door presentation. Only the
        -- independent blockers need an additional baseline; do not fight their timelines.
        if n:match('^[%w_ ]+$')and(i<=6 or i==7 and n=='BlockerBox'or i==8 and n=='InteractBox')then add('G',i,W.escape(name),W.escape(n),c.bHiddenInGame and 1 or 0,c:GetCollisionEnabled(),c:IsVisible()and 1 or 0,string.format('%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f',l.X,l.Y,l.Z,r.Pitch,r.Yaw,r.Roll,s.X,s.Y,s.Z))end
       end
      end
     end
    end end
   end
  end
 end
 function R.apply(v)
  local a=find(v.path);local cls=find(M.classes[v.kind]);if not valid(a)or not valid(cls)or not a:IsA(cls)then return false end
  if v.component==''then
   a:SetActorHiddenInGame(v.hidden);a:SetActorEnableCollision(v.collision~=0)
   if v.kind==1 then
    -- The original EnableDoor also activates Necro heads. Reconstruct only
    -- its material visibility input; local mesh bounds provide the same radius.
    local c=a.StaticMeshComponent;local mn,mx={},{};c:GetLocalBounds(mn,mx)
    if mn.X and mx.X then local scale=c:K2_GetComponentScale();local radius=math.sqrt(((mx.X-mn.X)*scale.X)^2+((mx.Y-mn.Y)*scale.Y)^2+((mx.Z-mn.Z)*scale.Z)^2)/2
     c:SetCustomPrimitiveDataFloat(7,v.hidden and 0 or(radius+2000)*1.2)
     c:SetCustomPrimitiveDataVector3(32,c:K2_GetComponentLocation())
    end
   end
  else
   local c
   for _,entry in ipairs(a:K2_GetComponentsByClass(find('/Script/Engine.PrimitiveComponent')))do local o=entry:get();if valid(o)and o:GetFName():ToString()==v.component then c=o;break end end
   if not valid(c)then return false end
   c:SetHiddenInGame(v.hidden,false);c:SetVisibility(v.visible,false);c:SetCollisionEnabled(v.collision)
   local t=v.values;c:K2_SetRelativeLocationAndRotation({X=t[1],Y=t[2],Z=t[3]},{Pitch=t[4],Yaw=t[5],Roll=t[6]},false,{},true);c:SetRelativeScale3D({X=t[7],Y=t[8],Z=t[9]})
  end
  return true
 end
 return R
end
return M
