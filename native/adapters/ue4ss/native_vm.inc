using NativeVmStep=void(*)(void*,void*,void*);
static NativeVmStep native_vm_step,native_vm_explicit;

static bool native_bind_vm(){
    if(native_vm_step&&native_vm_explicit)return true;
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto step=image+0x11b1870,explicit_step=image+0x11b18a0;
    constexpr unsigned char step_prefix[]{0x48,0x8b,0x41,0x20,0x4c,0x8b,0xd2,0x48,0x8b,0xd1,0x44,0x0f,0xb6,0x08};
    constexpr unsigned char explicit_prefix[]{0x41,0x8b,0x40,0x40,0x4d,0x8b,0xc8,0x4c,0x8b,0xd1,0x48,0x0f,0xba,0xe0,0x08};
    if(std::memcmp(reinterpret_cast<void*>(step),step_prefix,sizeof(step_prefix))||
       std::memcmp(reinterpret_cast<void*>(explicit_step),explicit_prefix,sizeof(explicit_prefix)))return false;
    native_vm_step=reinterpret_cast<NativeVmStep>(step);
    native_vm_explicit=reinterpret_cast<NativeVmStep>(explicit_step);
    return true;
}
static void native_vm_argument(void* frame,void* output){
    if(AnimU::field<const unsigned char*>(frame,32)){
        native_vm_step(frame,AnimU::field<void*>(frame,24),output);
        return;
    }
    auto& next=*reinterpret_cast<void**>(static_cast<unsigned char*>(frame)+128);
    const auto current=next;
    next=AnimU::field<void*>(current,32);
    native_vm_explicit(frame,output,current);
}
