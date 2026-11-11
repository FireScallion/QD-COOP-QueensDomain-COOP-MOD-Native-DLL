local M={}
function M.clamp(x,a,b)return math.max(a,math.min(b,x))end
function M.mul(a,b)return {a[4]*b[1]+a[1]*b[4]+a[2]*b[3]-a[3]*b[2],a[4]*b[2]-a[1]*b[3]+a[2]*b[4]+a[3]*b[1],a[4]*b[3]+a[1]*b[2]-a[2]*b[1]+a[3]*b[4],a[4]*b[4]-a[1]*b[1]-a[2]*b[2]-a[3]*b[3]}end
function M.inv(q)return {-q[1],-q[2],-q[3],q[4]}end
function M.rot(q,v)local r=M.mul(M.mul(q,{v[1],v[2],v[3],0}),M.inv(q));return {r[1],r[2],r[3]}end
function M.add(a,b)return {a[1]+b[1],a[2]+b[2],a[3]+b[3]}end
function M.sub(a,b)return {a[1]-b[1],a[2]-b[2],a[3]-b[3]}end
function M.vec(v)return {v.X,v.Y,v.Z}end
function M.quat(v)return {v.X,v.Y,v.Z,v.W}end
function M.mix(a,b,t)local q={};local dot=0;for i=1,4 do dot=dot+a[i]*b[i]end;local s=dot<0 and -1 or 1;local n=0;for i=1,4 do q[i]=a[i]+(b[i]*s-a[i])*t;n=n+q[i]^2 end;n=math.sqrt(n);if n<1e-8 then return {0,0,0,1}end;for i=1,4 do q[i]=q[i]/n end;return q end
function M.axis(x,y,z,degrees)local a=degrees*math.pi/360;local s=math.sin(a);return {x*s,y*s,z*s,math.cos(a)}end
function M.slerp(a,b,t)
 local dot=0;for i=1,4 do dot=dot+a[i]*b[i]end
 local sign=dot<0 and -1 or 1;dot=math.abs(dot)
 if dot>.9995 then return M.mix(a,b,t)end
 local angle=math.acos(M.clamp(dot,0,1));local den=math.sin(angle)
 local x=math.sin((1-t)*angle)/den;local y=math.sin(t*angle)/den*sign;local q={};local norm=0
 for i=1,4 do q[i]=a[i]*x+b[i]*y;norm=norm+q[i]^2 end
 norm=math.sqrt(norm);for i=1,4 do q[i]=q[i]/norm end;return q
end
function M.pitch(v)return M.clamp((v+180)%360-180,-45,45)end
function M.transform(p,q,s)s=s or {1,1,1};return {Translation={X=p[1],Y=p[2],Z=p[3]},Rotation={X=q[1],Y=q[2],Z=q[3],W=q[4]},Scale3D={X=s[1],Y=s[2],Z=s[3]}}end
function M.blendTransform(a,b,t)
 local v={};for i=1,3 do v[i]=a[i]+(b[i]-a[i])*t end
 local q=M.slerp({a[4],a[5],a[6],a[7]},{b[4],b[5],b[6],b[7]},t);for i=1,4 do v[i+3]=q[i]end
 for i=8,10 do v[i]=a[i]+(b[i]-a[i])*t end;return v
end
return M
