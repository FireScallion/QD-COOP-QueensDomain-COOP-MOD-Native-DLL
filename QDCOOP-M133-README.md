# Queen’s Domain COOP Native DLL Edition

[简体中文](QDCOOP-M133-README.zh-CN.md)

Queen’s Domain COOP adds co-op to Queen’s Domain. Explore the world, fight enemies, and take on bosses together. **Steam friends and LAN sessions both support 2–4 players, including one host and up to three guests.** The native edition runs inside the game process as a DLL and requires no UE4SS installation.

This guide covers **QD COOP M133** for the **64-bit Windows Steam release of Queen’s Domain 1.01.05 (Build 26496428)**. Everyone in the session should install the same native mod version.

## Install or Upgrade

1. Close the game. Extract the native setup archive to its own folder and keep the directory structure intact.
2. Run `Install-Native.cmd`. The installer looks for your Steam game folder. If prompted, select the folder containing `QueensDomain.exe`. You can upgrade an existing native installation by running the new installer.
3. Launch the game from your Steam Library. If you previously added Steam launch options for an older mod launcher or a custom user directory, clear those options first.
4. On the title screen, open **Multiplayer** directly below **Start Game**.

The installer temporarily disables a recognized UE4SS loader. The mod uses Chinese text when the game is set to Chinese and English for all other game languages.

## Steam Friends

### Host

1. Sign in to Steam, launch the game from your Library, and open **Multiplayer → Host Steam game**.
2. Click **Players** to cycle through 2, 3, and 4. Choose the total number of people who will play, including yourself. Select **2** if you are playing with one friend.
3. Select **Choose save** and load your own save using the original game’s save picker. Your Steam room opens when you enter the world.
4. Press **Esc** and select **Invite players** below the status and inventory entry. Find your teammate in the in-game friends list and select **Invite** on their row. They can also join directly from their own in-game friends list.

### Guest

1. Launch from your Steam Library and open **Multiplayer → Join Steam game**.
2. Find your host and select **Join** on their row. You can also accept a Steam invitation from the host.
3. When connected, choose your own character save using the original game’s save picker. Wait for the session to reach the selected player count and finish synchronizing.

The in-game list shows Steam avatars, names, and online status. Friends playing Queen’s Domain appear first. If you accept an invitation to another room while playing, the game asks whether to return to the title screen. Save first if you want to keep your current progress.

## LAN

Players on the same local network can connect directly. Players on different networks can join a shared virtual LAN using a tool such as **Radmin VPN**.

### Host

1. Open **Multiplayer → Host LAN game**.
2. Confirm the **Room port**. The default is **UDP `47771`**, and you can enter any port from `1024` to `65535`.
3. Set the total number of players, including yourself. Select **Host game**, choose your save, and enter the world.
4. Share **your PC’s IPv4 address and room port**. On a local network, run `ipconfig` on the host PC and look up the IPv4 address of the active Wi-Fi or Ethernet adapter. On a virtual LAN, share the host’s IPv4 address on that network.

### Guest

1. Open **Multiplayer → Join LAN game**.
2. Enter the host’s IPv4 address in **Host IP address** and the matching **Port**. For example, enter `192.168.1.42` and `47771`. Use `127.0.0.1` for testing on the host PC itself.
3. Select **Connect**, then choose your own save. Wait for the session to reach the host’s selected player count and finish synchronizing.

If the connection times out, check that the host has entered the world, the address and port match, and everyone is on the same network. If Windows Firewall blocks the connection, allow the Queen’s Domain game process to receive inbound UDP on the host’s network profile and room port.

## Co-op and Progression

- **Connection notices.** The top center of the screen shows **Joining…**, **Joined**, **Could not join**, or **Left**. Steam notices use the player’s display name when available and their Steam ID otherwise. LAN hosts see guest seat labels. LAN guests see their own connection status and host notices.
- **World progress.** Guests receive the host’s world state when they load in, including opened ordinary doors, broken barricades, and persistent breakables recorded in the save. Story teleports, some boss dialogue, and book-placement puzzles are handled by the host. If a guest’s existing save has already permanently removed an object that still exists for the host, the mod cannot recreate that object when the guest loads in.
- **Characters and rewards.** Everyone keeps their own character, equipment, and personal pickup progress. The host resolves combat, including attacks from guests. Supported enemy drops follow the original drop rules on each player’s game, and received kill XP uses that player’s original XP multiplier.
- **Place items.** Once the room is synchronized, open the details for a stackable consumable in your inventory and select **Place**. Stand where there is clear, level ground ahead so a teammate can pick it up. The action is hidden while the host is alone in the room.
- **Saves.** Single-player and co-op use the original game’s saves. When any player triggers a normal save, the room attempts to create a shared checkpoint. After the save completes, each player’s file contains the shared world progress and their own character state. On startup, the mod backs up the original saves to `%LOCALAPPDATA%\QueensDomain\Saved\QDCoopNative\Backups` by default. Saves from a custom user directory used by an older mod installation are still in that directory.

## Downed Players and Revival

Face a downed teammate and **hold Interact (E by default)** to revive them. The original blue charge bar shows progress. Releasing the button interrupts the revive. With an eligible healing item selected, a successful revive consumes one item and restores the amount of health it provides. Without one, the teammate gets up with 1 HP. If the whole party goes down, the game attempts to load the last complete shared checkpoint.

## Player Appearance

The right side of the **Multiplayer** screen offers **Prince, Arjaot, Ippibi, Ujji, and Imba**. Use the arrows above the preview to browse and click **Select** below the model to confirm. The **Select** button is hidden for the current appearance. The game remembers your choice, and teammates see it on your third-person character.

## Synchronization and Animation

The mod synchronizes third-person player actions, enemy movement and animation, health, deaths, projectiles, effects, and sounds. It plays animations locally where possible and synchronizes their state and progress. Enemies can attack the host or guests. For ordinary enemies, targeting considers distance, recent damage, and visibility. Fleeing star lizards retain their original fear behavior.

Opening the inventory, bestiary, or Esc menu during co-op leaves the world running for teammates. The host also handles shared world logic and enemy AI, so the player with the faster PC is better suited to hosting.

## Troubleshooting and Uninstalling

- **The Steam friends list is unavailable or my Steam saves are missing.** Check that Steam is signed in to the right account, launch the game from your Library, and clear old launch options. Exit and reopen the save picker to give the game time to refresh the account’s saves.
- **The room is open, but co-op features are still unavailable.** Check the selected total player count. A four-player room waits for the host and three guests. Select two players for a session with one friend.
- **Remove the native edition.** Close the game and run `Uninstall-Native.cmd`. If the installer suspended a recognized UE4SS loader, this restores it.
- **Remove only the older UE4SS edition of QD COOP.** Run `Uninstall-UE4SS-QDCoop.cmd` to remove the older scripts and mod enablement entries while keeping the native edition installed.
- **Remove both QD COOP editions.** Run `Uninstall.cmd`. The uninstallers keep your game saves, save backups, UE4SS itself, and other mods.

## Development Credits

AI development assistance by **GPT6 ASTRA** and **GPT-6.1 sol**. Thanks to the players who test builds, reproduce issues, and share logs.
