# 实现 / Implementation

## 动作和全身表现 / Character animation

**采集。** [body_source.lua](../QDCoop-Direct/mod/Scripts/body_source.lua) 跟踪本地第一人称动画实例的左右手根动作和 Montage。普通 Montage 播放入口和蓝图代理入口都参与识别。动作记录含动画 ID、播放代次、段落、时间、速率及播放状态。多个动画槽分组传输，蓄力或副手动作可以与其他动作同时存在。

**时间轴。** [smoothing.lua](../QDCoop-Direct/mod/Scripts/smoothing.lua) 在接收端建立短缓冲，以受限 Hermite 插值衔接位置，朝向沿短角度插值。缺包时只允许有界外推。播放时钟通过小幅调速收敛，将身体、头部和武器对齐到同一时间线。

**本地播放。** [body_native.lua](../QDCoop-Direct/mod/Scripts/body_native.lua) 驱动远端全身 AnimBlueprint。移动状态控制行走、奔跑、跳跃等姿态，动作槽播放重定向后的动画，并用收到的段落和时间校正进度。独立武器组件负责武器模型及材质，特效可以借用已有组件。双方确认相同的动画目录且动作稳定后，[body_transport.lua](../QDCoop-Direct/mod/Scripts/body_transport.lua) 才省略冗余的全骨骼帧。资源缺失或原生播放不适用时，[body_router.lua](../QDCoop-Direct/mod/Scripts/body_router.lua) 退回姿态驱动。

**资源制作。** [烘焙脚本](../tools/animation/bake.cjs) 将本机游戏采集的第一人称关节旋转按绑定姿势重定向至全身骨架，并转回局部骨骼变换。它记录 Montage 段落、槽位、混合参数和 additive 信息。 [QDAuthor](../tools/animation/QDAuthor/) 在 UE4.27 编辑器中导入采样、生成动画与 Montage，并组装全身动画图。发行时由游戏加载已烘焙资源，玩家端不需要编辑器。输入资产与构建边界见 [构建文档](BUILD.md)。

**Capture and timing.** The sender tracks first-person root poses and active montages, including playback generation, section, position, rate, and slot group. A buffered display clock drives bounded interpolation and extrapolation for the remote character.

**Playback and authoring.** The receiver drives a full-body AnimBlueprint, blends locomotion and separate montage lanes, and renders equipment through dedicated components. Full bone frames can be omitted after capability and catalog agreement. A pose-based renderer handles unsupported states. The authoring tools retarget locally captured arm rotations into full-body local transforms, then assemble animation sequences, montages, and the UE4.27 animation graph.

## 联机传输 / Transport

[Bridge](../QDCoop-Direct/source/Bridge/) 在独立进程中承接 UDP，会话按对端身份与端点校验。高频状态用序号筛掉旧帧。可靠事件与世界快照有各自的确认、重传或分块逻辑。Lua 与桥接进程通常通过 [共享内存邮箱](../QDCoop-Direct/source/AsyncIPC/) 交换数据，并保留文件通道作为回退。UE 对象的采集、应用和动画播放在游戏线程执行。

A separate UDP bridge validates peer identity and session. Sequenced snapshots, acknowledged events, and chunked world updates use different delivery paths. Shared memory carries local IPC, with a file fallback. Game-thread code owns Unreal object access.

## 敌人、互动和进度 / Enemies, interactions, and progression

房主管理敌人的 AI、伤害和 Boss 结算。客机发出本地命中事件，房主按会话、来源与场景等条件应用。客机依据敌人状态和时间轴更新模型、动画与血条。奖励事件将人物经验交给原版 XP 入口，掉落在客机通过原版逻辑独立抽取。 [敌人同步](../QDCoop-Direct/mod/Scripts/enemies.lua) · [奖励](../QDCoop-Direct/mod/Scripts/enemy_rewards.lua)

世界快照携带机关、门、区域和关键进度。客户端恢复世界状态后，在适用的门、宝箱、隐藏墙上调用原版表现。关键过场的进度回调由房主结算。客机的个人物品、拾取、角色和个人存档位置独立保存。 [世界同步](../QDCoop-Direct/mod/Scripts/world.lua) · [原版交互表现](../QDCoop-Direct/mod/Scripts/world_interaction_presentation.lua) · [过场](../QDCoop-Direct/mod/Scripts/cutscene_sync.lua)

