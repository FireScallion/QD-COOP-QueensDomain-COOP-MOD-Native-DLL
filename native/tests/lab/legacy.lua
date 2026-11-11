local M=assert(M1);local dir=M.root..'legacy/'
local function valid(o)return o and o:IsValid()end
local function full(o)return valid(o)and o:GetFullName()or''end
local Capture=dofile(dir..'enemy_capture.lua')(dir,true,valid)
local Place=dofile(dir..'enemy_placement.lua')(dir,true)
local P=dofile(dir..'enemy_protocol.lua');local Timeline=dofile(dir..'enemy_timeline.lua');local Q=dofile(dir..'pose_math.lua')
local Anim=dofile(dir..'enemy_animation.lua')(dir,valid,full,function()end)
local classes,meshes={},{};for i,path in ipairs(P.catalog.classes)do classes[path]=i end;for i,path in ipairs(P.catalog.meshes)do meshes[path]=i end
local function path(o)return full(o):match(' (/[^ ]+)$')end
M.timeline=Timeline.new();M.legacyFrames=0;M.legacyRejected=0;M.legacyApplied=0
for _,r in ipairs(M.entities)do
 r.pose=r.e.Mesh
 r.cid=assert(classes[path(r.e:GetClass())]);r.mid=assert(meshes[path(r.e.Mesh.SkeletalMesh)])
 r.anim=assert(Anim.capture(r,r.e.Mesh),'test requires existing Cocker native animation profile')
end
LoopInGameThreadWithDelay(0,function()
 if not M.legacy then return end
 local now=M.native('N')
 if M.role=='host'then
  if now<(M.legacyCaptureAt or 0)then return end;M.legacyCaptureAt=now+.05
  local batches={};local batch=''
  for _,r in ipairs(M.entities)do
   local s=Capture.capture(r)
   if s then
    local f={'N','M1', '/Game/RuinsLevel/Maps/Ruins_Level_00',r.id,now,r.cid,r.mid,s[2],(s[6]==1 or s[7]==0)and 2 or(s[2]<=0 and 1 or 0),s[8],s[9],s[10],s[12]}
    for j=17,26 do f[#f+1]=s[j]end
    f[24]=0;f[25],f[26],f[27],f[28]=s[14],s[15],s[16],s[3]+s[4]*2
    for _,value in ipairs(r.anim)do f[#f+1]=value end
    f[#f+1]=s[11];f[#f+1]=s[13]
    local line=P.encode(f)..'\n'
    if #batch+#line>1950 then batches[#batches+1]=batch;batch=''end
    batch=batch..line;M.legacyFrames=M.legacyFrames+1
   end
  end
  if #batch>0 then batches[#batches+1]=batch end
  for _,text in ipairs(batches)do assert(M.native('L'..text)==1,'legacy send backpressure')end
 else
  for _=1,256 do local text=M.native('P');if type(text)~='string'then break end
   for line in text:gmatch('[^\n]+')do local f=P.parse(line);if f then M.timeline:push(f,now);M.legacyFrames=M.legacyFrames+1 else M.legacyRejected=M.legacyRejected+1 end end
  end
  local time=M.timeline:clock(now)
  for _,r in ipairs(M.entities)do local s=M.timeline:sample(r.id,time,now);if s then
   local values={s.x,s.y,s.z,s.yaw,s.pitch,s.roll};local last=r.legacyPlacement;local changed=not last
   if last then for i=1,6 do if math.abs(values[i]-last[i])>.001 then changed=true end end end
   if changed then assert(Place.actor(r.e,values));r.legacyPlacement=values end
   local meshChanged=not r.legacyMesh
   if r.legacyMesh then for i=1,10 do if math.abs(s.mesh[i]-r.legacyMesh[i])>.0001 then meshChanged=true end end end
   if meshChanged then assert(Place.mesh(r.pose,s.mesh));r.legacyMesh=s.mesh end
   local f=s.a
   if r.legacyState~=f then r.e.HP=f[8];r.e.bIsAttacking=(f.action or 0)%2==1;r.e.bInRecoveryTime=math.floor((f.action or 0)/2)%2==1;r.e.bIsChasing=true;r.legacyState=f end
   local hidden=f[9]==2;if r.legacyHidden~=hidden then r.e:SetActorHiddenInGame(hidden);r.pose:SetVisibility(not hidden,false);r.legacyHidden=hidden end
   M.legacyApplied=M.legacyApplied+1
  end end
 end
end)
M.switch=function(mode)
 M.legacy=mode=='legacy';M.timeline:reset();M.legacyCaptureAt=0
 for _,r in ipairs(M.entities)do r.legacyPlacement=nil;r.legacyMesh=nil;r.legacyHidden=nil end
 assert(M.native('M '..(M.legacy and 2 or 1))==1)
 return mode..' '..M.native('S')
end
M.legacyStatus=function()return 'frames='..M.legacyFrames..' rejected='..M.legacyRejected..' applied='..M.legacyApplied..' '..Capture.status()..' '..Place.status()end
return 'legacy scalar comparator installed; '..M.legacyStatus()
