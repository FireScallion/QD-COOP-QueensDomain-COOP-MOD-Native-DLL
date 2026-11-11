-- Stable wire IDs, observed original materials, offline-bound meshes. No NPC actors.
local root='/Game/QDCoopSkins/'
local knight='/Game/NPCs/Knight/'
local function soldier(id,name,face,helmet)
 return {id=id,name=name,portrait=root..'Portraits/'..name,face=knight..'Knight_head_'..face..'_MI.Knight_head_'..face..'_MI',mesh=root..'SK_Knight'..helmet..'.SK_Knight'..helmet}
end
local M={list={
 {id='prince',name='Prince',nameKey='skin_prince',portrait=root..'Portraits/Prince',mesh='/Game/Player/Assets/FPModel/Player2026/Player_FullChar_2026.Player_FullChar_2026'},
 soldier('arjaot','Arjaot','02','00'),soldier('ippibi','Ippibi','04','00'),soldier('ujji','Ujji','06','00'),soldier('imba','Imba','05','01')
}}
M.byId={};for i,s in ipairs(M.list)do s.index=i;s.portrait=s.portrait..'.'..s.name;M.byId[s.id]=s end
function M.get(id)return M.byId[id]or M.list[1]end
return M
