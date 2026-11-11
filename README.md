# Queen’s Domain COOP

**English** | [简体中文](README.zh-CN.md)

Queen’s Domain COOP is a two-player co-op mod. Explore the world, fight enemies, and challenge bosses together with a friend. This preview adds Steam friends rooms alongside the existing LAN mode.

After the one-time installation described in [INSTALL.md](INSTALL.md), launch the game normally through Steam. The Multiplayer menu offers **Host game**, **Join friends**, and **Join LAN game**. Steam mode requires both players to own and run the Steam version, with the in-game overlay enabled.

You can play on the same local network or connect through a virtual LAN tool such as **Radmin VPN**.

## How to Play Co-op

**Hosting adds extra world simulation, enemy AI, and synchronization overhead. We recommend that the player with the more powerful PC hosts the game.**

1. **Install on both PCs.** Use matching game and mod versions. Extract the complete mod package into the game root and run `Install.cmd` once with the game closed. Then launch normally through the original launcher or Steam. Clear any Steam launch options left over from an older mod launcher. See [installation instructions](INSTALL.md) for details.
2. **Host a room.** Choose **Multiplayer → Host game**, keep or change the LAN room port (default `47771`), then choose a save or create a character. The room opens after entering the world. When Steam is available, it also becomes a friends room; no IP address or virtual LAN is needed for Steam joining.
3. **Invite a friend.** Open the pause menu and choose **Invite partner**, directly below Continue, to open Steam’s invitation overlay. Select and invite your teammate yourself.
4. **Join through Steam.** Choose **Multiplayer → Join friends** to open Steam’s friends overlay. Right-click a friend who is hosting and choose **Join Game**, or accept their invitation. Then choose your own character save. Joining from an existing game first asks you to return to the title screen; save before accepting if you want to retain current progress. Steam’s friends overlay is the standard friends list, not a mod-filtered room browser.
5. **Join over LAN instead.** Choose **Multiplayer → Join LAN game** and enter the host’s IPv4 address and port. Different networks can still use **Radmin VPN**. Use the host’s LAN/Radmin IP, not your own address or `127.0.0.1`. See the [Radmin VPN guide](https://www.radmin-vpn.com/help/). Windows Firewall must allow `QDCoopBridge.exe`; LAN uses UDP `47771` by default.

Steam support is a preview pending two-account, cross-network validation. Both transport options still support **one host and one guest in total**. If Steam is unavailable, LAN remains available. Enable Steam Overlay and restart the game if the overlay buttons cannot open it.

Currently supports two players. Version 1.18.0-preview3 shares the original game saves, so keep a save backup before your first co-op session.

## Player appearance

The Multiplayer menu includes five appearances: **Prince, Arjaot, Ippibi, Ujji, and Imba**. Use the arrows above the preview to browse, then press **Select** to equip an appearance. The button disappears for the selected model. Your selection is remembered when you reopen the menu or restart the game. Both the host and the guest can choose an appearance.

The four soldier appearances reuse the existing full-body animation system: Arjaot, Ippibi, and Ujji wear helmet 00; Imba wears the closed helmet 01. This changes the third-person appearance seen by your teammate. First-person arms, character statistics, and collision remain those of the original character.

## Co-op and Progression

The host’s world determines shared progression, including mechanisms, doors, destructible shortcuts, boss phases, and passages unlocked after boss fights. Special boss dialogue and cutscenes are controlled by the host, as is in-game teleportation. Book-placement puzzles are operated by the host, with the results synchronized to the joining player.

The joining player keeps their own character, equipment, experience, and personal pickup progress. Items placed in the original game world can be collected independently by each player. Both players can also receive their own enemy drops according to the original game’s drop rules. After saving, the guest can host with that character using the saved shared world progress and their own safe position.

Version 1.18.0-preview3 uses the original game save directory for both single-player and co-op. A backup is created under `QDCoop-Direct/Backups` at startup; old isolated saves are not imported automatically. When either player saves, both players save the shared world progress and their respective character states.

## Downed Players and Revival

When a player is downed, their teammate can face them and **hold E to revive them**. Revival progress uses the original blue charge bar. Releasing E interrupts the revival.

- If a healing item is selected, a successful revival consumes one item and restores the amount of health provided by that item.
- If no healing item is selected, the teammate is revived with 1 HP without consuming an item.
- If all players are downed, the most recent complete shared save is loaded automatically, restoring the last saved progress.

## Synchronization and Animation

To bring co-op to a game originally designed for single-player, the mod adds synchronization for players, enemies, and world interactions.

**Third-Person Player Animation**

Using the original character, weapon, and animation assets alongside newly created and adapted animations, the mod adds a full set of third-person actions: walking, running, jumping, looking up and down, holding weapons, charging attacks, attacking, and falling into a downed pose. Different weapons retain their distinctive motions, with synchronized ranged projectiles, weapon effects, and sound effects.

Where possible, animations play locally on each machine, with action states and playback progress synchronized between players. This reduces the overhead of continuously transferring skeletal poses. Movement uses interpolation and smoothing to soften the gaps between network updates.

**Enemies and Combat**

Enemy positions, orientations, animations, health, deaths, attack effects, and sounds are synchronized. Enemies can pursue and attack either player. Target selection considers distance, recent damage, and visibility, while limiting frequent target switching when players take turns attacking.

Enemy presentation reuses the original animations and effects wherever possible, playing them continuously on each machine. Combat outcomes are coordinated by the host, with the joining player’s attacks contributing to the same fight.

**How Enemy Aggro Works**

The host evaluates co-op enemy targets using both players’ positions, recent actual damage, and visibility.

- **Distance and damage both matter.** Nearby players attract more attention, while dealing more actual HP damage raises a player’s threat score. Damage is tracked over a rolling window of roughly 3 seconds and gradually loses weight. Rapid throwing-knife hits therefore gain no extra advantage merely from hit count, and an earlier burst of damage does not lock aggro indefinitely.
- **Sight and cover matter.** Players within an approximately 120° forward cone and a clear line of sight receive a visibility bonus. Attacking from behind can also draw attention, but does not give the enemy vision through walls. When a target briefly moves behind cover, the enemy can retain roughly 1 second of memory and pursue the last visible position. Treasure lizards retain their native fleeing behavior instead of following this chase-memory rule.
- **Damage can decisively transfer aggro.** If the other eligible, unobstructed player has dealt at least 1.5 times the current target’s recent effective damage, they take priority. Damage continues to decay over the same 3-second window and targets are reevaluated. Otherwise, score-based switching retains its usual meaningful-advantage and roughly 1.5-second hold rules. A committed attack or recovery may briefly delay a switch, for at most about 1.2 seconds, to avoid abruptly turning mid-swing.
- **Either player can draw aggro.** A host-side proxy represents the joining player for pursuit, hit detection, and damage, allowing enemies to actually attack them. Being downed, dying, or leaving the effective range also influences target selection. Some special attacks with targets dictated by story scripts still follow their original behavior.

**World Interactions**

Alongside synchronizing mechanisms and world states, the mod aims to preserve the animations and sounds of doors, chests, hidden passages, and similar interactions. Synchronization also covers boss phase transitions, death resolution, barrier removal, and restoring player control after cutscenes.

## Changes for Co-op

During multiplayer, the inventory, bestiary, pause menu, weapon evolution, and similar features no longer pause the entire world. Other players and enemies continue to act while these interfaces are open. Some original scripted sequences still temporarily restrict the controls of the player watching them.

To balance smooth gameplay with synchronization, we have moved inter-process communication to shared memory to reduce frequent file reads and writes; optimized object lookups, enemy state collection, effect updates, and animation processing; and moved many remote player actions to local animation playback. By design, local camera control and movement do not wait for network packets. The network primarily carries state, actions, and interaction results.

## Current Status

Currently supports **one host and one joining player**. The mod is still being improved, and complex scenes, special enemies, and network conditions may affect synchronization and performance. Both players should use the same version and back up their co-op saves before updating.

## Development Assistance and Thanks

**GPT ASTRA — AI development assistance.** Guided by the author’s requirements, gameplay decisions, and in-game testing feedback, GPT ASTRA contributed to co-op architecture and synchronization design, code implementation and review, third-person animation adaptation and tooling, performance analysis, troubleshooting, and documentation. The mod also benefits from players who test builds, reproduce issues, and share diagnostic logs.
