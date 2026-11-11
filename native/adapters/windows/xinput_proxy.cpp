#include "sibling_module.hpp"
#include <xinput.h>

namespace {
INIT_ONCE input_once=INIT_ONCE_STATIC_INIT;
HMODULE system_input{};

BOOL CALLBACK bind_system_input(PINIT_ONCE,void*,void**) {
    system_input=qdcoop::windows::load_system(L"xinput1_3.dll");
    return system_input!=nullptr;
}

template<class Function>
Function input(const char* name) {
    Function function{};
    if(InitOnceExecuteOnce(&input_once,bind_system_input,nullptr,nullptr))
        qdcoop::windows::load_export(system_input,name,function);
    return function;
}

DWORD WINAPI start_native(void* self) {
    const auto module=qdcoop::windows::load_sibling(static_cast<HMODULE>(self),L"QDNativeBootstrap.dll");
    if(module) {
        void (*start)(){};
        if(qdcoop::windows::load_export(module,"QDNativeStart",start))start();
    }
    return 0;
}
}

extern "C" DWORD WINAPI proxy_state(DWORD index,XINPUT_STATE* state) {
    static const auto call=input<decltype(&XInputGetState)>("XInputGetState");
    return call?call(index,state):ERROR_DEVICE_NOT_CONNECTED;
}
extern "C" DWORD WINAPI proxy_vibration(DWORD index,XINPUT_VIBRATION* vibration) {
    static const auto call=input<decltype(&XInputSetState)>("XInputSetState");
    return call?call(index,vibration):ERROR_DEVICE_NOT_CONNECTED;
}
extern "C" DWORD WINAPI proxy_capabilities(DWORD index,DWORD flags,XINPUT_CAPABILITIES* capabilities) {
    static const auto call=input<decltype(&XInputGetCapabilities)>("XInputGetCapabilities");
    return call?call(index,flags,capabilities):ERROR_DEVICE_NOT_CONNECTED;
}
extern "C" void WINAPI proxy_enable(BOOL enabled) {
    static const auto call=input<decltype(&XInputEnable)>("XInputEnable");
    if(call)call(enabled);
}
extern "C" DWORD WINAPI proxy_audio(DWORD index,GUID* render,GUID* capture) {
    using Audio=DWORD (WINAPI*)(DWORD,GUID*,GUID*);
    static const auto call=input<Audio>("XInputGetDSoundAudioDeviceGuids");
    return call?call(index,render,capture):ERROR_DEVICE_NOT_CONNECTED;
}
extern "C" DWORD WINAPI proxy_battery(DWORD index,BYTE device,XINPUT_BATTERY_INFORMATION* battery) {
    static const auto call=input<decltype(&XInputGetBatteryInformation)>("XInputGetBatteryInformation");
    return call?call(index,device,battery):ERROR_DEVICE_NOT_CONNECTED;
}
extern "C" DWORD WINAPI proxy_keystroke(DWORD index,DWORD reserved,XINPUT_KEYSTROKE* key) {
    static const auto call=input<decltype(&XInputGetKeystroke)>("XInputGetKeystroke");
    return call?call(index,reserved,key):ERROR_DEVICE_NOT_CONNECTED;
}

BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,void*) {
    if(reason==DLL_PROCESS_ATTACH) {
        // The new thread runs after process-attach callbacks return. Never wait here.
        if(const auto thread=CreateThread(nullptr,0,start_native,instance,0,nullptr))CloseHandle(thread);
    }
    return TRUE;
}
