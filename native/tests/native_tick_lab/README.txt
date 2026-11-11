Build the core with repo/Build-EnemyM1.ps1 and use its output as build/.
Copy this lab folder to workspace/work/m14 beside the existing work/m2/lab.
The isolated client must be stopped. Keep its process.json and Station.cs.

Use GCC with -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -static.
Build native_launch.cpp with -municode into build/QDNativeLaunch.exe.
Build native_tick.cpp together with these two repository sources

    native/adapters/windows/native_tick_bridge.cpp
    native/src/enemy_stream.cpp

Use -I repo/native/include -shared -lbcrypt -luser32 -lws2_32
and -Wl,--exclude-all-symbols to produce build/QDNativeTick.dll.

Run work/m2/lab/station-run.ps1 with -Script workspace/work/m14/run-native.ps1.
The private-station runner holds its own game process. It temporarily
renames the isolated game proxy and restores it in finally. Its separate
profile uses a copy of the working isolated NVIDIA DXCache. Prepare the
profile Config from a working isolated render run if needed. Archive
existing report files before rerunning.

The diagnostic uses loopback ports 49761 and 49762. It verifies 42 metadata
inputs, game-thread callbacks, no UE4SS module, two received datagrams,
one foreign source, one unbound snapshot, and one malformed packet.
native-tick.json records the bootstrap. native-core.json records core
counters before the worker is stopped. launch.txt and exit.json record
normal game and launcher exit. A 120s deadline records forced termination
as failure. The gameplay scene for this test is the title screen.

M16 isolated world test

Use the same build/ output and compiler as above. Build native_world.cpp
together with native/adapters/windows/native_tick_bridge.cpp with
-I repo/native/include -shared -static -lbcrypt -Wl,--exclude-all-symbols
into build/QDNativeWorld.dll. Rebuild QDNativeLaunch.exe from the current
native_launch.cpp. Place run-world.ps1 in workspace/work/m16 and copy
the existing isolated Config and SaveGames into profile/Saved there.

Invoke station-run.ps1 with -Script workspace/work/m16/run-world.ps1.
The runner creates a fresh runs/<id> directory and separate role profiles.
By default it launches the host and three clients with distinct loopback
ports 49861 through 49864. QD_WORLD_CLIENTS selects a smaller peer count
for troubleshooting. The client on 49862 renders offscreen with the
prepared isolated NVIDIA cache. The other roles use NullRHI.

The runner owns both isolated proxy renames and waits for all launcher
handles before restoring them. Only the two existing isolated game copies
are used. Stop other tests that are using these copies before running.

Each role writes native-world.json, native-core.json, launch.txt and
exit.json. The runner checks all process exits and compares each client's
two HP markers and final position with the host. summary.json records the
successful group run. runner-error.txt captures validation failures.
latest-run.txt gives the run directory.

For M17, stage the lab in workspace/work/m17 and rebuild the core from this revision.
Use the same runner and build commands. The lab attaches the
separate client display, verifies the live scalar updates and publishes
a terminal event. For three clients, client3 delays its core consumption
by 650 ms while its network worker receives retries. The aggregate gate
requires one application on every client, a hidden display, zero HP or
an expired gameplay object, an empty host pending queue and observed
duplicates on client3. The harness invokes the original Kill on the
host fixture and queues its event through the native API.

For M18, stage the lab in workspace/work/m18 and rebuild the core from this revision.
The host uses QDEnemyObserveNativeKill. The lab calls the original Kill
on an unregistered fixture and then a registered fixture. The host gate
checks two original call-throughs, one new death reservation and slot
restoration. The clients retain the M17 duplicate and one-application checks.

For M19, stage the lab in workspace/work/m19 and rebuild the core.
The runner checks live enemy ownership on each client after transport
binds. The fixture verifies repeated take, stale generation rejection,
full restoration on release, re-registration and takeover before death.
See native/NATIVE-OWNERSHIP-M19.txt for the tested behavior and limits.

For M20, stage the lab in workspace/work/m20 and rebuild the core.
The same four-process runner now requires live AnimBlueprint markers
and montage progress and stop on every client before the Kill event.
See native/NATIVE-ANIMATION-M20.txt for the profile and asset details.

For M21, stage the lab in workspace/work/m21 and rebuild the core.
From the source root, use node tools/animation/generate-native-catalog.cjs
with --check to verify the generated headers. The lab calls the native
catalog scanner in slices and checks the live Lizer profile chosen by
the core. See native/NATIVE-ANIMATION-CATALOG-M21.txt.

For M22, stage the lab in workspace/work/m22 and rebuild the core.
The host uses the engine enemy enumeration and enrolls a real Lizer
from the loaded world. Clients start listening before spawning and
resolve the offered class and mesh, bind display and take ownership,
then confirm. The third client delays confirmation to exercise retry.
See native/NATIVE-ENROLLMENT-M22.txt for generator and ABI details.

For M23, stage the lab in workspace/work/m23 and rebuild the core.
The host retires generation 1 after its original Kill and reuses
the same slot for a second live world enemy. The lab verifies both
client cleanup acknowledgments and that the live original host
actor survives retirement. See native/NATIVE-RETIREMENT-M23.txt.

For M24, stage the lab in workspace/work/m24 and rebuild the core.
Set QD_WORLD_AUTOMATIC to 1 in the runner environment to verify four
simultaneous world enemies, automatic expiration and same-slot refill.
Unset it to run the M23 explicit retirement regression.
See native/NATIVE-WORLD-RUNTIME-M24.txt for runtime configuration.

For M25, stage the lab in workspace/work/m25 and rebuild the core.
QD_WORLD_STEAM_SMOKE=1 runs one title-screen game under the same
Windows user as Steam. It creates two private rooms, leaves both and
checks the first room's rejected re-entry. QD_WORLD_AUTOMATIC=1 with
the Steam smoke flag unset runs the four-process loopback regression.
See native/NATIVE-STEAM-M25.txt.

For M31, stage the lab in workspace/work/m31 and rebuild the release
core and QDNativeWorld.dll. QD_WORLD_PLAYERS=1 uses four isolated game
processes and the stock pak. It checks twelve actual avatars, both
transforms for each pair of peers, stale-avatar hide and retirement,
room cleanup, and the SHA-256 of each private test save. See
native/NATIVE-PLAYER-POSE-M31.txt.

For M32, stage the lab in workspace/work/m32 with the two preview3
body animation PAKs under assets. Keep QD_WORLD_PLAYERS=1 and set
QD_WORLD_PLAYER_BODY=1 to verify the animation graph, native locomotion,
and concurrent source montage play and stop. For the dependency
fallback case use QD_WORLD_PLAYER_BODY_FALLBACK=1 with the body flag
off. See native/NATIVE-PLAYER-BODY-M32.txt.

For M33, stage the lab in workspace/work/m33 with the preview3
NativeBody, NativeSpear and Skins PAKs under assets. Use
QD_WORLD_PLAYERS=1 and QD_WORLD_PLAYER_GEAR=1 for the real equip,
swap, skin and component cleanup checks. See
native/NATIVE-PLAYER-EQUIPMENT-M33.txt.

For M34, stage the same asset packs and private profiles in
workspace/work/m34. Add QD_WORLD_PLAYER_BONES=1 to the M33 gear
environment for the BirdFlail and independent left-hand display
test. See native/NATIVE-WEAPON-BONES-M34.txt.
