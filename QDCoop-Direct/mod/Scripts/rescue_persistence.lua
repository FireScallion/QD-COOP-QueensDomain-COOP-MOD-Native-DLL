return function(dir,C,valid,full,items)
 local Store=dofile(dir..'rescue_store.lua');local M={};local bound;local serial=0
 local prefix='QDCoopPersonal~';local idPrefix=prefix..'rescueChar~';local branchPrefix=prefix..'rescueBranch~'
 local functions=StaticFindObject('/Game/Blueprints/SaveSystem/SaveFunctions.Default__SaveFunctions_C')
 local system=StaticFindObject('/Script/Engine.Default__KismetSystemLibrary')
 local function marker(s,key)local map=s.NamedSaveData;return map:Contains(FName(key))and map:Find(FName(key)):get()or 0 end
 local function set(s,key,v)s.NamedSaveData:Add(FName(key),v)end
 local function unique(s,prefix)
  local result;s.NamedSaveData:ForEach(function(k,v)local n=k:get():ToString();if v:get()==1 and n:sub(1,#prefix)==prefix then
   local id=n:sub(#prefix+1);assert(id:match('^[%w_%-]+$')and #id<=50,'Invalid rescue save identity');assert(not result or result==id,'Ambiguous rescue save identity');result=id
  end end);return result
 end
 local function installIdentity(s,prefix,id)
  local remove={};s.NamedSaveData:ForEach(function(k)local n=k:get():ToString();if n:sub(1,#prefix)==prefix then remove[#remove+1]=n end end)
  for _,n in ipairs(remove)do s.NamedSaveData:Remove(FName(n))end;set(s,prefix..id,1)
 end
 local function fingerprint(path)
  local f=io.open(path,'rb');if not f then return'missing'end
  local bytes=f:read('a');f:close();assert(#bytes<=8000000,'Native save too large for rescue binding');return #bytes..':'..Store.checksum(bytes)
 end
 local function location(gi)
  local index=gi:GetActiveSaveSlotIndex();assert(index>=0,'No native save slot')
  local utils=StaticFindObject('/Script/QueensDomain.BlueprintUtils'):GetCDO()
  local slot=utils:GetSaveSlotName(index,false,false,false,0):ToString():gsub('\\','/')
  local directory=C.ExpectedSaveDirectory:gsub('\\','/'):gsub('/+$','')..'/SaveGames/'
  local path=slot:match('^FULLPATH:(.+)$');local relative=slot
  if path then assert(path:sub(1,#directory):lower()==directory:lower(),'Rescue journal outside isolated saves');relative=path:sub(#directory+1):match('^(.+)%.sav$')end
  assert(relative and relative:match('^[%w_][%w_/%-]*$'),'Unsupported rescue slot path')
  return path or directory..relative..'.sav',directory,index
 end
 function M.bind(gi,player)
  local save=gi.ActiveSaveSlot;assert(valid(save),'No character save for rescue')
  local key=full(save)..'|'..full(player)
  if bound and bound.key==key then bound.player=player;return bound,false end
  local primary,directory,index=location(gi)
  local profile=Store.open(directory..'QDCoopRescue_Profile',{})
  if not profile.get().id then profile.change(function(s)s.id=C.RunId end)end
  local binding=Store.open(primary..'.qdrescue-binding',{})
  local char,branch=unique(save,idPrefix),unique(save,branchPrefix)
  local previous=binding.get();local anchor=fingerprint(primary)
  if not char and save.HasEverSaved and previous.anchor==anchor then char,branch=previous.char,previous.branch end
  serial=serial+1
  char=char or C.RunId..'_'..index..'_'..serial
  branch=branch or C.RunId..'_'..serial
  assert(char:match('^[%w_%-]+$')and branch:match('^[%w_%-]+$'),'Invalid rescue binding')
  -- The install component prevents two copied characters on different machines
  -- from impersonating each other in a recovery exchange.
  local identity=profile.get().id..'_'..char..'_'..branch
  assert(#identity<=150,'Rescue identity too long')
  local initial={id=identity,serial='0',closed='0'}
  for i=1,6 do initial['b'..i]=tostring(marker(save,prefix..'rescueBalance'..i));initial['r'..i]=tostring(marker(save,prefix..'rescueRevision'..i))end
  local ledger=Store.open(primary..'.qdrescue-'..char..'-'..branch,initial)
  if ledger.generation==0 then ledger.change(function()end)end
  assert(ledger.get().id==identity,'Rescue journal identity mismatch')
  binding.change(function(s)s.char=char;s.branch=branch;s.anchor=anchor end)
  installIdentity(save,idPrefix,char);installIdentity(save,branchPrefix,branch)
  bound={key=key,save=save,player=player,gi=gi,char=char,branch=branch,store=ledger,primary=primary,revisions={}}
  for i=1,6 do bound.revisions[i]=marker(save,prefix..'rescueRevision'..i)end
  return bound,true
 end
 local function itemFor(code)
  local item=StaticFindObject(items[code].asset);if not valid(item)then item=LoadAsset(items[code].asset)end;assert(valid(item),'Rescue recovery item unavailable')
  return item
 end
 function M.inventory(code)
  local item=itemFor(code);local map=assert(bound).save.InventoryContents;return map:Contains(item)and map:Find(item):get()or 0
 end
 function M.adjust(code,wanted,revision,quantity)
  local b=assert(bound);local save=b.save;local k=prefix..'rescueBalance'..code;local rk=prefix..'rescueRevision'..code;local applied=b.revisions[code]
  if applied==revision then return end
  assert(applied<revision and quantity and quantity>=0 and quantity%1==0,'Rescue journal is older than native character or has an invalid quantity')
  local item=itemFor(code)
  local inventory=save.InventoryContents;local before=inventory:Contains(item)and inventory:Find(item):get()or 0
  local delta=before-quantity;local oldBalance=marker(save,k)
  if not valid(functions)then functions=StaticFindObject('/Game/Blueprints/SaveSystem/SaveFunctions.Default__SaveFunctions_C')end
  assert(valid(functions),'Rescue inventory helper unavailable')
  local soft=system:Conv_ObjectToSoftObjectReference(item)
  -- Marker first: the original helpers notify inventory listeners only after
  -- their count/tag mutation. Any native save from that notification sees both.
  set(save,k,wanted);set(save,rk,revision)
  local ok,err=pcall(function()
   if delta>0 then functions:TryConsumingItemFromInventory(soft,delta,b.player,{})
   elseif delta<0 then functions:AddItemToInventory(soft,-delta,b.player)end
  end)
  local after=inventory:Contains(item)and inventory:Find(item):get()or 0
  if after~=before-delta then
   set(save,k,oldBalance);set(save,rk,applied)
   if after~=before then inventory:Add(item,before)end
   error(ok and'Rescue inventory delta did not commit'or err)
  end
  b.revisions[code]=revision
  b.player:RefreshSelectedConsumable()
  -- Count usage only when a new deduction actually happened, never on receipts.
  if wanted>oldBalance then for _=1,wanted-oldBalance do b.player:CountUsedConsumable()end end
 end
 local function receiptKey(t)return prefix..'rescueReceipt~'..t.donor..'~'..t.serial end
 function M.receipt(t)return bound and marker(bound.save,receiptKey(t))==1 end
 function M.applied(t)set(assert(bound).save,receiptKey(t),1)end
 function M.forget(t)if bound then bound.save.NamedSaveData:Remove(FName(receiptKey(t)))end end
 function M.rollback(save)
  if not bound then return end
  -- Fork the loaded checkpoint's rescue lineage. The old branch is retained for
  -- crash recovery, but its expense/revive records cannot leak into this rewind.
  serial=serial+1;installIdentity(save,idPrefix,bound.char);installIdentity(save,branchPrefix,C.RunId..'_'..serial)
  bound=nil
 end
 function M.reset()bound=nil end
 function M.current()return bound end
 return M
end
