-- Fresh session-scoped capabilities. Unknown peers continue receiving full B.
return function(cat,ready,enabled)
 local M={full=0,lite=0}
 function M.reset()M.peer=nil;M.key=nil;M.session=nil end
 function M.receive(frame,now)
  local j=frame.J;M.peer={session=frame.T.session,at=now,cap=j and j.hash==cat.hash and j.cap==1,ack=j and j.ack or 'None'}
 end
 function M.capture(session,action,down,now)
  if M.session~=session then M.session=session;M.key=nil end
  local p=M.peer;local fresh=p and now>=p.at and now-p.at<.75
  local cap=ready()and 1 or 0;local state=action.body
  local can=enabled()and cap==1 and fresh and p.cap and p.ack==session and state and state.ready
  local key=can and (state.root..'|'..state.left..'|'..tostring(down or 0))or nil
  local lite=can and M.key==key or false;M.key=key
  if lite then M.lite=M.lite+1 else M.full=M.full+1 end
  -- N remains version 1, capability zero: preview2 must never omit B for us.
  return string.format('N|1|0|%s|0\nJ|1|%s|%d|%s|%d',fresh and p.session or 'None',cat.hash,cap,fresh and p.session or 'None',lite and 1 or 0),lite
 end
 function M.status()return string.format('poseSourceFull=%d poseSourceLite=%d posePeerCap=%s',M.full,M.lite,tostring(M.peer and M.peer.cap or false))end
 return M
end
