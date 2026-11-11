-- Native movement remains authoritative. This state controls appearance only.
return function(Q)
 local M={};M.__index=M
 function M.new()return setmetatable({phase=0,weight=0,lastLand=nil,airTime=0,landing=0,wasAir=false},M)end
 function M:update(m,view,dt,yaw)
  dt=Q.clamp(dt,0,.1);local speed=math.sqrt(m.vx*m.vx+m.vy*m.vy)
  local air=m.falling==1;local running=(view and view.running==1) or m.dash==1
  local base=view and view.walkSpeed or 400;local goal=speed>8 and 1 or 0
  self.weight=self.weight+(goal-self.weight)*(1-math.exp(-14*dt))
  if self.lastLand and m.lands>self.lastLand then self.landing=.32 end;self.lastLand=m.lands
  if self.wasAir and not air then self.landing=.32 end
  self.airTime=air and (self.airTime+dt)or 0;self.wasAir=air
  self.landing=math.max(0,self.landing-dt)
  local radians=(yaw or 0)*math.pi/180
  local forward=m.vx*math.cos(radians)+m.vy*math.sin(radians)
  local right=-m.vx*math.sin(radians)+m.vy*math.cos(radians)
  local reverse=forward< -math.abs(right)*.5
  local angle=speed>8 and math.atan(right,math.abs(forward)+.001)*180/math.pi or 0
  angle=Q.clamp(angle,-45,45)
  local duration=running and .6333333 or .96666664
  local rate=Q.clamp(speed/math.max(base*(running and 1.6 or 1),1),.25,1.8)
  self.phase=(self.phase+dt*rate/duration*(reverse and -1 or 1))%1
  if air then
   local time=m.vz>20 and math.min(.6,.15+self.airTime*2.5)or 1.25
   return {clip='jump',time=time,weight=1,state=m.vz>20 and 'jump' or 'fall',yaw=angle*.35,grounded=false}
  elseif self.landing>0 then
   return {clip='jump',time=2.05+(.32-self.landing)*.7,weight=.60*self.landing/.32,state='land',yaw=0,grounded=true}
  end
  return {clip=running and 'run' or 'walk',time=self.phase*duration,weight=self.weight,state=self.weight<.04 and 'idle' or (running and 'run' or 'walk'),yaw=angle*self.weight,grounded=true}
 end
 return M
end
