-- Component space of the existing player proxy rig. It faces +Y before the
-- proxy's -90 degree yaw correction. The pose rotates that forward axis down.
return function(Q, rig, reference, names)
 local function align(from, to)
  local function unit(v)
   local len = math.sqrt(v[1]^2 + v[2]^2 + v[3]^2)
   return {v[1]/len, v[2]/len, v[3]/len}
  end
  local a, b = unit(from), unit(to)
  local q = {
   a[2]*b[3] - a[3]*b[2], a[3]*b[1] - a[1]*b[3], a[1]*b[2] - a[2]*b[1],
   1 + a[1]*b[1] + a[2]*b[2] + a[3]*b[3],
  }
  local length = math.sqrt(q[1]^2 + q[2]^2 + q[3]^2 + q[4]^2)
  for i = 1, 4 do q[i] = q[i]/length end
  return q
 end
 local directions = {
  arm_stretch_r = {'forearm_stretch_r', {-7, 1, 25}},
  forearm_stretch_r = {'hand_r', {3, 3, 21}},
  arm_stretch_l = {'forearm_stretch_l', {14, -1, -16}},
  forearm_stretch_l = {'hand_l', {-13, 3, 14}},
 }
 local override = {}
 for name, pair in pairs(directions) do
  local bone, child = names[name], names[pair[1]]
  override[bone] = Q.mul(align(Q.sub(reference[child], reference[bone]), pair[2]), rig.target[bone].cq)
 end
 local pelvis = names.root_x
 local restPositions, restRotations = {}, {}
 for i, bone in ipairs(rig.target) do
  local parent = bone.parent
  restPositions[i] = parent > 0 and Q.add(restPositions[parent], Q.rot(restRotations[parent], bone.p)) or bone.p
  restRotations[i] = override[i] or (parent > 0 and Q.mul(restRotations[parent], bone.q) or bone.q)
 end
 local function smooth(x)
  x = Q.clamp(x, 0, 1)
  return x*x*(3 - 2*x)
 end
 return function(positions, rotations, elapsed)
  local fall = smooth(elapsed/.72)
  local brace = smooth((elapsed - .08)/.45)
  local pitch = Q.axis(1, 0, 0, -90*fall)
  local standing = positions[pelvis]
  local height = 14 + (standing[3] - 14)*(1 - smooth(elapsed/.61))
  local center = {standing[1]*(1-fall), 22*fall, height}
  for i = pelvis, #rig.target do
   local pos = Q.add(center, Q.rot(pitch, Q.sub(restPositions[i], restPositions[pelvis])))
   local alpha = i == pelvis and 1 or brace
   local base = Q.add(center, Q.rot(pitch, Q.sub(positions[i], standing)))
   for axis = 1, 3 do base[axis] = base[axis] + (pos[axis] - base[axis])*alpha end
   positions[i] = base
   rotations[i] = Q.mul(pitch, Q.mix(rotations[i], restRotations[i], brace))
  end
 end
end
