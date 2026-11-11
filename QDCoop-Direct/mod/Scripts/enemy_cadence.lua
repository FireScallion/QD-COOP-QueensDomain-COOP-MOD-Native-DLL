-- Preserve roster and native animation playback at every distance. Only the
-- observation of idle, stationary enemies is less frequent beyond 25 metres.
local M={}
function M.idle(squaredDistance)
 local d=squaredDistance or 0
 return d<2500^2 and .1 or d<3500^2 and .2 or .5
end
function M.distance(f,localPosition,peerPosition)
 local function d(p)
  return p and ((f[10]-p.X)^2+(f[11]-p.Y)^2+(f[12]-p.Z)^2)or 1e20
 end
 return math.min(d(localPosition),d(peerPosition))
end
return M
