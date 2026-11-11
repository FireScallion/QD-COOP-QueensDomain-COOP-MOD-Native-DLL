# Native build dependencies

The prototype is built with MinGW-w64 GCC 16.2.0 (MSYS2 Rev5). The compiler is
not distributed. Static runtime components may be linked into the developer
DLL and test executables:

- GCC runtime (`libgcc`): `licenses/libgcc/`.
- GNU C++ runtime (`libstdc++`): `licenses/libstdc++/`.
- MinGW-w64 winpthreads: `licenses/winpthreads/`.
- MinGW-w64 CRT support: `licenses/crt/`.

The notices are copied unmodified from the toolchain used for this build.
The repository's root LICENSE covers QD COOP's own source. This developer
package contains no game-derived assets, game executable, Steam SDK library,
UE4SS distribution, Lua runtime, .NET runtime, or third-party mod binary.

The M1 tick bridge is additionally compiled with the locally installed licensed
Microsoft Visual C++ 2019 toolset (14.29.30133), using the static C/C++ runtime.
The compiler, Windows SDK, UE4SS runtime and game files are not redistributed.
The bridge calls UE4SS's exported registration API; UE4SS is a separate runtime
dependency governed by its own distribution license. The implementation source
here is QD COOP code, not a copy of UE4SS implementation code.
