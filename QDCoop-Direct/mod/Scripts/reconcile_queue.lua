-- Coalesce to the newest desired state. A failed object backs off independently
-- and never blocks every other door, lever, or quest notification.
return function(clock,report)
 local M={entries={},order={},at=1,count=0,failures=0,completed=0}
 function M.put(key,fn)
  local old=M.entries[key]
  if not old then M.order[#M.order+1]=key;M.count=M.count+1 end
  M.entries[key]={fn=fn,due=old and old.due or 0,failures=old and old.failures or 0,context=old and old.context or{},logged=old and old.logged}
 end
 function M.run(budget,maxCount)
  local started=clock();local visited,done=0,0;local length=#M.order
  while visited<length and done<(maxCount or 4)and #M.order>0 do
   if M.at>#M.order then M.at=1 end
   local key=M.order[M.at];M.at=M.at+1;visited=visited+1
   local item=M.entries[key]
   if item and clock()>=item.due then
    local ok,err=pcall(item.fn,item.context);done=done+1
    if M.entries[key]==item then
     if ok and err=='retry'then item.due=clock()+.2
     elseif ok then M.entries[key]=nil;M.count=M.count-1;M.completed=M.completed+1
     else
      item.failures=item.failures+1;M.failures=M.failures+1;item.due=clock()+math.min(2,.1*2^math.min(5,item.failures-1))
      if not item.logged or clock()-item.logged>=5 then item.logged=clock();report(key,err,item.failures)end
     end
    end
   end
   if clock()-started>=budget then break end
  end
  -- Bound empty slots without reordering the cursor on every engine frame.
  if #M.order>M.count*2+32 then
   local fresh,kept={},{};for i=0,#M.order-1 do local key=M.order[(M.at+i-1)%#M.order+1];if M.entries[key]and not kept[key]then fresh[#fresh+1]=key;kept[key]=true end end
   M.order=fresh;M.at=1
  end
 end
 function M.clear()M.entries={};M.order={};M.at=1;M.count=0 end
 return M
end
