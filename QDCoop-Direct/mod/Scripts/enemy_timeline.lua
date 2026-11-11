-- One enemy presentation clock, with per-entity history. Engine-free regression surface.
local dir=debug.getinfo(1,'S').source:gsub('^@',''):match('^(.*[/\\])')
local Q=dofile(dir..'pose_math.lua');local S=dofile(dir..'smoothing.lua')
local M={};M.__index=M
function M.new()return setmetatable({tracks={},delay=.16,interval=.1,jitter=0,stats={accepted=0,stale=0,teleports=0,interpolate=0,hold=0,extrapolate=0}},M)end
function M:reset(session)self.tracks={};self.session=session;self.offset=nil;self.latest=nil;self.arrived=nil;self.play=nil;self.sampleAt=nil;self.delay=.16;self.interval=.1;self.jitter=0 end
function M:push(f,now)
 if self.session~=f[2]then self:reset(f[2])end
 local track=self.tracks[f[4]];if not track then track={frames={}};self.tracks[f[4]]=track end
 local frames=track.frames;local prev=frames[#frames]
 if prev and f[5]<=prev[5]then self.stats.stale=self.stats.stale+1;return false end
 if prev and(f[6]~=prev[6]or f[7]~=prev[7]or f[24]~=prev[24]or f[5]-prev[5]>.75 or(f[10]-prev[10])^2+(f[11]-prev[11])^2+(f[12]-prev[12])^2>1000^2)then frames={};track.frames=frames;track.play=nil;track.sampleAt=nil;track.interval=nil;track.jitter=nil;track.delay=nil;prev=nil;self.stats.teleports=self.stats.teleports+1 end
 if not self.latest or f[5]>self.latest then
  if self.latest and self.arrived then local gap=f[5]-self.latest;local arrival=now-self.arrived
   if gap<.5 then local steps=math.max(1,math.floor(gap/self.interval+.5));self.interval=self.interval+(gap/steps-self.interval)*.08;self.jitter=self.jitter+(math.abs(arrival-gap)-self.jitter)*.1 end
  end
  self.latest=f[5];self.arrived=now;local measured=now-f[5];self.offset=self.offset and math.min(measured,self.offset+.0001)or measured
  self.delay=Q.clamp(self.interval*1.4+self.jitter*3+.02,.14,.30)
 end
 if prev then
  local gap=f[5]-prev[5];local arrival=now-(track.at or now)
  track.interval=track.interval and(track.interval+(math.min(gap,.3)-track.interval)*.15)or gap
  track.jitter=(track.jitter or 0)+(math.abs(arrival-gap)-(track.jitter or 0))*.1
  track.delay=Q.clamp(track.interval*1.35+track.jitter*2+.025,.14,.35)
 end
 local endBone=24+f[24]*10;f.vx=f[endBone+1]or(prev and(f[10]-prev[10])/(f[5]-prev[5]))or 0;f.vy=f[endBone+2]or(prev and(f[11]-prev[11])/(f[5]-prev[5]))or 0;f.vz=f[endBone+3]or(prev and(f[12]-prev[12])/(f[5]-prev[5]))or 0;f.action=f[endBone+4]or 0
 f.mesh={};for j=1,10 do f.mesh[j]=f[13+j]end;f.bones={};for j=1,f[24]do local b={};for k=1,10 do b[k]=f[24+(j-1)*10+k]end;f.bones[j]=b end
 frames[#frames+1]=f;while #frames>24 do table.remove(frames,1)end;track.at=now;self.stats.accepted=self.stats.accepted+1;return true
end
function M:clock(now)
 if not self.latest then return end;local target=now-self.offset-self.delay
 if not self.play then self.play=target else local dt=math.max(0,now-(self.sampleAt or now));local error=target-(self.play+dt)
  if dt>.25 or error>.5 then self.play=math.max(self.play,target)else self.play=self.play+dt*Q.clamp(1+error*3,.9,1.1)end
 end
 self.sampleAt=now;self.play=math.min(self.play,self.latest+.06);return self.play
end
function M:sample(id,time,now)
 local track=self.tracks[id];if not track or not time then return end
 -- Each enemy has its own measured cadence. A fast packet from a different
 -- enemy cannot make a slow stream run out of interpolation history.
 local target=now-self.offset-math.max(self.delay,track.delay or .16)
 local dt=math.max(0,now-(track.sampleAt or now));local play=track.play
 if not play then play=target elseif dt>.25 then play=math.max(play,target)else
  play=play+dt*Q.clamp(1+(target-play-dt)*4,.85,1.15)
 end
 time=math.min(play,track.frames[#track.frames][5]+.1);track.play=time;track.sampleAt=now
 local frames=track.frames;while #frames>2 and frames[2][5]<=time do table.remove(frames,1)end
 local a,b=frames[1],frames[2];if not a then return end
 if b and time>=b[5]then a=b;b=nil end
 local u=b and Q.clamp((time-a[5])/(b[5]-a[5]),0,1)or 0
 local x,y,z=a[10],a[11],a[12];local yaw=a[13];local pitch=a.pitch or 0;local roll=a.roll or 0;local kind='hold'
 if b and time>a[5]then local span=b[5]-a[5];x=S.position(x,b[10],a.vx,b.vx,span,u);y=S.position(y,b[11],a.vy,b.vy,span,u);z=S.position(z,b[12],a.vz,b.vz,span,u);yaw=yaw+((b[13]-yaw+180)%360-180)*u;pitch=pitch+(((b.pitch or 0)-pitch+180)%360-180)*u;roll=roll+(((b.roll or 0)-roll+180)%360-180)*u;kind='interpolate'
 elseif not b and time>a[5]and a[8]>0 then local dt=math.min(time-a[5],.1);x=x+a.vx*dt;y=y+a.vy*dt;z=z+a.vz*dt;kind='extrapolate'end
 self.stats[kind]=self.stats[kind]+1
 local mesh=b and Q.blendTransform(a.mesh,b.mesh,u)or{table.unpack(a.mesh)}
 local linearX=a[10]+(b and(b[10]-a[10])*u or 0);local linearY=a[11]+(b and(b[11]-a[11])*u or 0);local linearZ=a[12]+(b and(b[12]-a[12])*u or 0)
 mesh[1]=mesh[1]+x-linearX;mesh[2]=mesh[2]+y-linearY;mesh[3]=mesh[3]+z-linearZ
 return {a=a,b=b or a,u=u,x=x,y=y,z=z,yaw=yaw,pitch=pitch,roll=roll,mesh=mesh,kind=kind,time=time}
end
return M
