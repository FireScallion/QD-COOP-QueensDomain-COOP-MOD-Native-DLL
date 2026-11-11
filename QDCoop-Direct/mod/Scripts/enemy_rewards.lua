-- Replicate reward opportunities, never the host's random result. The original
-- helper rolls against the receiving character's Luck and personal pity cache.
return function(dir,C,M,valid,full,write,log)
 local R={serial=0,seen={},order={},rolls=0,kills=0};local ks=StaticFindObject('/Script/Engine.Default__KismetSystemLibrary')
 local resources=dofile(dir..'audio_resources.lua')(valid,full,function()end)
 local helperPath='/Game/Blueprints/ItemSystem/SpawnItemDrop.SpawnItemDrop_C:TrySpawningItem'
 local savePath='/Game/Blueprints/SaveSystem/SaveFunctions.SaveFunctions_C:RegisterEnemyKill'
 local keys={'Chance_6_F7D575EC47153AD85A40D99F7290916A','PityModifier_27_B8F9C135469490AF613E2CBEFE28E997','Item_10_B0EB20114658902BDFBB5C9B0C9C4024','CoinCount_23_28CF6E804686C7685DDD0B9549FC3CDB','Chance2_16_57FB355047EE0AD89E8390BC9CD4BB85','PityModifier2_29_C2C42D7C4CDA82A5D6710C9748127E89','Item2_15_C69280724CF002967E616FB459A2A5CA','CoinCount2_24_1BDFAFF14AB42929887CE8910E3E4003'}
 local function path(o)return valid(o)and full(o):match(' (/Game/[%w_/%.-]+)$')end
 local function allowed(s,empty)return empty and s==''or type(s)=='string'and #s<=300 and s:match('^/Game/[%w_/%.-]+$')~=nil end
 local function data(s)local a=resources.load(s);assert(a:IsA('/Game/Enemies/Shared/EnemyData.EnemyData_C'),'Invalid enemy reward data');return a end
 local function send(op,a)
  R.serial=R.serial+1;local f={'ER',M.world,M.session,op,R.serial};for _,v in ipairs(a)do f[#f+1]=type(v)=='number'and string.format('%.9g',v)or v end
  local text=table.concat(f,'|');assert(#text<=1000,'Enemy reward record too large');write('out.events',text..'\n',true)
 end
 local function report()write('enemy-rewards-status.txt','personalRolls='..R.rolls..' bestiaryKills='..R.kills..' xpAwards='..(R.xpAwards or 0)..' rawXP='..(R.xp or 0)..'\nerror='..(R.error or'')..'\n')end
 function R.install(hooks)
  local function hook(p,fn)local a,b=RegisterHook(p,function(...)if not M.connected then return end;local ok,e=pcall(fn,...);if not ok then R.error=tostring(e);report();log('Enemy rewards '..R.error)end end);hooks[#hooks+1]={p,a,b}end
  hook(savePath,function(_,d,setOne,onlyZero,context)
   if C.Role~='host'then return end;local e=context:get();if not valid(e)or not e:IsA('/Game/Enemies/EnemyBase.EnemyBase_C')then return end
   local asset=path(d:get());if asset then
    send('K',{asset,setOne:get()and 1 or 0,onlyZero:get()and 1 or 0})
    if not setOne:get()and not onlyZero:get()and not e:ShouldSkipXPReward()then local amount=e:GetXPReward();if amount>0 then send('X',{amount})end end
   end
  end)
  hook(helperPath,function(_,drop,loc,rot,nav,dropper,bounce,surface,gravity,extra,source,pity,force,skip,override,overridePos,context,first,second)
   if C.Role=='client'and R.applying then
    local s=FindFirstOf('FS_GameInstance_C').ActiveSaveSlot;local a,b=first:get(),second:get()
    if valid(a)then s.EnemyDropDataA:Add(R.applying.data,true)end;if valid(b)then s.EnemyDropDataB:Add(R.applying.data,true)end
    local e=R.applying.actor;if valid(e)then e:SetUpDropFog(a,b)end
    return
   end
   if C.Role~='host'then return end;local e=dropper:get();if not valid(e)or not e:IsA('/Game/Enemies/EnemyBase.EnemyBase_C')then return end;local r=M.byActor[full(e)]
   local asset=path(e.Data);if not asset then return end
   local p,q,x,y=loc:get(),rot:get(),force:get(),overridePos:get()
   local flags=(nav:get()and 1 or 0)+(bounce:get()and 2 or 0)+(surface:get()and 4 or 0)+(gravity:get()and 8 or 0)+(skip:get()and 16 or 0)+(override:get()and 32 or 0)
   local a={r and r.id or 0,asset,source:get():ToString(),p.X,p.Y,p.Z,q.Pitch,q.Yaw,q.Roll,flags,extra:get(),pity:get(),x.X,x.Y,x.Z,y.X,y.Y,y.Z}
   local d=drop:get();for i,k in ipairs(keys)do a[#a+1]=(i==3 or i==7)and ks:Conv_SoftObjectReferenceToString(d[k]):ToString()or d[k]end
   send('D',a)
  end)
 end
 local function mark(key)
  R.seen[key]=true;R.order[#R.order+1]=key;if #R.order>2048 then R.seen[table.remove(R.order,1)]=nil end
 end
 local function applyEvent(f,world,peer)
  if C.Role~='client'or not M.connected or f[2]~='ER'or f[3]~=world or f[4]~=peer then return end
  local serial=tonumber(f[6]);if not serial or serial<1 or serial%1~=0 or serial>1e9 then return end;local key=peer..':'..serial;if R.seen[key]then return end
  if f[5]=='X'and #f==7 then
   local amount=tonumber(f[7]);if not amount or amount%1~=0 or amount<1 or amount>1e7 then return end
   if not valid(M.player)then return 'retry' end
   R.nativeStarted=true;mark(key);M.player:AddXP(amount,false);R.xp=(R.xp or 0)+amount;R.xpAwards=(R.xpAwards or 0)+1;report();return
  end
  if f[5]=='K'and #f==9 and allowed(f[7])and(f[8]=='0'or f[8]=='1')and(f[9]=='0'or f[9]=='1')then
   local d=data(f[7]);local helper=StaticFindObject('/Game/Blueprints/SaveSystem/SaveFunctions.Default__SaveFunctions_C');assert(valid(helper),'Bestiary helper not ready')
   local save=FindFirstOf('FS_GameInstance_C').ActiveSaveSlot
   local function count()local map=save.KilledEnemyCounts;return map:Contains(d)and map:Find(d):get()or 0 end
   local before=count();local expected=(f[9]=='1'and before>0)and before or(f[8]=='1'and 1 or before+1)
   R.nativeStarted=true
   local ok,err=pcall(function()helper:RegisterEnemyKill(d,f[8]=='1',f[9]=='1',M.player)end)
   local after=count()
   if not ok and after==before and expected~=before then R.nativeStarted=false;error(err)end
   mark(key)
   if not ok and after~=expected then error(err)end
   R.kills=R.kills+1;report();return
  end
  if f[5]~='D'or #f~=32 or not allowed(f[8])or #f[9]>300 or f[9]:find('[\r\n]')then return end
  local a={};for i=7,32 do if i~=8 and i~=9 and i~=27 and i~=31 then local n=tonumber(f[i]);if not n or n~=n or math.abs(n)>1e7 then return end;a[i]=n end end
  -- Native drop tables use -1 for the pickup's default quantity (for example
  -- Fisher_base_Data/IronFruit). Rejecting it discarded the ENTIRE two-item roll.
  if a[7]<0 or a[7]%1~=0 or a[16]<0 or a[16]>63 or a[16]%1~=0 or a[28]<-1 or a[28]>10000 or a[28]%1~=0 or a[32]<-1 or a[32]>10000 or a[32]%1~=0 then return end
  if not allowed(f[27],true)or not allowed(f[31],true)then return end
  local d=data(f[8]);local gi=FindFirstOf('FS_GameInstance_C')
  if not valid(R.scratch)then R.scratch=StaticConstructObject(StaticFindObject('/Game/Enemies/Shared/EnemyData.EnemyData_C'),gi);resources.hold(R.scratch)end
  for i,k in ipairs(keys)do local n=24+i
   if i==3 or i==7 then local item=f[n]~=''and resources.load(f[n])or nil;if valid(item)and not item:IsA('/Script/QueensDomain.FS_Item')then error('Invalid reward item')end;R.scratch.Drop[k]=ks:Conv_ObjectToSoftObjectReference(item)
   else R.scratch.Drop[k]=a[n]end
  end
  local flags=a[16];local function flag(bit)return math.floor(flags/bit)%2==1 end
  local r=M.replicas[a[7]];local actor=r and valid(r.e)and r.e or nil
  local helper=StaticFindObject('/Game/Blueprints/ItemSystem/SpawnItemDrop.Default__SpawnItemDrop_C');assert(valid(helper),'Native loot helper not ready')
  R.applying={data=d,actor=actor};R.nativeStarted=true;mark(key)
  local ok,e=pcall(function()
   helper:TrySpawningItem(R.scratch.Drop,{X=a[10],Y=a[11],Z=a[12]},{Pitch=a[13],Yaw=a[14],Roll=a[15]},flag(1),actor or M.player,flag(2),flag(4),flag(8),a[17],FName(f[9]),a[18],{X=a[19],Y=a[20],Z=a[21]},flag(16),flag(32),{X=a[22],Y=a[23],Z=a[24]},M.player,{}, {})
  end);R.applying=nil;if not ok then error(e)end;R.rolls=R.rolls+1;report()
 end
 function R.event(f,world,peer)
  if f[2]~='ER'then return end
  if C.Role=='client'and f[3]==world and f[4]==peer and not M.connected then return 'retry' end
  R.nativeStarted=false
  local ok,result=pcall(applyEvent,f,world,peer);R.applying=nil
  if ok then
   if R.error and R.seen[(peer or'')..':'..tostring(tonumber(f[6]))]then R.error=nil;report()end
   return result
  end
  R.error=tostring(result);report()
  if R.nativeStarted then
   -- A loot roll may have spawned its first item before failing. Preserve the
   -- dedup receipt and quarantine the event instead of rolling a second time.
   error('Enemy reward outcome requires review: '..R.error,0)
  end
  if not R.lastRetryLog or os.clock()-R.lastRetryLog>5 then R.lastRetryLog=os.clock();log('Enemy reward waiting for resources: '..R.error)end
  return 'retry'
 end
 function R.prepareReplicaDeath(e)
  -- Default_Lizer_01's DieAnim has its own delayed treasure roll, independent
  -- of Kill(NoRewards). Keep its native fade/montage but select the ordinary
  -- death branch; the authority's ER event owns this character's single roll.
  if e:IsA('/Game/Enemies/Lizer_00/Default_Lizer_01.Default_Lizer_01_C')and not e.DefaultEnemyMode then e.DefaultEnemyMode=true end
 end
 function R.reset()R.applying=nil;R.scratch=nil;resources.reset();R.seen={};R.order={};R.error=nil end
 return R
end
