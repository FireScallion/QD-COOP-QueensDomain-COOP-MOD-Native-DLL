-- Mod-owned UI only. Native widgets retain the game's own translations.
return function()
 local M={language='en',revision=0};local nextRead=0
 local strings={
  skin_prince={'王子','Prince'},skin_select={'选择','Select'},skin_previous={'‹','‹'},skin_next={'›','›'},skin_save_error={'保存失败，请重试','Could not save; try again'},
  multiplayer={'多人游戏','Multiplayer'},host={'主持游戏','Host game'},join={'加入游戏','Join game'},back={'返回','Back'},
  join_friends={'加入好友','Join friends'},join_lan={'加入局域网游戏','Join LAN game'},invite_partner={'邀请伙伴','Invite partner'},
  steam_friends_hint={'已打开 Steam 好友列表。右键已开房的好友，选择加入游戏。','In Steam friends, right-click a friend hosting a room and choose Join game.'},
  steam_unavailable={'Steam 不可用。请从 Steam 启动正版游戏；局域网仍可使用。','Steam unavailable. Launch your Steam copy through Steam; LAN is still available.'},
  steam_overlay_disabled={'请在 Steam 中启用游戏内覆盖界面，再重新启动游戏。','Enable the Steam in-game overlay, then restart the game.'},
  steam_room_error={'无法加入 Steam 房间。请确认房间未满、房主仍在游戏中，双方使用相同版本。','Could not join the Steam room. Check that it is open, has space, and both players use the same version.'},
  steam_leave_confirm={'返回主菜单并加入好友？未保存的进度可能丢失。','Return to the title screen and join your friend? Unsaved progress may be lost.'},
  host_port={'房间端口','Room port'},host_address={'房主 IP 地址','Host IP address'},port={'端口','Port'},
  choose_save={'选择存档','Choose save'},connect={'连接','Connect'},
  host_save={'主持游戏 — 选择存档','Host game — Choose save'},guest_save={'加入游戏 — 选择角色存档','Join game — Choose character save'},
  joining={'正在连接……','Connecting…'},loading={'正在加载……','Loading…'},receiving_world={'正在接收房主世界……','Receiving host world…'},
  connected={'已连接，请选择自己的角色存档。','Connected. Choose your character save.'},
  invalid_ip={'请输入有效的 IPv4 地址。','Enter a valid IPv4 address.'},invalid_port={'端口须为 1024 至 65535 的整数。','Port must be an integer from 1024 to 65535.'},
  timeout={'连接超时。请确认房主已经开房、IP 和端口正确。','Connection timed out. Check that the host has opened a room and the IP and port are correct.'},
  world_timeout={'接收世界数据超时，请重试。','World transfer timed out. Please try again.'},disconnected={'连接已断开，请重新连接。','Disconnected. Please reconnect.'},
  bridge_error={'联机服务未能建立连接。请检查端口是否被占用，详细信息已写入日志。','The connection service could not start. Check whether the port is in use; details are in the log.'},
  opening={'正在开启房间，端口 %d。','Opening room on port %d.'},
  address_hint={'同机测试用 127.0.0.1；其他电脑填写房主的局域网或 Radmin IP。','Use 127.0.0.1 on the same PC, or the host’s LAN or Radmin IP on another PC.'},
  host_hint={'选择角色存档后进入游戏并开启房间。','Choose a character save to enter the game and open a room.'},place={'放置','Place'}
 }
 function M.refresh()
  local now=os.clock();if now<nextRead then return false end;nextRead=now+.25
  local ok,value=pcall(function()return StaticFindObject('/Script/Engine.Default__KismetInternationalizationLibrary'):GetCurrentLanguage():ToString()end)
  local lang=ok and type(value)=='string'and value:lower():sub(1,2)=='zh'and'zh'or'en'
  if M.language~=lang then M.language=lang;M.revision=M.revision+1;return true end;return false
 end
 function M.get(key,...)
  local pair=assert(strings[key],'Unknown UI text: '..tostring(key));local s=pair[M.language=='zh'and 1 or 2]
  if select('#',...)>0 then return string.format(s,...)end;return s
 end
 M.refresh();return M
end
