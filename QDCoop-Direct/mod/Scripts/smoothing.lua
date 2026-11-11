-- Presentation clock is independent of the network sampling clock.
-- No Unreal objects here: deterministic snapshot buffering can be tested offline.
local M={}
local dir=debug.getinfo(1,'S').source:gsub('^@',''):match('^(.*[/\\])')
local Q=dofile(dir..'pose_math.lua')
local function lerp(a,b,t)return a+(b-a)*t end
local function yaw(a,b,t)return a+((b-a+180)%360-180)*t end
function M.new(options)
 local o=options or {};return setmetatable({delay=o.delay or 0.10,minDelay=o.delay or .10,maxDelay=o.maxDelay or .24,adaptive=o.adaptive~=false,extrapolate=o.extrapolate or 0.075,teleport=o.teleport or 1000,frames={},accepted=0,resets=0,interval=1/30,jitter=0,stats={samples=0,interpolate=0,extrapolate=0,buffer=0}},{__index=M})
end
function M:reset()
 self.frames={};self.session=nil;self.offset=nil;self.lastSeq=nil;self.lastReceived=nil;self.resets=self.resets+1;self.playTime=nil;self.sampleAt=nil;self.interval=1/30;self.jitter=0;self.delay=self.minDelay;self.receiveInterval=nil
