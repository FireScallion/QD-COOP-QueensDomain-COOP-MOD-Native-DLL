// This small MSVC-built boundary matches UE4SS's MSVC std::function ABI.
// The main core is compiler-agnostic C ABI. No C++ standard-library object
// crosses between this bridge and the MinGW-built core.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <functional>
#include <cstring>
namespace RC { namespace Unreal { class UEngine; } }
using Tick = void(*)(float);
static Tick tick;
extern "C" __declspec(dllexport) int QDInstallEngineTick(Tick function) {
    if(tick)return tick==function;
    auto dll=GetModuleHandleW(L"UE4SS.dll");if(!dll||!function)return 0;
    using Register=void(*)(std::function<void(RC::Unreal::UEngine*,float)>);
    auto raw=GetProcAddress(dll,"?RegisterEngineTickPreCallback@Hook@Unreal@RC@@YAXV?$function@$$A6AXPEAVUEngine@Unreal@RC@@M@Z@std@@@Z");
    Register install{};static_assert(sizeof(raw)==sizeof(install));std::memcpy(&install,&raw,sizeof(raw));if(!install)return 0;
    HMODULE self{},core{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&QDInstallEngineTick),&self))return 0;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(function),&core))return 0;
    tick=function;
    install([](RC::Unreal::UEngine*,float dt){tick(dt);});
    return 1;
}
