-- Optional native-body capability in each self-contained state snapshot.
-- No event queue/ACK dependency. Old peers ignore N and continue receiving B.
return function(ready,eligible,enabled)
 local M={full=0,lite=0,peer=nil,session=nil,guardUntil=0}
 function M.reset()M.peer=nil;M.session=nil;M.key=nil;M.guardUntil=0 end
 function M.receive(frame,now)
  local n=frame.N
  M.peer={session=frame.T.session,at=now,cap=n and n.cap==1 or false,ack=n and n.ack or 'None'}
 end
 function M.capture(session,action,down,now)
  if M.session~=session then M.session=session;M.key=nil end
  if down and down>0 then M.guardUntil=now+1 end
  local p=M.peer;local fresh=p and now>=p.at and now-p.at<.75
  local cap=ready() and 1 or 0
  local can=enabled() and cap==1 and fresh and p.cap and p.ack==session and now>=M.guardUntil and eligible({action=action})
  local key=can and (action.weapon..'|'..action.montage..'|'..action.serial)or nil
  -- First sample of every action has a complete recovery pose. Unknown actions,
  -- expired capabilities, down/rise, and legacy peers always receive full B.
  local lite=can and M.key==key or false;M.key=key
  if lite then M.lite=M.lite+1 else M.full=M.full+1 end
  return string.format('N|1|%d|%s|%d',cap,fresh and p.session or 'None',lite and 1 or 0),lite
 end
 function M.status()return string.format('poseSourceFull=%d poseSourceLite=%d posePeerCap=%s',M.full,M.lite,tostring(M.peer and M.peer.cap or false))end
 return M
end