end
function M:push(p,t,m,now,bones,view)
 if self.session~=t.session then self:reset();self.session=t.session end
 if self.lastSeq and t.seq<=self.lastSeq then return false end
 local a=self.frames[#self.frames]
 if a and (t.time<=a.time or (p.x-a.x)^2+(p.y-a.y)^2+(p.z-a.z)^2>self.teleport^2) then self:reset();self.session=t.session end
 local measured=now-t.time
 if a and self.lastReceived then
  local sourceGap=t.time-a.time;local arrivalGap=now-self.lastReceived
  self.receiveInterval=self.receiveInterval and self.receiveInterval+(arrivalGap-self.receiveInterval)*.1 or arrivalGap
  self.interval=self.interval+((sourceGap/math.max(t.seq-self.lastSeq,1))-self.interval)*.1
  self.jitter=self.jitter+(math.abs(arrivalGap-sourceGap)-self.jitter)*.1
  if self.adaptive then self.delay=Q.clamp(self.interval*2+self.jitter*4+.015,self.minDelay,self.maxDelay)end
 end
 if not self.offset then self.offset=measured else self.offset=math.min(measured,self.offset+math.min(now-(self.lastReceived or now),0.25)*0.001)end
 self.lastReceived=now;self.lastSeq=t.seq;self.accepted=self.accepted+1
 self.frames[#self.frames+1]={time=t.time,x=p.x,y=p.y,z=p.z,yaw=p.yaw,vx=m.vx,vy=m.vy,vz=m.vz,mode=m.mode,falling=m.falling,dash=m.dash,jumps=m.jumps,lands=m.lands,bones=bones,view=view}
 while #self.frames>64 do table.remove(self.frames,1)end
 return true
end
function M.view(a,b,t)
 if not a or not b then if t<1 then return a else return b end end
 local v={pitch=Q.clamp(lerp(a.pitch,b.pitch,t),-45,45),running=t<1 and a.running or b.running,walkSpeed=lerp(a.walkSpeed,b.walkSpeed,t),weapon=t<1 and a.weapon or b.weapon,parts={}}
 local discrete=t<1 and a or b
 v.skin=discrete.skin
 v.action=discrete.action;v.actionTime=discrete.actionTime;v.body=discrete.body;v.bodyTime=discrete.bodyTime
 for slot,x in pairs(discrete.parts)do
  local y=b.parts[slot];local z={mesh=x.mesh,hand=x.hand,visible=x.visible,transform=x.transform,bones=x.bones,materials=x.materials};v.parts[slot]=z
  if a.weapon==b.weapon and y and x.mesh==y.mesh and x.hand==y.hand then
   z.transform=Q.blendTransform(x.transform,y.transform,t);z.bones={}
   for bone,bt in pairs(x.bones)do z.bones[bone]=y.bones[bone] and Q.blendTransform(bt,y.bones[bone],t) or bt end
  end
 end
 return v
end
function M.bones(a,b,t)
 -- Pose and actor position share the same buffered time. Missing poses mark a
 -- weapon change; do not blend between the spear and an unrelated weapon.
 if not a or not b then if t<1 then return a else return b end end
 local v={};for i=145,147 do v[i]=lerp(a[i],b[i],t)end
 for k=1,37 do
  local start=k<=36 and (k-1)*4+1 or 148
  local q=Q.slerp({a[start],a[start+1],a[start+2],a[start+3]},{b[start],b[start+1],b[start+2],b[start+3]},t)
  for j=0,3 do v[start+j]=q[j+1]end
 end
 return v
end
-- Bounded Hermite tangents prevent overshoot through walls on sudden stops.
function M.position(a,b,va,vb,span,t)
 local d=b-a;if math.abs(d)<1e-6 then return a end
 local x=Q.clamp(va*span/d,0,3);local y=Q.clamp(vb*span/d,0,3);local n=x*x+y*y
 if n>9 then local scale=3/math.sqrt(n);x=x*scale;y=y*scale end
 local t2=t*t;local t3=t2*t
 return (2*t3-3*t2+1)*a+(t3-2*t2+t)*x*d+(-2*t3+3*t2)*b+(t3-t2)*y*d
end
function M.motion(a,b,t)
 local d=t<1 and a or b
 return {vx=lerp(a.vx,b.vx,t),vy=lerp(a.vy,b.vy,t),vz=lerp(a.vz,b.vz,t),mode=d.mode,falling=d.falling,dash=d.dash,jumps=d.jumps,lands=d.lands}
end
function M:finish(p,time)
 p.sampleTime=time;self.stats.samples=self.stats.samples+1;self.stats[p.kind]=self.stats[p.kind]+1;return p
end
function M:sample(now)
 local f=self.frames;if #f==0 then return nil end
 local target=now-self.offset-self.delay
 if not self.playTime then self.playTime=target
 else
  local dt=math.max(0,now-self.sampleAt)
  if dt>.25 or target-self.playTime>.5 then
   self.playTime=math.max(self.playTime,target);self.stats.rebuffers=(self.stats.rebuffers or 0)+1
  else
   -- Slew playback speed instead of stepping the display clock when latency
   -- or jitter changes. This clock drives body, head and weapon together.
   local error=target-(self.playTime+dt)
   local rate=math.abs(error)<.003 and 1 or Q.clamp(1+error*3,.9,1.1)
   self.playTime=self.playTime+dt*rate
  end
 end
 self.sampleAt=now;self.playTime=math.min(self.playTime,f[#f].time+self.extrapolate)
 local time=self.playTime
 while #f>2 and f[2].time<=time do table.remove(f,1)end
 local a,b=f[1],f[2]
 if time<=a.time then return self:finish({X=a.x,Y=a.y,Z=a.z,yaw=a.yaw,motion=a,bones=a.bones,view=a.view,kind='buffer'},time)end
 if b and time<=b.time then
  local t=(time-a.time)/(b.time-a.time)
  local span=b.time-a.time
  local view=M.view(a.view,b.view,t)
  local native=self.nativeBody and self.nativeBody(view,now)
  -- Native animation never consumes B. Retain the original source pose for an
  -- immediate error fallback, and interpolate normally for all legacy actions.
  local bones;if native then bones=a.bones;self.stats.nativeBoneFrames=(self.stats.nativeBoneFrames or 0)+1 else bones=M.bones(a.bones,b.bones,t)end
  return self:finish({X=M.position(a.x,b.x,a.vx,b.vx,span,t),Y=M.position(a.y,b.y,a.vy,b.vy,span,t),Z=M.position(a.z,b.z,a.vz,b.vz,span,t),yaw=yaw(a.yaw,b.yaw,t),motion=M.motion(a,b,t),bones=bones,view=view,kind='interpolate'},time)
 end
 a=f[#f];local dt=math.min(math.max(time-a.time,0),self.extrapolate)
 return self:finish({X=a.x+a.vx*dt,Y=a.y+a.vy*dt,Z=a.z+a.vz*dt,yaw=a.yaw,motion=a,bones=a.bones,view=a.view,kind='extrapolate'},time)
end
function M.state(m)
 if m.dash==1 then return 'dash' end
 if m.falling==1 then return m.vz>20 and 'jump' or 'fall' end
 return m.vx*m.vx+m.vy*m.vy>100 and 'move' or 'idle'
end
return M
