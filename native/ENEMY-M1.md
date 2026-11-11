# Native enemy scalar path, M1

This is a developer vertical slice, not a playable replacement for preview3.
M0's standalone room ABI is unchanged. M1 adds an actual two-process engine
path, but still dynamically depends on UE4SS 3.0.1 Beta e84db30c for cold
metadata discovery, Lua control and producer hooks. Native engine access and
the M12 tick bridge no longer use UE4SS after activation. There
is no claim that UE4SS has been removed or that all enemy gameplay is ported.

Only the current official Steam build tested on 2026-11-09 is accepted:
app 1799840, installed build 26496428, shipping executable SHA256
`79ccdc6cf9e1e85512d52d2cb1c3ffa963d400be1555051a3523c982d181d1e6`.
The IO worker verifies the actual running executable before enabling the path.
An update requires revalidation, not weakening the build gate or trying old
offsets. The authoritative installation is `D:\steam\steamapps\common\QueensDomain`.

## Implemented path

1. A cold Lua bootstrap resolves reflected types, properties and functions.
   Native code validates property sizes/offsets and function parameter layouts.
2. The native Win64 bridge installs a checked GameEngine Tick vtable callback.
   It keeps the original virtual target and a C function boundary. Both DLLs are
   pinned for the process lifetime; unloading an installed callback is unsupported.
3. Actor and mesh registrations become weak references. Only the recorded game
   thread may resolve them or call engine functions. Destruction and a changed
   actor-to-mesh association deactivate the binding. Metadata weak references are
   checked before every active callback.
4. Native capture collects HP, attack/recovery/chase flags, hidden/visible state,
   actor position/rotation/velocity and component transform. Target interval is
   50 ms, quantized to engine frames. No Lua call is needed by this path after
   registration; test motion stimulus and the inspection probe still use Lua.
5. A bounded single-producer/single-consumer queue transfers owned scalar packet
   bytes to a nonblocking UDP worker. There are no UObject pointers, engine calls,
   file reads or socket waits on the game thread's sync path. Queue overload drops
   state updates and increments counters; it does not overwrite unread entries.
6. The receiver validates wire version, room nonce, world, entity generation,
   sequence, lengths and finite scalar values. The game thread interpolates bounded
   history using the preview2 timeline rules (Hermite, shortest angle, quaternion
   interpolation, bounded extrapolation), then applies actual engine transforms
   and state. State loss beyond 500 ms hides the display entity once.
7. `LoadMapPre` advances the world epoch and disables/clears all bindings before
   actor destruction. After the map is ready, refresh metadata, register the new
   actors and explicitly resume. Equal monotonic world IDs must be coordinated by
   the future room/world adapter; this lab does not implement that coordinator.

Actor velocity is used by the timeline, not written into client movement AI.
Registration does not revoke local AI/movement authority. Client display actors
must already be owned by the replication adapter, with competing AI, movement,
collision and transform writers disabled. Cached placement assumes this contract.
A diagnostic rebind to an existing stock enemy recovered HP but failed the fixed
pose check; repeating travel with newly created, fully controlled display enemies
passed pose and HP recovery. Automatic remote-actor ownership is not implemented.
HP zero and hidden state do not invoke the game's kill/reward path. Animation
events, AI, hit validation, death choreography, drops, rewards, checkpoints,
rescue, saved inventory, authenticated Steam transport and a four-process session
are still outside this M1 adapter. The four-member model tests are not evidence
of four-player in-game operation.

## Build and test

```powershell
.\Build-EnemyM1.ps1 -Compiler 'C:\toolchain\mingw64\bin\g++.exe'
```

Tested: GCC 16.2.0 / MSYS2 Rev5, Windows x64. M12 no longer requires MSVC.
No dependency downloads. The script builds both DLLs and runs 24,283 M0 model
checks, 1,037 enemy codec/timeline/queue checks, 100,000 ordered two-thread queue
transfers and 40 standalone DLL ABI checks. Static imports are Windows system
DLLs; this does not imply that the live adapter runs without UE4SS.

