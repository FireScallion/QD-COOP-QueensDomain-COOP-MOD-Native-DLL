-- The native primary slot remains the regular save. Immutable copies are paired
-- by a host commit record so a partial save can never replace a shared checkpoint.
return function(C, P, write, log, valid)
 local M = {}
 local WAIT = 15
 local context, pending, prepared, requested
 local queuedRequest, handledRequest, lastPrepare = 0, 0, 0
 local serial, requestSerial, nativeWrites = 0, 0, 0
 local latest, localSaved, writing, syncPending
 local commitNotice
 local message = ''
 local guard=function()return true end
 local deferredSave=false
 local initialCheckpoint=false
 function M.setGuard(fn)guard=fn end
 function M.busy()return pending~=nil or prepared~=nil or requested~=nil end

 local function readFile(path)
  local f = io.open(path, 'rb')
  if not f then return end
  local data = f:read('a')
  f:close()
  return data
 end

 local function writeFile(path, data, mode)
  local f = assert(io.open(path, mode))
  local ok, err = f:write(data)
  if ok then ok, err = f:flush() end
  local closed, closeError = f:close()
  assert(ok, err)
  assert(closed, closeError)
 end

 local function archive(id)
  return context.prefix .. '_' .. id .. '.sav'
 end

 local function hasArchive(id)
  if not id or not P.id(id) or id == '-' then return false end
  local f = io.open(archive(id), 'rb')
  if not f then return false end
  local validHeader = f:read(4) == 'GVAS'
  f:close()
  return validHeader
 end

 local function loadCommit()
  local history = readFile(context.prefix .. '.host.log') or ''
  for id in history:gmatch('QDCP1|([%w_%-]+)\n') do
   if hasArchive(id) then latest = id end
  end
 end

 local function stage(id)
  local data = readFile(context.primary)
  assert(data and data:sub(1, 4) == 'GVAS', 'Native primary save is unreadable')
  local path = archive(id)
  local temporary = path .. '.tmp'
  writeFile(temporary, data, 'wb')
  assert(os.rename(temporary, path))
 end

 local function status()
  write('checkpoint-status.txt', string.format(
   'role=%s ready=%s latest=%s pending=%s nativeWrites=%d\n%s\n',
   C.Role, tostring(context ~= nil and context.ready), latest or '-',
   (pending or prepared or {}).id or '-', nativeWrites, message))
 end

 local function report(s)
  message = s
  log('CHECKPOINT ' .. s)
  status()
 end

 local function send(op, id, request)
  write('out.events', table.concat({
   'CP', context.world, context.epoch, context.peer, op, id or '-', request or 0
  }, '|') .. '\n', true)
 end

 function M.reset()
  context, pending, prepared, requested = nil, nil, nil, nil
  queuedRequest, handledRequest, lastPrepare = 0, 0, 0
  latest, localSaved, syncPending = nil, false, false
  commitNotice=nil
  deferredSave=false
  initialCheckpoint=false
  message = ''
  status()
 end

 function M.savedSlot(name, success)
  if not context or name ~= context.slot or not success then return end
  nativeWrites = nativeWrites + 1
  if not writing then
   if not guard() or not context.ready then deferredSave=true;localSaved=false
   elseif context.ready then localSaved=true end
  end
 end

 local function saveAndStage(id, alreadySaved)
  return pcall(function()
   if not alreadySaved then
    local before = nativeWrites
    writing = true
    local ok, err = pcall(function()
     context.player:Player_WriteStatusToSave()
     context.gi:GI_SaveGameToDisk()
    end)
    writing = false
    assert(ok, err)
    assert(nativeWrites > before, 'Native primary save failed')
   end
   stage(id)
  end)
 end

 local function abortHost(reason)
  local id, request = pending.id, pending.request
  pending = nil
  send('CANCEL', id, request)
  report('cancelled ' .. id .. ' ' .. reason)
 end

 local function beginHost(now, initial)
  serial = serial + 1
  local request, alreadySaved = queuedRequest, localSaved
  handledRequest = math.max(handledRequest, request)
  queuedRequest, localSaved = 0, false
  local transaction = {id = context.epoch .. '_' .. serial, request = request, initial = initial, deadline = now + WAIT, retry = now + 1}
  pending = transaction
  local ok, err = saveAndStage(transaction.id, initial or alreadySaved)
  if pending ~= transaction then return end
  if not ok then abortHost(tostring(err)); return end
  send(initial and 'BASE' or 'PREPARE', pending.id, request)
  report('waiting for guest ' .. pending.id)
 end

 local function finishHost(request)
  local id = pending.id
  handledRequest = math.max(handledRequest, request)
  if queuedRequest <= handledRequest then queuedRequest = 0 end
  local ok, err = pcall(writeFile, context.prefix .. '.host.log', '\nQDCP1|' .. id .. '\n', 'ab')
  if not ok then abortHost(tostring(err)); return end
  latest, pending = id, nil
  commitNotice={id=id,retry=context.now+1,deadline=context.now+WAIT+2}
  send('COMMIT', id)
  report('committed ' .. id)
 end

 local function receivePrepare(id, generation, request, initial)
  if prepared and prepared.id == id then
   send('READY', id, prepared.request)
   return
  end
  if generation <= lastPrepare then return end
  lastPrepare = generation
  prepared = nil
  local usedRequest = requested and requested.number or 0
  if request > usedRequest then
   send('FAILED', id)
   return
  end
  local alreadySaved = localSaved or requested ~= nil
  localSaved, requested = false, nil
  local session = context
  local ok, err = saveAndStage(id, initial or alreadySaved)
  if context ~= session then return end
  if not ok then
   send('FAILED', id, usedRequest)
   report('guest write failed ' .. tostring(err))
   return
  end
  prepared = {id = id, request = usedRequest, deadline = context.now + WAIT + 2}
  send('READY', id, usedRequest)
  report('guest prepared ' .. id)
 end

 function M.event(fields)
  if #fields ~= 8 or fields[2] ~= 'CP' or not context or not context.ready then return end
  if fields[3] ~= context.world or fields[4] ~= context.peer or fields[5] ~= context.epoch then return end
  local op, id = fields[6], fields[7]
  local request = P.number(fields[8], 0, 1e9)
  if not request or request % 1 ~= 0 or not P.id(id) then return end
  if C.Role == 'host' then
   if op == 'HELLO' then
    send('STATE', latest)
   elseif op=='COMMITTED'and commitNotice and id==commitNotice.id then
    commitNotice=nil
   elseif op=='READY'and id==latest then
    send('COMMIT',id)
   elseif op == 'REQUEST' and request > handledRequest then
    queuedRequest = math.max(queuedRequest, request)
   elseif pending and id == pending.id then
    if op == 'READY' and request >= pending.request then finishHost(request)
    elseif op == 'FAILED' then
     handledRequest = math.max(handledRequest, request)
     if queuedRequest <= handledRequest then queuedRequest = 0 end
     abortHost('guest write failed')
    end
   end
   return
  end
  if op == 'STATE' and syncPending and not prepared then
   latest = hasArchive(id) and id or nil
   syncPending = false
   status()
   return
  end
  if id:sub(1, #context.peer + 1) ~= context.peer .. '_' then return end
  local generation = tonumber(id:sub(#context.peer + 2))
  if not generation or generation < 1 or generation % 1 ~= 0 then return end
  if op == 'PREPARE' or op == 'BASE' then
   if not guard()then return end
   receivePrepare(id, generation, request, op == 'BASE')
  elseif op == 'CANCEL' and generation >= lastPrepare then
   lastPrepare, prepared = generation, nil
   if requested and requested.number <= request then requested = nil end
   report('host cancelled ' .. id)
  elseif op == 'COMMIT' and (id==latest or generation==lastPrepare) and hasArchive(id) then
   -- A transport ACK only confirms mailbox delivery. COMMIT may have reached
   -- Lua during a temporary not-ready state, after the prepare timer expired.
   if id~=latest then latest, prepared, syncPending = id, nil, false end
   send('COMMITTED',id)
   report('committed ' .. id)
  end
 end

 function M.poll(gi, player, world, epoch, connected, frame, stable, now)
  if not connected then
   if context then M.reset() end
   return
  end
  local ready = stable and frame and frame.P.world == world
  if not ready or not valid(gi.ActiveSaveSlot) then
   if context and context.ready then context.ready = false; status() end
   return
  end
  local peer = frame.T.session
  local index = gi:GetActiveSaveSlotIndex()
  if index < 0 then return end
  if not context or context.epoch ~= epoch or context.peer ~= peer or context.index ~= index then
   local utils = StaticFindObject('/Script/QueensDomain.BlueprintUtils'):GetCDO()
   local slot = utils:GetSaveSlotName(index, false, false, false, 0):ToString():gsub('\\', '/')
   local directory = C.ExpectedSaveDirectory:gsub('\\', '/'):gsub('/+$', '') .. '/SaveGames/'
   local primary = slot:match('^FULLPATH:(.+)$')
   local relative = slot
   if primary then
    relative = primary:sub(1, #directory):lower() == directory:lower()
     and primary:sub(#directory + 1):match('^(.+)%.sav$') or nil
   end
   if not relative or not relative:match('^[%w_][%w_/%-]*$') or relative:sub(-1) == '/' then
    if context then M.reset() end
    if message ~= 'Unsupported native slot path' then report('Unsupported native slot path') end
    return
   end
   M.reset()
   local parent, leaf = relative:match('^(.*[/])([^/]+)$')
   context = {
    gi = gi, slot = slot, index = index, world = world, epoch = epoch, peer = peer,
    primary = primary or directory .. relative .. '.sav',
    prefix = directory .. (parent or '') .. 'QDCoopCP_' .. (leaf or relative),
    now = now, ready = true, helloAt = 0,
   }
   if C.Role == 'host' then loadCommit();initialCheckpoint=true else syncPending = true end
   status()
  elseif not context.ready then
   context.ready = true
   status()
  end
  context.now = now
  context.player = player
  if not guard()then return end
  if deferredSave then
   deferredSave=false
   -- A save attempted during a transfer must capture the settled inventory,
   -- not stage the older primary bytes after the network award arrives.
   context.player:Player_WriteStatusToSave()
   context.gi:GI_SaveGameToDisk()
  end
  if C.Role == 'host' then
   if commitNotice then
    if now>=commitNotice.deadline then commitNotice=nil
    elseif now>=commitNotice.retry then send('COMMIT',commitNotice.id);commitNotice.retry=now+1 end
   end
   if pending then
    if now >= pending.deadline then abortHost('guest timed out')
    elseif now >= pending.retry then
     send(pending.initial and 'BASE' or 'PREPARE', pending.id, pending.request)
     pending.retry = now + 1
    end
   elseif initialCheckpoint then
    initialCheckpoint=false
    beginHost(now,true)
   elseif localSaved or queuedRequest > 0 then
    beginHost(now)
   end
   return
  end
  if prepared and now >= prepared.deadline then
   prepared, syncPending = nil, true
   report('awaiting host checkpoint status')
  end
  if syncPending and now >= context.helloAt then
   send('HELLO')
   context.helloAt = now + 2
  end
  if requested then
   if now >= requested.deadline then
    requested = nil
    report('host did not accept save request')
   elseif now >= requested.retry then
    send('REQUEST', '-', requested.number)
    requested.retry = now + 1
   end
  elseif localSaved and not prepared then
   localSaved = false
   requestSerial = requestSerial + 1
   requested = {number = requestSerial, retry = now + 1, deadline = now + WAIT}
   send('REQUEST', '-', requestSerial)
   report('requested shared save')
  end
 end

 function M.latest()
  return latest
 end

 function M.open(id)
  if not context or not hasArchive(id) or (C.Role=='host' and id~=latest)then return end
  local gs=StaticFindObject('/Script/Engine.Default__GameplayStatics')
  local save=gs:LoadGameFromSlot('FULLPATH:'..archive(id),0)
  if valid(save) and save:IsA(context.gi.ActiveSaveSlot:GetClass())then return save end
 end

 return M
end
