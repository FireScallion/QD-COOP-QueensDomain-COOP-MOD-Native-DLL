using SoundVector=std::array<float,3>;
struct SoundLocationInput {
    void* world{};
    void* sound{};
    SoundVector location{},rotation{};
    float volume{},pitch{},start{};
    void* attenuation{};
    void* concurrency{};
};
struct SoundAttachmentInput {
    void* sound{};
    void* parent{};
    AnimU::NameKey socket{};
    SoundVector location{},rotation{};
    unsigned char location_type{},stop_on_destroy{};
    float volume{},pitch{},start{};
    void* attenuation{};
    void* concurrency{};
};
static_assert(sizeof(SoundLocationInput)==72&&offsetof(SoundLocationInput,volume)==40);
static_assert(sizeof(SoundAttachmentInput)==80&&offsetof(SoundAttachmentInput,volume)==52);
using StockSoundLocation=void*(*)(void*,void*,SoundVector,SoundVector,float,float,float,void*,void*,bool);
using StockSoundAttachment=void*(*)(void*,void*,AnimU::NameKey,SoundVector,SoundVector,unsigned char,bool,
                                    float,float,float,void*,void*,bool);
static StockSoundLocation sound_stock_location;
static StockSoundAttachment sound_stock_attachment;

static bool sound_observing_frame(void* frame){
    return GetCurrentThreadId()==game_thread.load()&&!sound_rendering&&active_peer_mask&&envelope.world&&frame;
}
static void* sound_reflected_params(void* frame,const SoundHook& hook){
    return AnimU::field<void*>(frame,16)==api.weakget(&hook.function)?AnimU::field<void*>(frame,40):nullptr;
}
static void observe_sound_location(void* context,void* frame,void* result){
    const auto original=sound_location_hook.original.load(std::memory_order_acquire);
    if(!sound_observing_frame(frame)||!result){
        original(context,frame,result);
        return;
    }
    SoundLocationInput args;
    auto& code=*reinterpret_cast<const unsigned char**>(static_cast<unsigned char*>(frame)+32);
    bool muted{};
    if(code){
        unsigned auto_destroy{};
        for(auto field:{static_cast<void*>(&args.world),static_cast<void*>(&args.sound),static_cast<void*>(&args.location),
                        static_cast<void*>(&args.rotation),static_cast<void*>(&args.volume),static_cast<void*>(&args.pitch),
                        static_cast<void*>(&args.start),static_cast<void*>(&args.attenuation),static_cast<void*>(&args.concurrency)})
            native_vm_argument(frame,field);
        native_vm_argument(frame,&auto_destroy);
        if(code)++code;
        muted=sound_mute_replica(args.world,args.volume);
        const auto component=sound_stock_location(args.world,args.sound,args.location,args.rotation,args.volume,args.pitch,args.start,
                                                   args.attenuation,args.concurrency,auto_destroy!=0);
        std::memcpy(result,&component,sizeof(component));
    }else{
        const auto params=sound_reflected_params(frame,sound_location_hook);
        if(params){
            std::memcpy(&args,params,sizeof(args));
            muted=sound_mute_replica(args.world,args.volume);
        }
        sound_call_with_mute(original,context,frame,result,params,40,muted);
    }
    if(!muted)sound_capture_component(args.world,AnimU::field<void*>(result,0),args.start);
}
static void observe_sound_attached(void* context,void* frame,void* result){
    const auto original=sound_attached_hook.original.load(std::memory_order_acquire);
    if(!sound_observing_frame(frame)||!result){
        original(context,frame,result);
        return;
    }
    SoundAttachmentInput args;
    auto& code=*reinterpret_cast<const unsigned char**>(static_cast<unsigned char*>(frame)+32);
    bool muted{};
    if(code){
        unsigned stop_on_destroy{},auto_destroy{};
        for(auto field:{static_cast<void*>(&args.sound),static_cast<void*>(&args.parent),static_cast<void*>(&args.socket),
                        static_cast<void*>(&args.location),static_cast<void*>(&args.rotation),static_cast<void*>(&args.location_type),
                        static_cast<void*>(&stop_on_destroy),static_cast<void*>(&args.volume),static_cast<void*>(&args.pitch),
                        static_cast<void*>(&args.start),static_cast<void*>(&args.attenuation),static_cast<void*>(&args.concurrency)})
            native_vm_argument(frame,field);
        native_vm_argument(frame,&auto_destroy);
        if(code)++code;
        muted=sound_mute_replica(args.parent,args.volume);
        const auto component=sound_stock_attachment(args.sound,args.parent,args.socket,args.location,args.rotation,args.location_type,
            stop_on_destroy!=0,args.volume,args.pitch,args.start,args.attenuation,args.concurrency,auto_destroy!=0);
        std::memcpy(result,&component,sizeof(component));
    }else{
        const auto params=sound_reflected_params(frame,sound_attached_hook);
        if(params){
            std::memcpy(&args,params,sizeof(args));
            muted=sound_mute_replica(args.parent,args.volume);
        }
        sound_call_with_mute(original,context,frame,result,params,52,muted);
    }
    if(!muted)sound_capture_component(args.parent,AnimU::field<void*>(result,0),args.start);
}
