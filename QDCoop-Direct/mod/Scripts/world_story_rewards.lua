-- Host dialogue advances shared story flags; each character owns its inventory.
-- Native Shrineboss-DummyNPC_BP rewards StoneDuckShopUnlock at reward count 0.
-- Checkpoint consumes that duck and writes ShopUnlockFinished/ShopUnlocks.
return function(valid,log)
 local R={};local item,helper,ks
 local receipt='QDCoopPersonal~story~stoneDuck'
 local function named(save,name)local key=FName(name);return save.NamedSaveData:Contains(key)and save.NamedSaveData:Find(key):get()or 0 end
 function R.ensure(gi,player)
  if not valid(gi)or not valid(player)or player.IsDead then return end
  local save=gi.ActiveSaveSlot;if not valid(save)then return end
  local received=named(save,receipt)>0
  -- Never skip the original boss dialogue/reward sequence. Older broken guest
  -- saves can repair themselves once the claimed shared reward is present.
  if named(save,'ShrineBossRewardCount')<1 then return end
  if named(save,'ShopUnlockFinished')>0 then if not received then save.NamedSaveData:Add(FName(receipt),1)end;return 'already-unlocked' end
  if not valid(item)then item=LoadAsset('/Game/Items/KeyItems/ShopProgressionKey/StoneDuckShopUnlock.StoneDuckShopUnlock')end
  if not valid(item)then return end
  local count=save.InventoryContents:Contains(item)and save.InventoryContents:Find(item):get()or 0
  if count<1 then
   if not valid(helper)then helper=StaticFindObject('/Game/Blueprints/SaveSystem/SaveFunctions.Default__SaveFunctions_C')end
   if not valid(ks)then ks=StaticFindObject('/Script/Engine.Default__KismetSystemLibrary')end
   if not valid(helper)or not valid(ks)then return end
   helper:AddItemToInventory(ks:Conv_ObjectToSoftObjectReference(item),1,player)
   if not save.InventoryContents:Contains(item)or save.InventoryContents:Find(item):get()<1 then return end
   log('WORLD restored personal Stone Duck from claimed shrine boss reward')
  end
  -- The receipt and inventory are saved together by the existing checkpoint /
  -- suspend path. No separate disk receipt can survive a rolled-back inventory.
  -- A character may join an earlier world after consuming its duck. If that
  -- world's claimed reward is saved without a shop unlock, an older receipt
  -- must not make the character permanently unable to unlock the shop again.
  if not received then save.NamedSaveData:Add(FName(receipt),1)end;return count<1 and 'granted' or 'already-owned'
 end
 return R
end