房主负责敌人权威，双方各自保有个人存档。共同检查点将两边的提交关联起来。救援将施救进度、扣药和复活拆成可追踪事务，处理重复消息和中途中断。 [共同检查点](../QDCoop-Direct/mod/Scripts/checkpoint.lua) · [救援事务](../QDCoop-Direct/mod/Scripts/rescue_transactions.lua)

The host owns enemy AI, damage, encounter outcomes, and shared world progression. Clients present remote state and preserve their own character progression and pickups. World updates reconcile interactive actors and replay their native presentation where supported. Common checkpoints link the players' save commits, and rescue transactions track healing item consumption and revival across retries.

图书馆书槽及解谜门只接受房主进度。客机将书槽设为不可互动并恢复房主的书本、模型和门状态。房主同时拒绝客机对此类对象的通用世界状态写入。 [书架权威策略](../QDCoop-Direct/mod/Scripts/world_library_books.lua)

The library puzzle is host-authoritative. The guest sees the host's book slots and gate state, and the host rejects generic guest world-state writes to those actors.

## 1.17.2 original launcher integration / 原版启动集成

`main.lua` waits for the native object hash index, obtains the actual UE Saved directory, then calls `qdboot.dll`. This helper uses CreateProcessW with CREATE_NO_WINDOW to start Native-Session.ps1. No shell command is constructed from network input; no waiting for a process happens on the game thread. A process creation timestamp guards against reused PIDs. The supervisor owns an exclusive install-local lock and a fresh 32-hex shared-memory channel, backs up the actual SaveGames directory, and publishes configuration before runtime_main.lua starts. Failed setup leaves the base game running.

The supervisor starts the existing bridge only on a validated session.request. Bridge ownership is tied to the game process and creation timestamp rather than a visible launcher. Leaving a room stops the bridge; game exit stops both supervisor and bridge. A forced supervisor exit cannot keep a bridge alive after the game dies. An installation supports one co-op game process at a time. Save persistence uses the actual original directory, retaining personal guest state and host authority; old isolated saves are not imported.

1.17.2 不再使用隔离运行目录。安装时合并 Mod 启用项和必要 UE4SS 设置，备份覆盖文件；卸载只恢复或移除仍与安装结果相符的文件。其他 Mod、存档、旧版隔离目录均保留。Steam 的覆盖层、云存档和 DRM 仍需正版实测。

## 1.17.3 XP ownership and native travel / 经验归属与原版传送

EnemyBase grants kill experience to its current TargetPlayer. A host-side guest target deliberately overrides AddXP with an inert implementation, so switching aggro to the guest previously swallowed the host's native kill award. The host now forwards non-steal AddXP calls received by that exact proxy to its real local player. The guest's existing deduplicated ER award remains unchanged; each side still applies its own native character modifiers. XP-steal is not converted into another kill reward. Diagnostic counters distinguish guest XP awards from recovered host rewards. This does not rebalance historical saves.

原版 EnemyBase 将击杀经验发给当前 TargetPlayer。怪物攻击客机时，该目标是房主侧的客机代理，其空 AddXP 实现曾吞掉房主经验。现在只将该代理收到的非吸取经验交还房主本地角色；客机原有的去重奖励保持不变，各自仍使用原版角色经验加成。不追补历史存档。

World synchronization defers snapshot consumption, streaming replacement and arrival placement while the guest's native teleport flags are active. A late first-shop callback preserves an already queued host arrival instead of clearing it. Disconnect handling stays active during the wait. The original first-shop flow and local movement/interaction remain native. Arrival logs include the authority travel revision and destination, to distinguish completed world-sync placement from a later native move.

客机原版传送进行中，暂缓世界快照消费、区域加载替换与位置校正，结束后使用房主最新快照。首次商店回调不会清掉已排队的房主传送；等待期间断线处理仍正常执行。原版商店演出和本地操作逻辑保留。
