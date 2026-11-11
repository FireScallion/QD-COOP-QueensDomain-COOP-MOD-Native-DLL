// QA only. Observe the verified game's delay-import dispatch without driving it.
using SteamPump=void(*)();
std::atomic<SteamPump> observed_pump{};
std::atomic<unsigned> engine_dispatches{};

void observe_engine_dispatch() {
    engine_dispatches.fetch_add(1,std::memory_order_relaxed);
    observed_pump.load(std::memory_order_acquire)();
}

void bind_dispatch_probe(HMODULE module) {
    const auto wrapper=reinterpret_cast<void*>(&observe_engine_dispatch);
    SteamPump pump{};
    if(!load_export(module,"SteamAPI_RunCallbacks",pump))return;
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    auto slot=reinterpret_cast<void* volatile*>(image+0x4964B58);
    const auto expected=*slot;
    if(expected!=reinterpret_cast<void*>(pump))return;
    auto page=const_cast<void**>(slot);
    DWORD protection{},ignored{};
    if(!VirtualProtect(page,sizeof(void*),PAGE_READWRITE,&protection))return;
    observed_pump.store(pump,std::memory_order_release);
    const auto prior=InterlockedCompareExchangePointer(slot,wrapper,expected);
    if(!VirtualProtect(page,sizeof(void*),protection,&ignored)) {
        if(prior==expected)InterlockedCompareExchangePointer(slot,expected,wrapper);
        VirtualProtect(page,sizeof(void*),protection,&ignored);
    }
}
