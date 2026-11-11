#include "qdcoop/api.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
static unsigned checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "FAIL DLL line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
template<class T> static T bind(HMODULE dll, const char* name) {
    FARPROC raw = GetProcAddress(dll, name); CHECK(raw != nullptr);
    T fn{}; static_assert(sizeof(fn) == sizeof(raw)); std::memcpy(&fn, &raw, sizeof(fn)); return fn;
}
int main(int argc, char** argv) {
    CHECK(argc == 2); CHECK(GetModuleHandleW(L"UE4SS.dll") == nullptr);
    HMODULE dll = LoadLibraryA(argv[1]); CHECK(dll != nullptr);
    const auto abi = bind<decltype(&QDNativeAbi)>(dll, "QDNativeAbi");
    const auto create = bind<decltype(&QDNativeCreateHost)>(dll, "QDNativeCreateHost");
    const auto destroy = bind<decltype(&QDNativeDestroy)>(dll, "QDNativeDestroy");
    const auto admit = bind<decltype(&QDNativeAdmit)>(dll, "QDNativeAdmit");
    const auto ready = bind<decltype(&QDNativeReady)>(dll, "QDNativeReady");
    const auto leave = bind<decltype(&QDNativeDisconnect)>(dll, "QDNativeDisconnect");
    const auto travel = bind<decltype(&QDNativeTravel)>(dll, "QDNativeTravel");
    const auto down = bind<decltype(&QDNativeDown)>(dll, "QDNativeDown");
    const auto view = bind<decltype(&QDNativeRoom)>(dll, "QDNativeRoom");
    CHECK(abi() == QD_NATIVE_ABI);
    QDHostConfig config{sizeof(QDHostConfig), QD_NATIVE_ABI, QD_NATIVE_PROTOCOL, 0, {7, 8}, {0, 1}};
    QDNativeSession* session = nullptr;
    CHECK(create(nullptr, &session) == QD_INVALID); CHECK(session == nullptr);
    config.abi = 999; CHECK(create(&config, &session) == QD_INVALID); config.abi = QD_NATIVE_ABI;
    CHECK(create(&config, &session) == QD_OK); CHECK(session != nullptr);
    QDRoomView room{}; room.struct_size = sizeof(room);
    CHECK(view(session, &room) == QD_OK);
    const auto host = room.members[0].ref;
    CHECK(ready(session, host, 1) == QD_OK);
    QDMemberRef members[3]{};
    for (unsigned i = 0; i < 3; ++i) {
        CHECK(admit(session, {0, i + 2}, QD_NATIVE_PROTOCOL, &members[i]) == QD_OK);
        CHECK(ready(session, members[i], 1) == QD_OK);
    }
    QDMemberRef extra{};
    CHECK(admit(session, {0, 9}, QD_NATIVE_PROTOCOL, &extra) == QD_FULL);
    CHECK(down(session, members[1], 1, 1) == QD_OK);
    CHECK(leave(session, members[0]) == QD_OK);
    CHECK(admit(session, {0, 2}, QD_NATIVE_PROTOCOL, &extra) == QD_OK);
    CHECK(extra.connection != members[0].connection);
    CHECK(ready(session, members[0], 1) == QD_STALE);
    CHECK(view(session, &room) == QD_OK); CHECK(room.members[2].down == 1);
    CHECK(travel(session, 2) == QD_OK); CHECK(ready(session, members[1], 1) == QD_STALE);
    CHECK(leave(session, host) == QD_OK); CHECK(view(session, &room) == QD_OK); CHECK(room.closed == 1);
    destroy(session); destroy(nullptr); CHECK(FreeLibrary(dll));
    std::printf("PASS DLL load/export/ABI and 4-member lifecycle: %u checks; UE4SS absent\n", checks);
}
