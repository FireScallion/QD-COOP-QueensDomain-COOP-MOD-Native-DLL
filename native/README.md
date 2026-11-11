# QD COOP native foundation (M0)

The M0 foundation described below remains standalone. The newer **M1 enemy
scalar adapter** has a separate build and scope: see [ENEMY-M1.md](ENEMY-M1.md).
M1 includes real engine/thread/lifetime/loopback integration and still depends
on the pinned UE4SS runtime. Do not confuse the standalone M0 ABI test with a
UE4SS-free playable mod.

This is a developer prototype, **not an installable replacement mod**. It does
not inject into the game, register engine callbacks, authenticate a Steam/LAN
peer, exchange real network packets, or manipulate game saves/inventory. The
legacy preview2 mod remains unchanged. Do not place this DLL in the game folder
expecting multiplayer to work.

The C++20 library implements a four-member host model, connection/world lifetime
checks, bounded per-peer state/event channels, and checkpoint/rescue arbitration.
The DLL currently exports the room lifecycle C ABI in `include/qdcoop/api.h`.
Transaction and queue models are exercised directly by the C++ harness; their
game/transport/persistence adapters are not implemented yet.

Build on Windows x64 from the repository root:

```powershell
.\Build-Native.ps1 -Compiler 'C:\path\to\mingw64\bin\g++.exe'
```

The tested compiler is MSYS2 MinGW-w64 GCC 16.2.0 (Rev5). `objdump.exe` must be
beside the compiler. No SDK downloads are performed. The script builds the DLL
and two console test harnesses, verifies runtime dependencies, then tests with
compiler directories removed from PATH. Results are under `dist/native/`.

Run the packaged tests from a terminal:

```powershell
.\QDNativeCoreTests.exe
.\QDNativeDllTests.exe "$PWD\QDCoopNative.dll"
```

All model operations have one owner thread. Never call them concurrently. IO
threads must communicate through owned command buffers (adapter still pending).
No UObject pointer, file IO, polling thread or network wait occurs in these model
operations. Allocating/destroying a DLL session is a low-frequency operation.

`admit_authenticated` is a trust boundary: an ID is not authentication. The future
Steam adapter must bind Steam identity to its authenticated connection; LAN must
validate a room join credential and a reconnect credential. The current C ABI is
for trusted local callers and is not a wire protocol. Native protocol version
`0x4e340001` explicitly differs from preview2.

This work derives from QD COOP's own code and rules. No Knights' Domain binary,
implementation, algorithm or private protocol is included.
