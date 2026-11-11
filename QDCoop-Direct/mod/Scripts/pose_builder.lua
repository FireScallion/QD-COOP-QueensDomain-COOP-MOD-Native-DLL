-- Same q*v*conjugate(q) and parent-first hierarchy as pose_math, without the
-- transient quaternion/vector tables per bone. Buffers are owned by this body.
return function(rig,pelvisIndex)
 local positions,rotations,byName={},{},{}
 local M={}
 function M.build(overrides,pelvis)
  for i,b in ipairs(rig.target)do
   local p=positions[i]or{};local q=rotations[i]or{};positions[i]=p;rotations[i]=q;byName[b.name]=p
   local parent=b.parent;local override=overrides[i]
   if override then for k=1,4 do q[k]=override[k]end
   elseif parent>0 then
    local a,z=rotations[parent],b.q;local x,y,t,w=a[1],a[2],a[3],a[4];local u,v,s,h=z[1],z[2],z[3],z[4]
    q[1]=w*u+x*h+y*s-t*v;q[2]=w*v-x*s+y*h+t*u;q[3]=w*s+x*v-y*u+t*h;q[4]=w*h-x*u-y*v-t*s
   else for k=1,4 do q[k]=b.q[k]end end
   if i==pelvisIndex then for k=1,3 do p[k]=pelvis[k]end
   elseif parent>0 then
    local a,z=rotations[parent],b.p;local x,y,t,w=a[1],a[2],a[3],a[4];local vx,vy,vz=z[1],z[2],z[3]
    local u=w*vx+y*vz-t*vy;local v=w*vy-x*vz+t*vx;local s=w*vz+x*vy-y*vx;local h=-x*vx-y*vy-t*vz;local origin=positions[parent]
    p[1]=origin[1]+(-h*x+u*w-v*t+s*y);p[2]=origin[2]+(-h*y+u*t+v*w-s*x);p[3]=origin[3]+(-h*t-u*y+v*x+s*w)
   else for k=1,3 do p[k]=b.p[k]end end
  end
  return positions,rotations,byName
 end
 return M
end
