# QD COOP M131 原生 DLL

适用于 Queen’s Domain Steam Build 26496428（游戏内版本 1.01.05）。联机双方使用同一版本的模组。

客机进图后会应用房主保存的开门和破坏状态。玩家加入、加入失败或离开时，屏幕上方会显示提示。

## 安装或升级

1. 退出游戏。将压缩包解压到单独的文件夹，保留包内目录结构。
2. 双击 `Install-Native.cmd`。脚本会查找 Steam 库，也可手动指定包含 `QueensDomain.exe` 的目录。现有原生版可以直接升级。
3. 在 Steam 库点击「开始游戏」。如果 Steam 的启动选项里留有旧模组启动器或自定义用户目录，清除后再启动。标题菜单的「多人游戏」位于「开始游戏」下方。

Steam 联机使用原版游戏为当前 Steam 账号管理的存档。若看到空槽，先退出选档并确认 Steam 库启动、当前登录账号以及启动选项。游戏会在获得 Steam 账号后刷新选档。现有存档仍在游戏自己的目录内，原生模组在启动时备份到 `%LOCALAPPDATA%\QueensDomain\Saved\QDCoopNative\Backups`。

游戏语言选择中文时，模组显示中文。选择其他语言时，模组显示英文。好友头像从 Steam 读取。

## 卸载

- `Uninstall.cmd` 删除原生 DLL、QD COOP 资源包以及 UE4SS 版 QD COOP 的已识别脚本和启用条目。UE4SS 本体、其他模组、存档和备份会保留。如果安装包直接解压在游戏根目录，脚本也会清理校验通过的安装包副本。
- `Uninstall-Native.cmd` 只撤销原生 DLL 安装。若安装时停用了已有的 UE4SS 加载器，脚本会将它恢复。
- `Uninstall-UE4SS-QDCoop.cmd` 只移除 UE4SS 版的 QD COOP，可在保留原生 DLL 和它使用的资源包时直接运行。

安装后改动过的已登记 UE4SS 版 QD COOP 脚本会先备份到 `QDCoop-Direct\InstallBackup\removed` 再从运行目录删除。请退出游戏后再执行卸载。
