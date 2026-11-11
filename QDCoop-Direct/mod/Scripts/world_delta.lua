-- Resolve every asset and construct an undoable delta BEFORE writing the live
-- save. Map operations touch changed keys only; personal keys never enter it.
local M={}
function M.plan(save,snapshot,fields,api)
 local ops,changed,names={},{},{}
 local function add(name,key,before,after,hadBefore,hasAfter)
  ops[#ops+1]={name=name,key=key,before=before,after=after,hadBefore=hadBefore,hasAfter=hasAfter}
 end
 local function canonical(t,v)
  if t=='o'then return api.asset(v)elseif t=='n'then return v:ToString()end
  return v
 end
 local function prepare(t,v)
  if t=='o'then return api.resolve(v)elseif t=='n'then return api.name(v)end
  return v
 end
 local ordered={};for name in pairs(fields)do ordered[#ordered+1]=name end;table.sort(ordered)
 for _,name in ipairs(ordered)do
  local t=fields[name];local wanted=snapshot.fields[name];local current=save[name]
  if #t==1 then
   if canonical(t,current)~=wanted then add(name,nil,current,prepare(t,wanted),true,true)end
  elseif t=='ao'then
   local before,after,paths={},{},{}
   current:ForEach(function(_,v)local value=v:get();before[#before+1]=value;paths[#paths+1]=api.asset(value)end)
   local equal=#paths==#wanted;for i,v in ipairs(wanted)do if paths[i]~=v then equal=false end end
   if not equal then for i,v in ipairs(wanted)do after[i]=api.resolve(v)end;add(name,nil,before,after,true,true)end
  else
   local old={}
   current:ForEach(function(k,v)
    local key=t:sub(1,1)=='g'and api.guid(k:get())or k:get():ToString()
    if not api.personal(name,key)then old[key]={value=v:get()}end
   end)
   for key,value in pairs(wanted)do if not api.personal(name,key)then
    local prior=old[key]
    if not prior or canonical(t:sub(2),prior.value)~=value then
     add(name,t:sub(1,1)=='g'and api.parseGuid(key)or api.name(key),prior and prior.value,prepare(t:sub(2),value),prior~=nil,true)
     if name=='SavableActorStates'then changed[key]=true elseif name=='NamedSaveData'then names[key]=true end
    end
    old[key]=nil
   end end
   for key,prior in pairs(old)do
    add(name,t:sub(1,1)=='g'and api.parseGuid(key)or api.name(key),prior.value,nil,true,false)
    if name=='SavableActorStates'then changed[key]=true elseif name=='NamedSaveData'then names[key]=true end
   end
  end
 end
 return {ops=ops,actors=changed,names=names}
end
function M.commit(save,plan)
 local function put(op,undo)
  local value,exists
  if undo then value,exists=op.before,op.hadBefore else value,exists=op.after,op.hasAfter end
  if op.key==nil then save[op.name]=value
  elseif exists then save[op.name]:Add(op.key,value)
  else save[op.name]:Remove(op.key)end
 end
 for i,op in ipairs(plan.ops)do
  local ok,err=pcall(put,op,false)
  if not ok then
   local failures={}
   -- Include the failing operation: a setter can mutate before reporting error.
   for j=i,1,-1 do local restored,why=pcall(put,plan.ops[j],true);if not restored then failures[#failures+1]=tostring(why)end end
   error(tostring(err)..(#failures>0 and ' [rollback incomplete: '..table.concat(failures,'; ')..']'or ' [live data restored]'),0)
  end
 end
 return #plan.ops
end
return M
