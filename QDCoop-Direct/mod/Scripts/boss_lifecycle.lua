-- Only classes whose special death/interaction bytecode has been inspected.
-- Keep their native host controller; guest animation remains cosmetic, including
-- zero-HP transitions. Never execute their Kill or story Interact on the guest.
return function(dir,C,M,valid,full,write,log)
 local N=dofile(dir..'encounter_native.lua')(dir,valid)
 local profiles={
  {path='/Game/Enemies/Queen/Queen_BP.Queen_BP_C',fields={'CurrentStateIndex','DoingPhaseAnim','InTrapMode'},component='InteractSphere'},
  {path='/Game/Enemies/Sipeha/Sipeha_BP.Sipeha_BP_C',fields={'CurrentStateIndex'},component='HeadInteract'},
  {path='/Game/Enemies/JosamiLord/JosamiLord_BP.JosamiLord_BP_C',fields={'CurrentStateIndex','ActivePhaseIndex','StandaloneBoss'},component='InteractCollision'},
  {path='/Game/Enemies/WhaleKing/Whaleking_Boss_BP.Whaleking_Boss_BP_C',fields={'CurrentStateIndex','DoingPhaseAttack','Phases Executed On'},component='InteractBox'},
 }
 profiles[#profiles+1]={path='/Game/Enemies/Boner_00/Skeletons24/ShrineBoss/Boner_Shrineboss_BP.Boner_Shrineboss_BP_C',fields={},component='Box'}
 local B={profiles=profiles};local classes,cache={},{};local hooks={};local nextGuard=0;local seen={}
 local function profile(e)
  local key=full(e:GetClass());if cache[key]~=nil then return cache[key]or nil end
  for i,p in ipairs(profiles)do local c=classes[i];if not valid(c)then c=StaticFindObject(p.path);classes[i]=c end;if valid(c)and e:IsA(c)then cache[key]=i;return i end end
  cache[key]=false
 end
 function B.managed(r)
  if r.bossProfile==nil then r.bossProfile=profile(r.e)or false end
  return r.bossProfile~=false
 end
 function B.classManaged(cid)
  local p=M.classPaths[cid];if not p then return false end
  local c=StaticFindObject(p);while valid(c)do for _,v in ipairs(profiles)do if full(c)=='BlueprintGeneratedClass '..v.path then return true end end;c=c:GetSuperStruct()end;return false
 end
 function B.capture(r,f)
  if not B.managed(r)then return end
  local e=r.e;local p=profiles[r.bossProfile];local bits=(e.bDead and 1 or 0)+(e.DeathIsSuspended and 2 or 0)+(e.bIsAttacking and 4 or 0)
  local vals={};for i=1,3 do local name=p.fields[i];local v=name and e[name]or 0;vals[i]=type(v)=='boolean'and(v and 1 or 0)or tonumber(v)or 0 end
  local comp=e[p.component];local collision=valid(comp)and comp:GetCollisionEnabled()or 0
  local life={r.bossProfile,bits,vals[1],vals[2],vals[3],collision}
  for _,v in ipairs({-901,table.unpack(life)})do f[#f+1]=v end
 end
 function B.apply(r,f)
  local life=f.life;if not life or not B.managed(r)or life[1]~=r.bossProfile then return end
  local p=profiles[r.bossProfile];local e=r.e;local bits=life[2]
  e.bDead=bits%2==1;e.DeathIsSuspended=math.floor(bits/2)%2==1;e.bIsAttacking=math.floor(bits/4)%2==1
  for i,name in ipairs(p.fields)do local v=life[2+i];if type(e[name])=='boolean'then e[name]=v==1 else e[name]=v end end
  local c=e[p.component];if valid(c)then c:SetCollisionEnabled(life[6])end
  r.encounterInteractive=life[6]~=0 and(e.bDead or(r.bossProfile==1 and e.InTrapMode))
  r.encounterPhase=e.bDead and(e.DeathIsSuspended and'defeated-interaction'or'final-death')or(f[8]<=0 and'transition'or'combat')
 end
 function B.install()
  if C.Role~='client'then return end
  for _,p in ipairs(profiles)do
   for _,name in ipairs({'Kill','Interact'})do
    local path=p.path..':'..name
    if not hooks[path]and N.guard(path)then
     if name=='Interact'then
      local a,b=RegisterHook(path,function(ctx)
       if not M.connected then return end
       local r=M.byActor[full(ctx:get())];if not r or not r.encounterInteractive then return end
       local now=os.clock();if now<(r.interactSent or 0)+.5 then return end;r.interactSent=now
       write('out.events','BI|'..M.world..'|'..M.peer..'|'..r.id..'|'..string.format('%.6f',now)..'\n',true)
      end);hooks[path]={a,b}
     else hooks[path]=true end
    end
   end
  end
 end
 function B.poll(now)if now>=nextGuard then nextGuard=now+1;B.install()end end
 function B.event(f,world,peer,position)
  if C.Role~='host'or not M.connected or #f~=6 or f[2]~='BI'or f[3]~=world or f[4]~=M.session or not position then return end
  local id=tonumber(f[5]);local r=id and M.actors[id];if not r or not valid(r.e)or not B.managed(r)then return end
  local key=f[4]..'|'..f[5]..'|'..f[6];if seen[key]then return end
  local e=r.e;local pos=e:K2_GetActorLocation();if(pos.X-position.X)^2+(pos.Y-position.Y)^2+(pos.Z-position.Z)^2>1800^2 then return end
  if not(e.bDead or(r.bossProfile==1 and e.InTrapMode and not e.bIsAttacking))then return end
  if r.interactionAccepted and os.clock()-r.interactionAccepted<2 then return end
  seen[key]=true;r.interactionAccepted=os.clock()
  -- Native dialogue remains on the authority; its completion owns the story.
  e:Interact(M.player);log('BOSS guest interaction accepted id='..r.id)
 end
 function B.reset()
  N.restore();for path,h in pairs(hooks)do if type(h)=='table'then UnregisterHook(path,h[1],h[2])end end
  hooks={};cache={};classes={};seen={};nextGuard=0
 end
 return B
end