The adapter's `qdenemy` entry is a trusted local LuaMadeSimple control interface,
not a stable public C ABI. `I/H/O` configure metadata, hook and transport; `R`
registers an entity; `M` selects native/legacy/off; `Z` advances the world; `S`
returns counters; `B/E` delimit metrics; `X` requests worker shutdown; `V` exercises
the wrong-thread guard. The worker is single-start for a process. These controls
are for the isolated harness and are not exposed to network peers.

## Reproducing the isolated lab

`tests/lab/` contains bootstrap, deterministic scene, comparator and probe/measure
scripts, a private-station launcher and `prepare.ps1`. Copy this directory to a
fresh workspace before preparing it. `prepare.ps1` takes the official game,
pinned runtime directory, disposable test save, native build output, delivered
preview2 Scripts directory (including built helper DLLs), and optional test
graphics settings. It never installs into the official game. The launcher refuses
the interactive station and has a 45-minute owned-process deadline. Archive run
metadata/heartbeats before a restart. The scripts require a private Windows
window station, isolated UserDir and a game-thread file probe. Do not run the
scene setup in the user's actual game or save. It freezes stock AI and creates
24 real Cocker actors purely for measurement. It is not gameplay installation.

Use the official game's fresh copies as `host/game` and `client/game`, with
independent `profile/Saved`. Copy only a disposable test World1 into each
profile's local `SaveGames/World1.sav`. A private station has no authenticated
Steam account: leaving the save only in a Steam ID subdirectory produced an
empty slot and loaded the intro during an excluded setup attempt. Verify
`DoesSaveGameExist('World1',0)` and the expected enemy population before testing.
Do not use the live save or rely on the title screen's default selection.

Place the two native DLLs in `bin/`, and the tick bridge beside each copied game
executable. Install the pinned UE4SS runtime plus a local file probe in the copies
only; disable all other mods. Set UE4SS `UseCache = 0` and boot copies sequentially
until their probe heartbeats arrive. Host: NullRHI; client: D3D12 RenderOffscreen;
no mouse capture, sound or visible desktop window. Keep exact PID/start/executable
ownership and stop only processes created for this test.

Load the existing slot using separate language/menu/slot actions. On the game
thread set `_G.M1Role` to `host` or `client`, optionally `_G.M1Root` to the lab
directory with a trailing slash, then execute `init.lua`. Wait for `build:1`, run
`setup.lua` once, then `legacy.lua` once. After travel, `M1.refreshMetadata()`
re-resolves metadata; reset scene bindings before resuming. Calling `init.lua`
again on an existing context only refreshes metadata, not the socket or hook.

For the comparator, copy the repository's preview2 enemy Lua modules and its
own capture/placement helper DLLs into `legacy/`. The comparator uses the actual
capture, protocol, timeline and placement modules with cached actor/mesh handles;
animation metadata is held constant. It deliberately shares the new UDP worker
with native mode. This isolates this scalar path, and is NOT a comparison against
the full preview2 mod, its .NET bridge, all animations or real Steam networking.

`measure.ps1` switches native/legacy/legacy/native/native/legacy, warms each arm,
then records approximately 13 seconds per trial. EngineTick interval quantiles
are frame-cadence measurements, not GPU duration or display-present timings.
Game/render/RHI/network thread CPU are OS scheduled CPU time. Native-work timing
covers the callback only; legacy mode's near-zero native-work value does not
measure Lua work. Compare total game-thread CPU per measured frame instead.
GPU utilization is whole-machine `nvidia-smi` telemetry; GPU milliseconds were
not obtained from this shipping build. Record concurrent workloads and avoid
inferring GPU improvement from CPU savings.

The transport is intentionally loopback-only with configured source ports and a
test nonce. This is not peer authentication or a safe LAN deployment protocol.
The next stage must replace the temporary UE4SS boundary, integrate real room /
world / entity authority, and migrate enemy animation and gameplay events before
testing full feature parity or shipping a playable build.
