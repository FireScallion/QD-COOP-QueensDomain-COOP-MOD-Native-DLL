-- 1/16 frames, bounded categories. Work timers exclude measurement bookkeeping.
-- Memory deltas are NOT GC durations. The original presentation budget/cadences
-- remain in force; subtract measured instrumentation bookkeeping from its clock.
return function(P)
 local M={};local clock=P.clock or os.clock;local frame=0;local enabled=false
 local start,last,overhead,mem,stages,actors,category,actorStages,deferred,groups,nearWait,farWait,nearGap
 function M.begin()
  frame=frame+1;enabled=frame%16==1;if not enabled then return end
  nearWait=0;farWait=0;nearGap=0;start=clock();mem=collectgarbage('count');stages={};groups={};actors=0;deferred=0;overhead=0;category=nil;last=clock()
 end
 function M.queue(list,now)
  if not enabled then return end
  for _,r in ipairs(list)do local age=math.max(0,now-(r.nextPresent or now))*1000;if r.presentNear then nearWait=math.max(nearWait,age)else farWait=math.max(farWait,age)end end
 end
 function M.presented(r,now)if enabled and r.presentNear and r.lastPresented then nearGap=math.max(nearGap,(now-r.lastPresented)*1000)end end
 function M.phase(name)
  if not enabled then return end;local at=clock();local d=(at-last)*1000
  stages[name]=(stages[name]or 0)+d;if actorStages then actorStages[name]=(actorStages[name]or 0)+d end
  last=clock();overhead=overhead+last-at
 end
 function M.actor(r)
  if not enabled then return end;M.phase('loop');actors=actors+1;actorStages={};category=(r.native and'native'or'fallback')..'.pending'
 end
 function M.classify(distance,active)
  if not enabled then return end;category=category:match('^[^.]+')..(distance<1800^2 and'.near'or distance<3500^2 and'.mid'or'.far')..(active and'.active'or'.idle')
 end
 function M.endActor()
  if not enabled then return end;M.phase('loop');local at=clock()
  if actorStages then local g=groups[category]or{count=0,stages={}};groups[category]=g;g.count=g.count+1;for k,v in pairs(actorStages)do g.stages[k]=(g.stages[k]or 0)+v end end
  actorStages=nil;last=clock();overhead=overhead+last-at
 end
 function M.deferred()if enabled then deferred=deferred+1 end end
 function M.overhead()return enabled and overhead or 0 end
 function M.finish(count)
  if not enabled then return end;M.phase('tail');local at=clock();local memory=collectgarbage('count')-mem
  for k,v in pairs(stages)do P.add('enemyPresent.'..k,v)end
  for name,g in pairs(groups)do for k,v in pairs(g.stages)do P.add('enemyCategory.'..name..'.'..k,v)end;P.add('enemyCount.'..name,g.count)end
  P.add('enemyAge.nearMaxWaitMs',nearWait);P.add('enemyAge.farMaxWaitMs',farWait);P.add('enemyAge.nearMaxGapMs',nearGap)
  P.add('enemyPresent.work',(at-start-overhead)*1000);P.add('enemyCount.roster',count);P.add('enemyCount.visited',actors);P.add('enemyCount.budgetExit',deferred)
  P.add('enemyMemory.growthKB',math.max(0,memory));P.add('enemyMemory.gcDropKB',math.max(0,-memory));P.add('enemyPresent.probeOverhead',(clock()-at+overhead)*1000)
  actorStages=nil;enabled=false
 end
 return M
end
