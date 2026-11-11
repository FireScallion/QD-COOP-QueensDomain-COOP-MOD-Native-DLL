-- Library book slots are shared world state. The guest only restores the
-- host's saved book and gate state, without taking or spending a key item.
return function(valid, full)
 local M = {}
 local locked = {}
 local bookClass = 'LIB_BookRecepticle_BP_C'
 local gateClass = 'Lib_TreasureGate_BP_C'

 local function kind(a, name)
  if not valid(a) then return end
  local class = (name or full(a)):match('^([^ ]+) ')
  if class == bookClass then return 'book' end
  if class ~= gateClass then return end
  local record = locked[a:GetAddress()]
  if a.SetPuzzleNSD or (record and record.kind == 'gate') then return 'gate' end
 end

 function M.hostOnly(a)
  return kind(a) ~= nil
 end

 function M.lock(a, name)
  local target = kind(a, name)
  if not target then return end
  local address = a:GetAddress()
  local record = locked[address]
  if not record or not valid(record.actor) then
   record = {actor = a, kind = target}
   if target == 'book' then record.disabled = a.InteractIsDisabled end
   locked[address] = record
  end
  if target == 'gate' then
   -- Native presentation of the host's gate must not write quest progress.
   a.SetPuzzleNSD = false
   return
  end
  a.InteractIsDisabled = true
  local box = a.InteractBox
  if valid(box) then
   local collision = box:GetCollisionEnabled()
   if record.collision == nil then record.collision = collision end
   if collision ~= 0 then box:SetCollisionEnabled(0) end
  end
 end

 function M.reset()
  for _, record in pairs(locked) do
   local a = record.actor
   if valid(a) then
    if record.kind == 'gate' then
     a.SetPuzzleNSD = true
    else
     a.InteractIsDisabled = record.disabled
     if record.collision ~= nil and valid(a.InteractBox) then
      a.InteractBox:SetCollisionEnabled(record.collision)
     end
    end
   end
  end
  locked = {}
 end

 return M
end
