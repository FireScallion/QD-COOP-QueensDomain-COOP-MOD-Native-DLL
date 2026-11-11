-- Persistent two-party rescue settlement. Inventory is reconciled against a
-- saved per-item watermark; timeout alone NEVER authorizes a refund.
return function(store,api)
 local M={};local nextSend=0;local peer
 local function split(s)local a={};for v in((s or'')..'~'):gmatch('(.-)~')do a[#a+1]=v end;return a end
 local function outgoing()
  local s=store.get();if not s.out then return end;local a=split(s.out)
  return{donor=s.id,recipient=a[1],serial=tonumber(a[2]),life=a[3],code=tonumber(a[4]),phase=a[5]}
 end
 local function key(t)return'i:'..t.donor..':'..t.serial end
 local function value(t,phase)return table.concat({t.recipient,t.serial,t.life,t.code,phase},'~')end
 local function incoming(t)local s=store.get();local v=s[key(t)];if not v then return end;local a=split(v);if a[1]~=t.recipient or tonumber(a[2])~=t.serial or a[3]~=t.life or tonumber(a[4])~=t.code then return'CONFLICT'end;return a[5]end
 local function putIncoming(t,phase)
  store.change(function(s)s[key(t)]=value(t,phase)end)
 end
 local function send(op,t)if peer and(t.donor==peer or t.recipient==peer)then api.send(op,t)end end
 local function reconcile()
  local s=store.get();for code=1,6 do api.balance(code,tonumber(s['b'..code])or 0,tonumber(s['r'..code])or 0,tonumber(s['n'..code]))end
 end
 local function resolve(t)
  local phase=incoming(t)
  if phase=='D'or phase=='R'then send(phase=='D'and'D2'or'X2',t);return end
  if phase=='CONFLICT'then return end
  if phase~='A'then return end
  if api.receipt(t)then putIncoming(t,'D');send('D2',t);return end
  if api.life()~=t.life then putIncoming(t,'R');send('X2',t);return end
  -- apply() must recognize its native receipt after a partial callback failure.
  if api.apply(t)then putIncoming(t,'D');send('D2',t)end
 end
 local function complete(t,success)
  local out=outgoing()
  if not out or out.serial~=t.serial then
   if t.serial<=(tonumber(store.get().closed)or 0)then send('A2',t)end;return
  end
  if value(out,'')~=value(t,'')or out.donor~=t.donor then return end
  if out.phase=='C'then
   -- Bring an older loaded native save to the last settled quantity before
   -- computing a refund. This also restores potions picked up after that save.
   reconcile()
   local quantity=not success and t.code>0 and api.inventory(t.code)or nil
   store.change(function(s)
    if not success and t.code>0 then
     s['b'..t.code]=tostring((tonumber(s['b'..t.code])or 0)-1)
     s['r'..t.code]=tostring((tonumber(s['r'..t.code])or 0)+1);s['n'..t.code]=tostring(quantity+1)
    end
    s.out=nil;s.closed=tostring(t.serial)
   end)
  elseif(out.phase=='P'and not success)or out.phase=='R'then store.change(function(s)s.out=nil;s.closed=tostring(math.max(tonumber(s.closed)or 0,t.serial))end)
  else return end
  reconcile();send('A2',t)
 end
 function M.identity()return store.get().id end
 function M.active()local out=outgoing();return out and(out.phase=='P'or out.phase=='C')and out or nil end
 function M.begin(recipient,life,code)
  store.ensure()
  if M.active()then return end
  local t={donor=M.identity(),recipient=recipient,life=life,code=code,serial=(tonumber(store.get().serial)or 0)+1}
  store.change(function(s)s.serial=tostring(t.serial);s.out=value(t,'P')end)
  send('P2',t);nextSend=api.now()+.8;return t.serial
 end
 function M.cancel(serial)
  store.ensure()
  local out=outgoing();if out and out.serial==serial and out.phase=='P'then
   store.change(function(s)s.out=value(out,'R');s.closed=tostring(math.max(tonumber(s.closed)or 0,out.serial))end);send('N2',out)
  end
 end
 function M.commit(serial)
  store.ensure()
  local out=outgoing();if not out or out.serial~=serial or out.phase~='P'then return false end
  local quantity=out.code>0 and api.inventory(out.code)or 0
  assert(out.code==0 or quantity>=1,'Selected rescue medicine no longer available')
  store.change(function(s)
   if out.code>0 then
    s['b'..out.code]=tostring((tonumber(s['b'..out.code])or 0)+1)
    s['r'..out.code]=tostring((tonumber(s['r'..out.code])or 0)+1);s['n'..out.code]=tostring(quantity-1)
   end
   s.out=value(out,'C')
  end)
  reconcile();send('C2',out);nextSend=api.now()+.8;return true
 end
 function M.event(op,t)
  store.ensure()
  if not peer then return end
  if t.donor==M.identity()and t.recipient==peer then
   if op=='R2'then local out=M.active();if out and out.phase=='P'and value(out,'')==value(t,'')then api.ready(out)end
   elseif op=='D2'or op=='X2'then complete(t,op=='D2')end
   return
  end
  if t.donor~=peer or t.recipient~=M.identity()then return end
  local phase=incoming(t)
  if phase=='CONFLICT'then return end
  if op=='A2'then
   if phase=='D'or phase=='R'then
    store.change(function(s)
     s['h:'..t.donor]=tostring(math.max(tonumber(s['h:'..t.donor])or 0,t.serial));s[key(t)]=nil
    end)
    api.forget(t)
   end;return
  end
  if not phase and t.serial<=(tonumber(store.get()['h:'..t.donor])or 0)then return end
  if phase=='D'or phase=='R'then send(phase=='D'and'D2'or'X2',t);return end
  if phase=='A'then resolve(t);return end
  if op=='N2'then
   if phase=='A'then resolve(t)else putIncoming(t,'R');send('X2',t)end
  elseif op=='P2'then
   if(phase=='P'and api.life()==t.life)or api.prepare(t)then if not phase then putIncoming(t,'P')end;send('R2',t)
   else putIncoming(t,'R');send('X2',t)end
  elseif op=='C2'then
   if phase=='P'and api.life()==t.life then putIncoming(t,'A');resolve(t)
   elseif phase=='A'then resolve(t)
   else putIncoming(t,'R');send('X2',t)end
  end
 end
 function M.poll(who)
  store.ensure()
  peer=who;reconcile()
  if api.now()<nextSend then return end;nextSend=api.now()+.8
  local out=outgoing()
  if out then
   if out.phase=='C'then send('C2',out)
   elseif out.phase=='P'then send('P2',out)
   elseif out.phase=='R'then send('N2',out)
   elseif out.phase=='D'then send('A2',out)end
  end
  local pending={}
  for k,v in pairs(store.get())do local donor,serial=k:match('^i:([^:]+):(%d+)$');if donor then
   local a=split(v);pending[#pending+1]={donor=donor,recipient=a[1],serial=tonumber(serial),life=a[3],code=tonumber(a[4]),phase=a[5]}
  end end
  for _,t in ipairs(pending)do
   if t.phase=='A'then resolve(t)
   elseif t.phase=='P'and api.life()~=t.life then putIncoming(t,'R');send('X2',t)
   elseif t.phase=='D'or t.phase=='R'then send(t.phase=='D'and'D2'or'X2',t)end
  end
 end
 function M.recoverPrepared()
  -- A charge cannot continue across a process restart / different native pawn.
  local out=outgoing();if out and out.phase=='P'then M.cancel(out.serial)end
 end
 function M.inspect()return store.get()end
 return M
end
