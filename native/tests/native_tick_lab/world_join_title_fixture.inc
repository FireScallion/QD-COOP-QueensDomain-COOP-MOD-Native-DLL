namespace WorldJoinFixture {
using Guid=std::array<int,4>;
constexpr std::array<Guid,3> keys{{{157137,190137,217137,227137},{157138,190138,217138,227138},{157139,190139,217139,227139}}};
constexpr std::array<std::string_view,3> paths{
    "/Game/Blueprints/BreakableBarricade/BreakableBarricade.BreakableBarricade_C",
    "/Game/Props/Containers/Permabreakable_base.Permabreakable_base_C",
    "/Game/Blueprints/Door_Base.Door_Base_C"
};
bool seeded{},joining{},joined{},left{},failed{},spawned{};
enum class Mode { host_saved,guest_saved,guest_breaks,wipe,rest,duck,duck_pickup,door_trigger,special,enemy_lease,heartbeat };
Mode mode(){
    wchar_t value[2]{};
    if(environment_title(L"QD_WORLD_JOIN_REVERSE",value,2))return Mode::guest_saved;
    if(environment_title(L"QD_WORLD_JOIN_LIVE",value,2))return Mode::guest_breaks;
    if(environment_title(L"QD_WORLD_JOIN_WIPE",value,2))return Mode::wipe;
    if(environment_title(L"QD_WORLD_JOIN_REST",value,2))return Mode::rest;
    if(environment_title(L"QD_WORLD_JOIN_DUCK",value,2))return Mode::duck;
    if(environment_title(L"QD_WORLD_JOIN_DUCK_PICKUP",value,2))return Mode::duck_pickup;
    if(environment_title(L"QD_WORLD_JOIN_DOOR_TRIGGER",value,2))return Mode::door_trigger;
    if(environment_title(L"QD_WORLD_JOIN_ENEMY_LEASE",value,2))return Mode::enemy_lease;
    if(environment_title(L"QD_WORLD_JOIN_HEARTBEAT",value,2))return Mode::heartbeat;
    if(GetEnvironmentVariableW(L"QD_WORLD_JOIN_SPECIAL",value,2))return Mode::special;
    return Mode::host_saved;
}
std::array<UE::WeakKey,3> fixtures;
Address begin_spawn{},finish_spawn{},actor_location{},collision{},health_fn{},pit_fn{};
Address get_all_actors{};
unsigned (*status_read)(char*,unsigned){};
std::wstring directory(){
    const std::wstring path=lan.listening;
    return path.substr(0,path.find_last_of(L"/\\"));
}
std::wstring proof(const wchar_t* name){return directory()+L"/"+lan.role+L"-"+name+L".txt";}
Address pawn(Address instance){
    if(!instance||!ui.ready)return 0;
    Address args[]{instance,0,0};
    TitleUi::event(ui.get(TitleUi::gameplay),ui.get(TitleUi::local_pawn),args);
    return args[2];
}
Address method(Address actor,std::string_view name,unsigned bytes){
    auto type=actor?at<Address>(actor,16):0;
    for(unsigned depth=0;type&&depth<32;++depth,type=at<Address>(type,64)){
        unsigned checked{};
        for(auto fn=at<Address>(type,72);fn&&checked<1024;fn=at<Address>(fn,40),++checked)
            if(at<Address>(fn,32)==type&&leaf(fn)==name&&at<std::uint16_t>(fn,182)==bytes)return fn;
    }
    return 0;
}
void scan(){
    const auto array=at<UE::ObjectArray>(image+TitleUi::object_array,0);
    if(!UE::object_array_shape(array))return;
    for(int i=0;i<array.num_elements;++i){
        const auto chunk=at<Address>(array.chunks,(i/UE::objects_per_chunk)*8);
        if(!chunk)continue;
        const auto entry=at<UE::ObjectItem>(chunk,(i%UE::objects_per_chunk)*sizeof(UE::ObjectItem));
        if(!entry.object||(entry.flags&0x30000000u))continue;
        const auto name=leaf(entry.object);
        if(name=="BeginDeferredActorSpawnFromClass"&&path_is(entry.object,"/Script/Engine.GameplayStatics.BeginDeferredActorSpawnFromClass"))
            begin_spawn=entry.object;
        if(name=="FinishSpawningActor"&&path_is(entry.object,"/Script/Engine.GameplayStatics.FinishSpawningActor"))finish_spawn=entry.object;
        if(name=="K2_GetActorLocation"&&path_is(entry.object,"/Script/Engine.Actor.K2_GetActorLocation"))actor_location=entry.object;
        if(name=="GetAllActorsOfClass"&&path_is(entry.object,"/Script/Engine.GameplayStatics.GetAllActorsOfClass"))
            get_all_actors=entry.object;
        if(name=="GetActorEnableCollision"&&path_is(entry.object,"/Script/Engine.Actor.GetActorEnableCollision"))collision=entry.object;
        if(name=="Set Health Points"&&path_is(entry.object,"/Game/Player/FS_PlayerChar.FS_PlayerChar_C.Set Health Points"))
            health_fn=entry.object;
        if(name=="ApplyPitDamage"&&path_is(entry.object,"/Game/Player/FS_PlayerChar.FS_PlayerChar_C.ApplyPitDamage"))
            pit_fn=entry.object;
    }
}
struct ActorFixtureArray {
    Address* data{};
    int count{},capacity{};
    ~ActorFixtureArray(){
        if(data)reinterpret_cast<void(*)(void*)>(image+0xeae0b0)(data);
    }
    ActorFixtureArray()=default;
    ActorFixtureArray(const ActorFixtureArray&)=delete;
    ActorFixtureArray& operator=(const ActorFixtureArray&)=delete;
};
bool collect_world_actors(Address player,Address type,ActorFixtureArray& out){
    if(!player||!type||!get_all_actors||at<std::uint16_t>(get_all_actors,182)!=32||
       at<Address>(get_all_actors,216)!=image+0x2dbe520)return false;
    constexpr unsigned char expected[]{0x40,0x55,0x53,0x56,0x57,0x48,0x8d,0x6c,0x24,0xc1};
    if(std::memcmp(reinterpret_cast<void*>(image+0x28dda50),expected,sizeof(expected)))return false;
    reinterpret_cast<void(*)(Address,Address,ActorFixtureArray*)>(image+0x28dda50)(player,type,&out);
    return out.count>=0&&out.count<=8192&&out.capacity>=out.count&&(!out.count||out.data);
}
bool seed(Address instance){
    if(seeded)return true;
    if(!pawn(instance))return false;
    const auto save=ui.object(instance,"ActiveSaveSlot"),map=ui.member(save,"SavableActorStates","MapProperty",80);
    if(!map||at<std::array<int,6>>(map,0x88)!=std::array<int,6>{16,20,24,28,4,28})return false;
    const auto put=reinterpret_cast<void(*)(void*,const void*,const void*,const void*)>(image+0x2755ec0);
    const auto address=reinterpret_cast<void*>(save+at<unsigned>(map,76));
    const int positive=1;
    for(const auto& key:keys)put(address,reinterpret_cast<void*>(map),key.data(),&positive);
    seeded=true;
    marker(proof(L"world-seeded").c_str());
    return true;
}
bool seed_host(Address instance){return mode()!=Mode::host_saved||seed(instance);}
void seed_guest(Address instance){if(mode()==Mode::guest_saved)seed(instance);}
Address hud_canvas(Address instance){
    const auto player=pawn(instance);
    const auto tree=ui.object(ui.object(player,"HUD"),"WidgetTree");
    if(tree)return ui.object(tree,"RootWidget");
    if(!ui.ready||!ui.get(ui.active_title))return 0;
    return ui.object(ui.object(ui.get(ui.active_title),"WidgetTree"),"RootWidget");
}
void observe(Address instance){
    const auto canvas=hud_canvas(instance);
    if(!ui.is(canvas,ui.get(TitleUi::canvas_type)))return;
    for(int i=0;i<ui.count(canvas);++i){
        const auto widget=ui.child(canvas,i);
        if(!ui.is(widget,ui.get(TitleUi::text_type))||ui.visibility(widget)!=3)continue;
        const auto text=ui.caption(widget);
        const auto slot=ui.object(widget,"Slot");
        if(text.empty()||!slot)continue;
        std::array<float,10> layout{};
        TitleUi::event(slot,ui.get(TitleUi::canvas_layout),layout.data());
        if(layout[4]!=.5f||layout[5]!=0.f||layout[6]!=.5f||layout[7]!=0.f||layout[8]!=.5f)continue;
        const auto is_joining=lan.host?text==L"LAN player 2  ·  Joining…":text==L"You  ·  Joining…";
        const auto is_joined=lan.host?text==L"LAN player 2  ·  Joined":text==L"You  ·  Joined";
        if(is_joining){joining=true;marker(proof(L"joining").c_str());}
        if(is_joined){joined=true;marker(proof(L"joined").c_str());}
        if(text==L"LAN host  ·  Left"){left=true;marker(proof(L"left").c_str());}
        if(text==L"You  ·  Could not join"){failed=true;marker(proof(L"join-failed").c_str());}
    }
}
Address spawn(Address player,Address type,unsigned index,bool require_guid=true){
    if(!player||!type||!begin_spawn||!finish_spawn||!actor_location)return 0;
    std::array<float,3> location{};
    TitleUi::event(player,actor_location,location.data());
    const std::array<float,12> transform{0,0,0,1,location[0]+400.f+200.f*index,location[1],location[2]+70.f,0,1,1,1,0};
    alignas(16) std::array<unsigned char,96> args{},done{};
    std::memcpy(args.data(),&player,8);
    std::memcpy(args.data()+8,&type,8);
    std::memcpy(args.data()+16,transform.data(),48);
    args[64]=1;
    const auto gameplay=ui.get(TitleUi::gameplay);
    TitleUi::event(gameplay,begin_spawn,args.data());
    const auto actor=UE::field<Address>(args.data(),80);
    if(!actor)return 0;
    std::memcpy(done.data(),&actor,8);
    std::memcpy(done.data()+16,transform.data(),48);
    TitleUi::event(gameplay,finish_spawn,done.data());
    const auto prop=ui.member(actor,"GUIDs","ArrayProperty",16);
    if(UE::field<Address>(done.data(),64)!=actor)return 0;
    if(!require_guid)return actor;
    if(!prop)return 0;
    struct Array {Guid* data;int count,capacity;};
    const auto guids=at<Array>(actor,at<unsigned>(prop,76));
    if(!guids.data||guids.count<1||guids.capacity<guids.count)return 0;
    guids.data[0]=keys[index];
    return actor;
}
UE::WeakKey wipe_original_save;
bool wipe_cut_enabled(){
    wchar_t enabled[2]{};
    return environment_title(L"QD_WORLD_JOIN_CUT_WIPE",enabled,2);
}
bool wipe_original_binding(Address instance){
    const auto save=ui.get(wipe_original_save);
    if(!save||ui.object(instance,"ActiveSaveSlot")!=save||ui.object(instance,"BaseActiveSave")!=save)return false;
    marker(proof(L"wipe-original-save").c_str());
    return true;
}
bool check_wipe(Address instance){
    namespace CP=qdcoop::checkpoint;
    static int (*inspect)(CP::BindingView*){};
    static bool fired{};
    if(!inspect)if(const auto core=GetModuleHandleW(L"QDCoopNativeM1.dll"))load_export(core,"QDEnemyInspectNativeCheckpoint",inspect);
    CP::BindingView view{};
    if(!inspect||!inspect(&view))return false;
    if(auto stream=_wfopen(proof(L"wipe-status").c_str(),L"wb")){
        std::fprintf(stream,"token=%llu nonce=%llu rollbacks=%llu flags=%u stage=%u failure=%u errors=%llu\n",
            static_cast<unsigned long long>(view.latest.token),static_cast<unsigned long long>(view.world_nonce),
            static_cast<unsigned long long>(view.rollbacks),view.flags,static_cast<unsigned>(view.rollback_stage),
            view.failure,static_cast<unsigned long long>(view.errors));
        std::fclose(stream);
    }
    if(wipe_cut_enabled()&&!lan.host&&view.rollback_stage==CP::RollbackStage::committed&&
       !ui.get(wipe_original_save)){
        const auto save=ui.object(instance,"ActiveSaveSlot");
        if(save&&save==ui.object(instance,"BaseActiveSave")){
            ui.remember(wipe_original_save,save);
            marker(proof(L"wipe-committed").c_str());
        }
    }
    if(view.rollbacks&&view.world_nonce&&view.latest.token&&(view.flags&1)){
        marker(proof(L"world-applied").c_str());
        return lan.host&&marker_exists((directory()+L"/client-world-applied.txt").c_str());
    }
    if(fired){
        if(view.flags&2)marker(proof(L"wipe-down").c_str());
        return false;
    }
    if(!lan.host&&!marker_exists((directory()+L"/host-wipe-down.txt").c_str()))return false;
    if(!view.latest.token||(view.flags&5)!=5)return false;
    const auto player=pawn(instance);
    if(!health_fn||!pit_fn)scan();
    if(!player||!health_fn||!pit_fn||at<std::uint16_t>(health_fn,182)>256||at<std::uint16_t>(pit_fn,182)>256)return false;
    alignas(16) std::array<unsigned char,256> health{},pit{};
    float one=1.f;
    std::memcpy(health.data(),&one,sizeof(one));
    pit[0]=1;
    TitleUi::event(player,health_fn,health.data());
    TitleUi::event(player,pit_fn,pit.data());
    fired=true;
    marker(proof(L"wipe-fired").c_str());
    return false;
}
bool check_duck(Address instance,bool pickup_only){
    static Address strings{},make_name{};
    static UE::WeakKey checkpoint;
    static UE::WeakKey duck;
    static std::array<UE::NameKey,5> keys{};
    static bool seeded_kill{},picked_up{},interacted{},prompt_closed{},teleporting_seen{};
    static ULONGLONG stable_since{},prompt_opened_at{},host_travel_at{};
    static bool shop_verified{},host_moved{};
    static int starting_count{};
    wchar_t race_flag[2]{},return_flag[2]{};
    const bool race=environment_title(L"QD_WORLD_JOIN_DUCK_RACE",race_flag,2);
    const bool return_to_host=environment_title(L"QD_WORLD_JOIN_DUCK_RETURN",return_flag,2);
    const bool close_after_fade=race||return_to_host;
    constexpr std::array guests{L"client",L"client2",L"client3"};
    const auto all_guests=[&](const wchar_t* suffix){
        return std::ranges::all_of(guests,[&](const auto role){
            return marker_exists((directory()+L"/"+role+L"-"+suffix+L".txt").c_str());
        });
    };
    const auto player=pawn(instance),save=ui.object(instance,"ActiveSaveSlot");
    const auto map=ui.member(save,"NamedSaveData","MapProperty",80);
    if(!player||!map||at<std::array<int,6>>(map,0x88)!=std::array<int,6>{8,12,16,20,4,20})return false;
    if(!actor_location)scan();
    if(!strings||!make_name||(!pickup_only&&!lan.host&&!ui.get(checkpoint))){
        const auto array=at<UE::ObjectArray>(image+TitleUi::object_array,0);
        if(!UE::object_array_shape(array))return false;
        std::array<float,3> origin{};
        if(actor_location)TitleUi::event(player,actor_location,origin.data());
        float best=std::numeric_limits<float>::infinity();
        for(int i=0;i<array.num_elements;++i){
            const auto chunk=at<Address>(array.chunks,(i/UE::objects_per_chunk)*8);
            if(!chunk)continue;
            const auto entry=at<UE::ObjectItem>(chunk,(i%UE::objects_per_chunk)*sizeof(UE::ObjectItem));
            const auto object=entry.object;
            if(!object||(entry.flags&0x30000000u))continue;
            const auto name=leaf(object);
            if(name=="Conv_StringToName"&&path_is(object,"/Script/Engine.KismetStringLibrary.Conv_StringToName"))make_name=object;
            if(name=="Default__KismetStringLibrary"&&path_is(object,"/Script/Engine.Default__KismetStringLibrary"))strings=object;
            if(!pickup_only&&!lan.host&&actor_location&&!(at<unsigned>(object,8)&0x30u)&&!name.starts_with("Default__")&&
               leaf(at<Address>(object,16))=="Checkpoint_C"&&method(object,"Interact",8)){
                std::array<float,3> at_checkpoint{};
                TitleUi::event(object,actor_location,at_checkpoint.data());
                float distance{};
                for(unsigned axis=0;axis<3;++axis)distance+=(at_checkpoint[axis]-origin[axis])*(at_checkpoint[axis]-origin[axis]);
                if(distance<best){best=distance;ui.remember(checkpoint,object);}
            }
        }
    }
    if(!strings||!ui.method(make_name,24,{{"inString","StrProperty",0,16},{"ReturnValue","NameProperty",16,8}}))return false;
    constexpr std::array names{L"ShrineBossKilled",L"ShrineBossRewardCount",L"ShopUnlockFinished",
                               L"ShopUnlocks",L"QDCoopPersonal~story~stoneDuck"};
    for(unsigned i=0;i<keys.size();++i)if(!keys[i].index){
        const auto size=static_cast<int>(std::wcslen(names[i])+1);
        struct Args {const wchar_t* text;int size,capacity;UE::NameKey result;};
        Args args{names[i],size,size,{}};
        TitleUi::event(strings,make_name,&args);
        if(!args.result.index)return false;
        keys[i]=args.result;
    }
    const auto address=reinterpret_cast<void*>(save+at<unsigned>(map,76)),field=reinterpret_cast<void*>(map);
    const auto put=reinterpret_cast<void(*)(void*,const void*,const void*,const void*)>(image+0x2755ec0);
    const auto find=reinterpret_cast<bool(*)(void*,const void*,const void*,void*)>(image+0x2756200);
    const auto value=[&](unsigned i){int out{};find(address,field,&keys[i],&out);return out;};
    const auto write=[&](unsigned i,int data){put(address,field,&keys[i],&data);};
    if(return_to_host&&lan.host&&marker_exists((directory()+L"/client-duck-shop-arrived.txt").c_str())&&!host_moved){
        const auto quick=ui.member(player,"QuickTeleporting","BoolProperty",1);
        if(!quick||!actor_location)return false;
        const auto flag=[&](bool set){UE::set_bool(reinterpret_cast<void*>(quick),reinterpret_cast<void*>(player),set,0);};
        const auto now=GetTickCount64();
        if(!host_travel_at){
            flag(true);
            host_travel_at=now;
            return false;
        }
        if(now-host_travel_at<650)return false;
        const auto move=method(player,"K2_SetActorLocationAndRotation",166);
        const auto rotate=method(player,"K2_GetActorRotation",12);
        if(!rotate||!ui.method(move,166,{{"NewLocation","StructProperty",0,12},{"NewRotation","StructProperty",12,12},
             {"bSweep","BoolProperty",24,1},{"SweepHitResult","StructProperty",28,136},
             {"bTeleport","BoolProperty",164,1},{"ReturnValue","BoolProperty",165,1}}))return false;
        alignas(16) std::array<unsigned char,176> args{};
        TitleUi::event(player,actor_location,args.data());
        auto location=UE::field<std::array<float,3>>(args.data(),0);
        location[0]+=160.f;
        std::memcpy(args.data(),location.data(),12);
        TitleUi::event(player,rotate,args.data()+12);
        args[164]=1;
        TitleUi::event(player,move,args.data());
        flag(false);
        if(!args[165])return false;
        if(auto file=_wfopen(proof(L"duck-host-arrival").c_str(),L"wb")){
            host_moved=std::fwrite(location.data(),sizeof(float),location.size(),file)==location.size();
            std::fclose(file);
        }
        return false;
    }
    if(return_to_host&&!lan.host&&shop_verified){
        std::array<float,3> target{},position{};
        const auto path=directory()+L"/host-duck-host-arrival.txt";
        auto file=_wfopen(path.c_str(),L"rb");
        if(!file)return false;
        const bool loaded=std::fread(target.data(),sizeof(float),target.size(),file)==target.size();
        std::fclose(file);
        if(!loaded||value(2)!=1||value(3)!=starting_count+1)return false;
        TitleUi::event(player,actor_location,position.data());
        float distance{};
        for(unsigned i=0;i<target.size();++i)distance+=(target[i]-position[i])*(target[i]-position[i]);
        if(distance<=75.f*75.f)marker(proof(L"world-applied").c_str());
        return false;
    }
    if(pickup_only){
        static UE::WeakKey shrine,reward;
        static bool ready{};
        if(lan.host){
            if(marker_exists((directory()+L"/client-duck-pickup-ready.txt").c_str())&&!seeded_kill){
                if(value(1)||value(2))return false;
                write(0,1);
                write(1,1);
                seeded_kill=true;
            }
            return seeded_kill&&marker_exists((directory()+L"/client-world-applied.txt").c_str());
        }
        if(!begin_spawn||!finish_spawn||!collision)scan();
        if(!ui.get(shrine)){
            constexpr auto path="/Game/Enemies/Boner_00/Skeletons24/ShrineBoss/Shrineboss-DummyNPC_BP.Shrineboss-DummyNPC_BP_C";
            const auto type=ui.load_asset_path(path);
            if(type)ui.remember(shrine,spawn(player,type,0,false));
        }
        const auto npc=ui.get(shrine);
        if(!npc)return false;
        if(!ready&&!ui.get(reward)){
            const auto setup=method(npc,"SetUpReward",4);
            if(!ui.method(setup,4,{{"Index","IntProperty",0,4}}))return false;
            auto object=ui.object(npc,"Reward Actor");
            if(!object){
                int index{};
                TitleUi::event(npc,setup,&index);
                object=ui.object(npc,"Reward Actor");
            }
            if(!object||!path_is(at<Address>(object,16),
                "/Game/Items/KeyItems/ShopProgressionKey/StoneDuckShopUnlock_BP.StoneDuckShopUnlock_BP_C"))return false;
            ui.remember(reward,object);
        }
        const auto object=ui.get(reward);
        const auto acquired=ui.member(object,"Acquired","BoolProperty",1);
        const auto transient=ui.member(object,"TransientPickup","BoolProperty",1);
        if(!acquired||!transient||!collision)return false;
        const auto flag=[&](Address prop){return UE::get_bool(reinterpret_cast<void*>(prop),reinterpret_cast<void*>(object),0);};
        if(!ready){
            if(flag(acquired)||!flag(transient)||value(1)||value(2))return false;
            ready=true;
            marker(proof(L"duck-pickup-ready").c_str());
            return false;
        }
        unsigned char enabled=1;
        TitleUi::event(object,collision,&enabled);
        if(!flag(acquired)||enabled||value(1)!=1||value(2)||value(4)!=1)return false;
        const auto inventory=ui.member(save,"InventoryContents","MapProperty",80);
        const auto duck_item=ui.load_asset_path("/Game/Items/KeyItems/ShopProgressionKey/StoneDuckShopUnlock.StoneDuckShopUnlock");
        int quantity{};
        if(!inventory||!duck_item||
           !find(reinterpret_cast<void*>(save+at<unsigned>(inventory,76)),reinterpret_cast<void*>(inventory),&duck_item,&quantity)||
           quantity!=1)return false;
        marker(proof(L"world-applied").c_str());
        return false;
    }
    if(lan.host&&!seeded_kill){
        starting_count=value(3);
        if(value(2)>0)return false;
        write(0,1);
        if(race)write(1,1);
        seeded_kill=true;
        marker(proof(L"duck-killed").c_str());
    }
    if(!lan.host&&!picked_up&&value(0)==1&&value(1)==0){
        starting_count=value(3);
        write(1,1);
        picked_up=true;
        marker(proof(L"duck-claimed").c_str());
    }
    if(race&&!lan.host&&!picked_up&&value(1)==1&&!value(2)){
        starting_count=value(3);
        picked_up=true;
    }
    const auto race_file=directory()+L"/duck-race-start.bin";
    if(race&&lan.host&&all_guests(L"duck-race-ready")&&!marker_exists(race_file.c_str())){
        if(auto out=_wfopen(race_file.c_str(),L"wb")){
            const auto start=GetTickCount64()+1000;
            std::fwrite(&start,sizeof(start),1,out);
            std::fclose(out);
        }
    }
    if(!lan.host&&!interacted&&picked_up&&value(1)==1&&value(4)==1&&value(2)==0){
        if(race){
            if(!marker_exists(proof(L"duck-race-ready").c_str()))marker(proof(L"duck-race-ready").c_str());
            ULONGLONG start{};
            if(auto file=_wfopen(race_file.c_str(),L"rb")){
                if(std::fread(&start,sizeof(start),1,file)!=1)start=0;
                std::fclose(file);
            }
            if(!start||GetTickCount64()<start)return false;
        }
        const auto actor=ui.get(checkpoint),interact=method(actor,"Interact",8);
        if(!interact)return false;
        auto argument=player;
        TitleUi::event(actor,interact,&argument);
        interacted=value(2)==1&&value(3)==starting_count+1;
        if(interacted){
            marker(proof(L"duck-stock-interact").c_str());
            if(race)if(auto file=_wfopen(proof(L"duck-race-interact").c_str(),L"wb")){
                std::fprintf(file,"%llu\n",GetTickCount64());
                std::fclose(file);
            }
        }
    }
    if(!lan.host&&interacted){
        if(!prompt_closed){
            const auto prompt=ui.object(player,"ReadTextUI");
            const auto close=method(prompt,"Close",0);
            const auto opened=ui.member(prompt,"IsOpen","BoolProperty",1);
            const auto visibility=prompt?ui.visibility(prompt):255;
            const auto flag=[&]{return opened&&UE::get_bool(reinterpret_cast<void*>(opened),reinterpret_cast<void*>(prompt),0);};
            if(close&&flag()&&(visibility==0||visibility==3||visibility==4)){
                const auto tick=GetTickCount64();
                if(!prompt_opened_at)prompt_opened_at=tick;
                bool opening=false;
                if(close_after_fade){
                    const auto fade=ui.object(prompt,"FadeIn"),playing=method(prompt,"IsAnimationPlaying",9);
                    if(!fade||!ui.method(playing,9,{{"InAnimation","ObjectProperty",0,8},{"ReturnValue","BoolProperty",8,1}}))
                        return false;
                    alignas(8) std::array<unsigned char,16> args{};
                    std::memcpy(args.data(),&fade,8);
                    TitleUi::event(prompt,playing,args.data());
                    opening=args[8]!=0;
                }
                if(!opening&&(close_after_fade||tick-prompt_opened_at>=23000)){
                    TitleUi::event(prompt,close,nullptr);
                    prompt_closed=!flag();
                    if(prompt_closed)marker(proof(L"duck-stock-prompt-closed").c_str());
                }
            }
        }
        const auto teleporting=ui.member(player,"Teleporting","BoolProperty",1);
        const bool in_transit=teleporting&&UE::get_bool(reinterpret_cast<void*>(teleporting),reinterpret_cast<void*>(player),0);
        teleporting_seen|=in_transit;
        if(!ui.get(duck)){
            const auto item=ui.load_asset_path("/Game/Items/KeyItems/ShopProgressionKey/StoneDuckShopUnlock.StoneDuckShopUnlock");
            if(!item)return false;
            ui.remember(duck,item);
        }
        const auto inventory=ui.member(save,"InventoryContents","MapProperty",80);
        const auto item=ui.get(duck);
        int quantity{};
        if(!inventory||!item)return false;
        find(reinterpret_cast<void*>(save+at<unsigned>(inventory,76)),reinterpret_cast<void*>(inventory),&item,&quantity);
        if(auto file=_wfopen(proof(L"duck-state").c_str(),L"wb")){
            std::fprintf(file,"prompt_closed=%u teleporting_seen=%u in_transit=%u inventory=%d claimed=%d unlocked=%d unlocks=%d\n",
                prompt_closed,teleporting_seen,in_transit,quantity,value(1),value(2),value(3));
            std::fclose(file);
        }
        if(!teleporting_seen||in_transit||quantity!=0)return false;
        std::array<float,3> position{};
        TitleUi::event(player,actor_location,position.data());
        constexpr std::array shop{5706.726f,44269.910f,2471.736f};
        float distance{};
        for(unsigned axis=0;axis<3;++axis)distance+=(position[axis]-shop[axis])*(position[axis]-shop[axis]);
        if(distance>600.f*600.f){stable_since=0;return false;}
    }
    if(value(1)!=1||value(2)!=1||value(3)!=starting_count+1||(!lan.host&&!interacted)){
        stable_since=0;
        return false;
    }
    if(!stable_since)stable_since=GetTickCount64();
    if(GetTickCount64()-stable_since<5500)return false;
    if(return_to_host&&!lan.host){
        shop_verified=true;
        marker(proof(L"duck-shop-arrived").c_str());
        return false;
    }
    marker(proof(L"world-applied").c_str());
    return lan.host&&(race?all_guests(L"world-applied"):marker_exists((directory()+L"/client-world-applied.txt").c_str()));
}
bool set_fixture_array(Address pawn,Address allocation_type,Address actor,const char* name,
                       const void* value,std::size_t bytes){
    const auto property=ui.member(actor,name,"ArrayProperty",16);
    if(!property||(bytes!=16&&bytes!=32))return false;
    const auto inner=at<Address>(property,0x78);
    if(!inner||at<unsigned>(inner,60)!=16)return false;
    const auto count=static_cast<int>(bytes/16);
    struct Array {void* data;int count,capacity;};
    const auto address=actor+at<unsigned>(property,76);
    auto row=at<Array>(address,0);
    if(row.data&&row.capacity>=count){
        std::memcpy(row.data,value,bytes);
        row.count=count;
    }else{
        if(row.data||row.count||row.capacity)return false;
        ActorFixtureArray allocation;
        if(!collect_world_actors(pawn,allocation_type,allocation)||
           !allocation.data||allocation.capacity<2*count)return false;
        row={allocation.data,count,count};
        std::memcpy(row.data,value,bytes);
        allocation.data=nullptr;
    }
    std::memcpy(reinterpret_cast<void*>(address),&row,sizeof(row));
    return true;
}
bool check_door_trigger(Address instance,std::string_view status){
    static UE::WeakKey trigger,door,other;
    static bool prepared{},fired{},closed{};
    static ULONGLONG since{};
    wchar_t overlap_flag[2]{},mixed_flag[2]{},close_flag[2]{};
    const bool from_overlap=environment_title(L"QD_WORLD_JOIN_TRIGGER_OVERLAP",overlap_flag,2);
    const bool mixed=environment_title(L"QD_WORLD_JOIN_MIXED_DOOR",mixed_flag,2);
    const bool close_test=environment_title(L"QD_WORLD_JOIN_DOOR_CLOSE",close_flag,2);
    const bool close_lever=close_test&&close_flag[0]==L'2';
    const auto player=pawn(instance);
    if(!player||(!lan.host&&status.find("\"world_arrival_applied\":0")!=status.npos))return false;
    if(!begin_spawn||!finish_spawn||!actor_location||!get_all_actors)scan();
    if(!prepared){
        const auto trigger_type=ui.load_asset_path("/Game/Blueprints/ActivationTrigger.ActivationTrigger_C");
        const auto door_type=ui.load_asset_path(paths[2]);
        if(!trigger_type||!door_type||!get_all_actors)return false;
        if(!ui.get(door))ui.remember(door,spawn(player,door_type,0,false));
        if(!ui.get(trigger))ui.remember(trigger,spawn(player,trigger_type,1,false));
        if(mixed&&!ui.get(other))ui.remember(other,spawn(player,trigger_type,2,false));
        if(close_lever&&!ui.get(other)){
            const auto type=ui.load_asset_path("/Game/LD45/Blueprints/Objects/Gate/Lever.Lever_C");
            if(type)ui.remember(other,spawn(player,type,2,false));
        }
        const auto d=ui.get(door),t=ui.get(trigger);
        if(!d||!t||((mixed||close_lever)&&!ui.get(other)))return false;
        const auto script=ui.member(d,"ScriptOpenable","BoolProperty",1);
        const auto opened=ui.member(d,"Opened","BoolProperty",1);
        const auto required=ui.member(d,"ActivationsNeededForOpen","IntProperty",4);
        const auto count=ui.member(d,"CurrentActivationCount","IntProperty",4);
        const auto once=ui.member(t,"OnlyOnce","BoolProperty",1);
        const auto latch=ui.member(t,"HasAlreadybeenTriggered","BoolProperty",1);
        if(!script||!opened||!required||!count||!once||!latch)return false;
        const std::array<Address,4> link{d,0,ui.get(other),0};
        if(!set_fixture_array(player,trigger_type,d,"GUIDs",keys[0].data(),sizeof(keys[0]))||
           !set_fixture_array(player,trigger_type,t,"GUIDs",keys[1].data(),sizeof(keys[1]))||
           !set_fixture_array(player,trigger_type,t,"ActivationTargets",link.data(),mixed?32:16))return false;
        const auto boolean=[&](Address property,Address actor,bool value){
            UE::set_bool(reinterpret_cast<void*>(property),reinterpret_cast<void*>(actor),value,0);
        };
        boolean(script,d,true);
        boolean(opened,d,false);
        boolean(once,t,false);
        boolean(latch,t,false);
        if(close_lever){
            const auto lever=ui.get(other),unsaved=ui.member(lever,"DisableSaving","BoolProperty",1);
            if(!unsaved||!set_fixture_array(player,trigger_type,lever,"GUIDs",keys[2].data(),sizeof(keys[2]))||
               !set_fixture_array(player,trigger_type,lever,"ActivatableTargets",link.data(),16))return false;
            boolean(unsaved,lever,true);
        }
        if(from_overlap)for(const auto field:{"BoxMode","TriggerOnExit","Require Activation Before Use",
            "WaitForPlayerGroundedBeforeTrigger"}){
            const auto property=ui.member(t,field,"BoolProperty",1);
            if(!property)return false;
            boolean(property,t,false);
        }
        const int zero{},one=1;
        std::memcpy(reinterpret_cast<void*>(d+at<unsigned>(required,76)),&one,sizeof(one));
        std::memcpy(reinterpret_cast<void*>(d+at<unsigned>(count,76)),&zero,sizeof(zero));
        since=GetTickCount64();
        prepared=true;
        marker(proof(L"door-trigger-prepared").c_str());
        return false;
    }
    const auto actor=ui.get(trigger),target=ui.get(door);
    const auto count=ui.member(target,"CurrentActivationCount","IntProperty",4);
    if(!actor||!target||!count)return false;
    if(!lan.host&&!fired){
        if(GetTickCount64()-since<1800||!marker_exists((directory()+L"/host-door-trigger-prepared.txt").c_str()))return false;
        if(from_overlap){
            constexpr auto name="BndEvt__ActivationTrigger_Sphere_K2Node_ComponentBoundEvent_0_ComponentBeginOverlapSignature__DelegateSignature";
            const auto overlap=method(actor,name,168),sphere=ui.object(actor,"Sphere"),capsule=ui.object(player,"CapsuleComponent");
            if(!sphere||!capsule||!ui.method(overlap,168,{{"OverlappedComponent","ObjectProperty",0,8},
                {"OtherActor","ObjectProperty",8,8},{"OtherComp","ObjectProperty",16,8},{"OtherBodyIndex","IntProperty",24,4},
                {"bFromSweep","BoolProperty",28,1},{"SweepResult","StructProperty",32,136}}))return false;
            alignas(16) std::array<unsigned char,168> args{};
            std::memcpy(args.data(),&sphere,sizeof(sphere));
            std::memcpy(args.data()+8,&player,sizeof(player));
            std::memcpy(args.data()+16,&capsule,sizeof(capsule));
            TitleUi::event(actor,overlap,args.data());
        }else{
            const auto activate=method(actor,"DoTrigger",0);
            if(!activate)return false;
            TitleUi::event(actor,activate,nullptr);
        }
        fired=true;
        marker(proof(L"door-trigger-fired").c_str());
    }
    const auto counter=lan.host?"\"door_triggers_replayed\":1,":"\"door_triggers_observed\":1,";
    if(close_test){
        const auto open=ui.member(target,"Opened","BoolProperty",1);
        if(!open)return false;
        const bool opened=UE::get_bool(reinterpret_cast<void*>(open),reinterpret_cast<void*>(target),0);
        const auto activations=at<int>(target,at<unsigned>(count,76));
        if(status.find(counter)!=status.npos&&activations==1&&opened)
            marker(proof(L"door-close-open").c_str());
        if(!lan.host&&!closed&&marker_exists(proof(L"door-close-open").c_str())&&
           marker_exists((directory()+L"/host-door-close-open.txt").c_str())){
            const auto deactivate=method(target,"Deactivate",16);
            if(!ui.method(deactivate,16,{{"DeactivateByLoad","BoolProperty",0,1},{"ActorThatTriggered","ObjectProperty",8,8}}))
                return false;
            Address args[]{0,close_lever?ui.get(other):actor};
            TitleUi::event(target,deactivate,args);
            closed=true;
            marker(proof(L"door-close-issued").c_str());
            return false;
        }
        const auto closed_counter=lan.host?"\"door_triggers_replayed\":2,":"\"door_triggers_observed\":2,";
        if(status.find(closed_counter)==status.npos||activations||opened)return false;
        marker(proof(L"world-applied").c_str());
        return lan.host&&marker_exists((directory()+L"/client-world-applied.txt").c_str());
    }
    if(status.find(counter)==status.npos||at<int>(target,at<unsigned>(count,76))!=1)return false;
    if(mixed){
        const auto non_door=ui.get(other),activations=ui.member(non_door,"Activations","IntProperty",4);
        if(!activations||at<int>(non_door,at<unsigned>(activations,76))!=(lan.host?0:1))return false;
        marker(proof(L"mixed-target-scope").c_str());
    }
    wchar_t loss_flag[2]{};
    if(lan.host&&environment_title(L"QD_WORLD_JOIN_DROP_DOOR_ACK",loss_flag,2)&&
       status.find("\"door_triggers_received\":1,")!=status.npos)return false;
    marker(proof(L"world-applied").c_str());
    return lan.host&&marker_exists((directory()+L"/client-world-applied.txt").c_str());
}
bool check_special(Address instance,std::string_view status){
    struct Spec {const wchar_t* mode;const char* path;const char* action;const char* state;};
    constexpr std::array specs{
        Spec{L"secret","/Game/Blueprints/SecretWall.SecretWall_C","ForceOpen","Opened"},
        Spec{L"dagger","/Game/RuinsLevel/Models/DaggerButton/DaggerButton_DamageTarget.DaggerButton_DamageTarget_C","Hit","AlreadyHit"},
        Spec{L"gargoyle","/Game/Enemies/Gargler_00/Gargoyle_Switch.Gargoyle_Switch_C","Activate","Lit"}
    };
    static UE::WeakKey object;
    static ULONGLONG ready_at{},both_ready_at{},activated_at{};
    static bool fired{},recorded{},motion{};
    static std::array<float,3> starting_position{};
    wchar_t selected[16]{};
    if(!environment_title(L"QD_WORLD_JOIN_SPECIAL",selected,std::size(selected)))return false;
    const auto spec=std::ranges::find_if(specs,[&](const auto& value){return std::wcscmp(selected,value.mode)==0;});
    const auto player=pawn(instance);
    if(spec==specs.end()||!player||(!lan.host&&status.find("\"world_arrival_applied\":0")!=status.npos))return false;
    wchar_t late_flag[2]{},host_flag[2]{};
    const bool late=environment_title(L"QD_WORLD_JOIN_SPECIAL_LATE",late_flag,2);
    const bool host_origin=environment_title(L"QD_WORLD_JOIN_SPECIAL_HOST",host_flag,2);
    if(late&&!lan.host&&!marker_exists((directory()+L"/host-special-active.txt").c_str()))return false;
    if(!begin_spawn||!finish_spawn||!actor_location||!get_all_actors)scan();
    const auto stock_class=ui.load_asset_path(spec->path);
    if(!stock_class)return false;
    if(!ui.get(object))ui.remember(object,spawn(player,stock_class,0,false));
    const auto actor=ui.get(object),flag=ui.member(actor,spec->state,"BoolProperty",1);
    if(!actor||!flag)return false;
    const auto secret_position=[&](std::array<float,3>& value){
        const auto mesh=ui.object(actor,"Mesh_Internal"),position=ui.member(mesh,"RelativeLocation","StructProperty",12);
        if(!position)return false;
        value=at<std::array<float,3>>(mesh,at<unsigned>(position,76));
        return true;
    };
    if(!ready_at){
        const auto pool=ui.load_asset_path("/Game/Blueprints/ActivationTrigger.ActivationTrigger_C");
        if(!pool||!set_fixture_array(player,pool,actor,"GUIDs",keys[2].data(),sizeof(keys[2])))return false;
        const auto boolean=[&](const char* name,bool value){
            const auto prop=ui.member(actor,name,"BoolProperty",1);
            if(!prop)return false;
            UE::set_bool(reinterpret_cast<void*>(prop),reinterpret_cast<void*>(actor),value,0);
            return true;
        };
        if(!boolean(spec->state,false))return false;
        if(spec==specs.begin()&&(!boolean("Regular Interaction Mode",true)||!boolean("OneSided",false)))return false;
        if(host_origin&&spec==specs.begin()&&!secret_position(starting_position))return false;
        if(spec==specs.begin()+1&&!boolean("OnceEver",true))return false;
        ready_at=GetTickCount64();
        marker(proof(L"special-prepared").c_str());
        return false;
    }
    if(lan.host==(late||host_origin)&&!fired){
        if(!late&&!marker_exists((directory()+(lan.host?L"/client-special-prepared.txt":L"/host-special-prepared.txt")).c_str()))return false;
        const auto now=GetTickCount64();
        if(!both_ready_at)both_ready_at=now;
        if(now-(late?ready_at:both_ready_at)<2200)return false;
        const auto action=method(actor,spec->action,0);
        if(!action)return false;
        TitleUi::event(actor,action,nullptr);
        fired=true;
        marker(proof(L"special-fired").c_str());
    }
    const bool active=UE::get_bool(reinterpret_cast<void*>(flag),reinterpret_cast<void*>(actor),0);
    if(spec==specs.begin()+1&&active&&!recorded){
        const auto mesh=ui.object(actor,"StaticMesh"),position=ui.member(mesh,"RelativeLocation","StructProperty",12);
        const auto end=ui.member(actor,"ButtonPushinPos","FloatProperty",4);
        if(auto out=_wfopen(proof(L"dagger-position").c_str(),L"wb")){
            std::fprintf(out,"mesh=%u relative_location=%u endpoint=%u x=%.5f end=%.5f\n",mesh!=0,position!=0,end!=0,
                position?at<float>(mesh,at<unsigned>(position,76)):0.f,end?at<float>(actor,at<unsigned>(end,76)):0.f);
            std::fclose(out);
        }
        recorded=activated_at&&GetTickCount64()-activated_at>=3000;
    }
    if(!active||(!late&&!host_origin&&lan.host&&status.find("\"world_interactions_applied\":0")!=status.npos))return false;
    if(host_origin&&spec==specs.begin()){
        std::array<float,3> position{};
        if(!secret_position(position))return false;
        const auto running=ui.member(actor,"AnimRunning","BoolProperty",1);
        if(!running)return false;
        const bool animating=UE::get_bool(reinterpret_cast<void*>(running),reinterpret_cast<void*>(actor),0);
        motion|=animating&&position!=starting_position;
        if(auto out=_wfopen(proof(L"secret-position").c_str(),L"wb")){
            std::fprintf(out,"motion=%u running=%u start=%.4f,%.4f,%.4f current=%.4f,%.4f,%.4f\n",motion,animating,
                starting_position[0],starting_position[1],starting_position[2],position[0],position[1],position[2]);
            std::fclose(out);
        }
        if(!motion)return false;
    }
    if(!activated_at)activated_at=GetTickCount64();
    if(GetTickCount64()-activated_at<3000)return false;
    if(late&&lan.host)marker(proof(L"special-active").c_str());
    if(spec==specs.begin()){
        const auto running=ui.member(actor,"AnimRunning","BoolProperty",1);
        if(!running||UE::get_bool(reinterpret_cast<void*>(running),reinterpret_cast<void*>(actor),0))return false;
    }else if(spec==specs.begin()+1){
        const auto mesh=ui.object(actor,"StaticMesh"),position=ui.member(mesh,"RelativeLocation","StructProperty",12);
        const auto end=ui.member(actor,"ButtonPushinPos","FloatProperty",4);
        if(!position||!end||
           std::abs(at<float>(mesh,at<unsigned>(position,76))-at<float>(actor,at<unsigned>(end,76)))>.1f)return false;
    }else{
        const auto light=ui.object(actor,"PointLight"),intensity=ui.member(light,"Intensity","FloatProperty",4);
        if(!intensity||std::abs(at<float>(light,at<unsigned>(intensity,76))-2.f)>.1f)return false;
    }
    marker(proof(L"world-applied").c_str());
    return lan.host&&marker_exists((directory()+L"/client-world-applied.txt").c_str());
}
UE::WeakKey enemy_lease_actor;
std::array<bool,4> enemy_lease_original{};
bool enemy_lease_original_known{};
bool enemy_rehost_enabled(){
    wchar_t enabled[2]{};
    return environment_title(L"QD_WORLD_JOIN_ENEMY_REHOST",enabled,2);
}
bool enemy_lease_state(Address actor,std::array<bool,4>& flags){
    const auto hidden=ui.member(actor,"bHidden","BoolProperty",1),ai=ui.member(actor,"bIsAIDisabled","BoolProperty",1);
    const auto tick=method(actor,"IsActorTickEnabled",1);
    if(!hidden||!ai||!collision||!tick)return false;
    const auto flag=[&](Address property){return UE::get_bool(reinterpret_cast<void*>(property),reinterpret_cast<void*>(actor),0);};
    unsigned char enabled{},ticking{};
    TitleUi::event(actor,collision,&enabled);
    TitleUi::event(actor,tick,&ticking);
    flags={flag(hidden),flag(ai),enabled!=0,ticking!=0};
    return true;
}
bool enemy_lease_restored(){
    std::array<bool,4> current{};
    return enemy_lease_original_known&&enemy_lease_state(ui.get(enemy_lease_actor),current)&&current==enemy_lease_original;
}
bool check_enemy_lease(Address instance,std::string_view status){
    static ULONGLONG reenabled_at{};
    static bool checked{};
    static UE::WeakKey original_controller,new_controller;
    if(lan.host)return marker_exists((directory()+L"/client-world-applied.txt").c_str());
    if(checked)return false;
    wchar_t rebind_flag[2]{};
    const bool rebind=environment_title(L"QD_WORLD_JOIN_ENEMY_REBIND",rebind_flag,2);
    const auto controller_of=[&](Address actor){
        const auto get=method(actor,"GetController",8);
        Address controller{};
        if(get)TitleUi::event(actor,get,&controller);
        return controller;
    };
    const auto player=pawn(instance);
    if(!player||status.find("\"world_arrival_applied\":0")!=status.npos)return false;
    if(!begin_spawn||!finish_spawn||!collision)scan();
    if(!ui.get(enemy_lease_actor)){
        constexpr auto path="/Game/Enemies/Lizer_00/Default_Lizer_01.Default_Lizer_01_C";
        const auto type=ui.load_asset_path(path);
        const auto actor=type?spawn(player,type,0,false):0;
        if(actor){
            ui.remember(enemy_lease_actor,actor);
            enemy_lease_original_known=enemy_lease_state(actor,enemy_lease_original);
            if(rebind){
                const auto spawn_controller=method(actor,"SpawnDefaultController",0);
                if(spawn_controller&&!controller_of(actor))TitleUi::event(actor,spawn_controller,nullptr);
                ui.remember(original_controller,controller_of(actor));
            }
        }
        return false;
    }
    const auto actor=ui.get(enemy_lease_actor);
    const auto ai=ui.member(actor,"bIsAIDisabled","BoolProperty",1);
    const auto set_hidden=method(actor,"SetActorHiddenInGame",1),set_collision=method(actor,"SetActorEnableCollision",1);
    const auto set_tick=method(actor,"SetActorTickEnabled",1);
    std::array<bool,4> state{};
    if(!ai||!set_hidden||!set_collision||!set_tick||!enemy_lease_state(actor,state)||
       state!=std::array{true,true,false,false})return false;
    const auto now=GetTickCount64();
    if(!reenabled_at){
        if(rebind){
            const auto previous=ui.get(original_controller),unpossess=method(previous,"UnPossess",0);
            const auto spawn_controller=method(actor,"SpawnDefaultController",0);
            if(!unpossess||!spawn_controller||controller_of(actor)!=previous)return false;
            TitleUi::event(previous,unpossess,nullptr);
            TitleUi::event(actor,spawn_controller,nullptr);
            const auto current=controller_of(actor);
            if(!current||current==previous)return false;
            ui.remember(new_controller,current);
            marker(proof(L"enemy-controller-replaced").c_str());
        }
        unsigned char off{},on=1;
        TitleUi::event(actor,set_hidden,&off);
        TitleUi::event(actor,set_collision,&on);
        TitleUi::event(actor,set_tick,&on);
        UE::set_bool(reinterpret_cast<void*>(ai),reinterpret_cast<void*>(actor),false,0);
        if(!enemy_lease_state(actor,state)||state!=std::array{false,false,true,true})return false;
        reenabled_at=now;
        marker(proof(L"enemy-reenabled").c_str());
        return false;
    }
    if(now-reenabled_at>1200||status.find("\"stock_enemies_suppressed\":0")!=status.npos)return false;
    if(rebind){
        const auto current=ui.get(new_controller),controller_tick=method(current,"IsActorTickEnabled",1);
        if(!current||!controller_tick||controller_of(actor)!=current)return false;
        unsigned char controller_enabled{};
        TitleUi::event(current,controller_tick,&controller_enabled);
        if(controller_enabled)return false;
        const auto brain=ui.object(current,"BrainComponent");
        const auto is_running=method(brain,"IsRunning",1),is_ticking=method(brain,"IsComponentTickEnabled",1);
        unsigned char brain_running{},brain_ticking{};
        if(is_running)TitleUi::event(brain,is_running,&brain_running);
        if(is_ticking)TitleUi::event(brain,is_ticking,&brain_ticking);
        if(auto file=_wfopen(proof(L"enemy-controller-suppressed").c_str(),L"wb")){
            std::fprintf(file,"controller_tick=0 brain=%u running=%u tick=%u\n",brain!=0,brain_running,brain_ticking);
            std::fclose(file);
        }
    }
    if(auto file=_wfopen(proof(L"enemy-resuppressed").c_str(),L"wb")){
        std::fprintf(file,"hidden=1 ai_disabled=1 collision=0 tick=0 delay_ms=%llu\n",now-reenabled_at);
        std::fclose(file);
    }
    checked=true;
    marker(proof(L"world-applied").c_str());
    return false;
}
using HeartbeatExec=void(*)(void*,void*,void*);
HeartbeatExec heartbeat_next{};
void* volatile* heartbeat_cell{};
UE::WeakKey heartbeat_listener;
DWORD heartbeat_thread{};
bool heartbeat_attached{};
void watch_heartbeat(void* context,void* frame,void* result);
bool restore_heartbeat(){
    if(!heartbeat_cell)return true;
    if(InterlockedCompareExchangePointer(heartbeat_cell,reinterpret_cast<void*>(heartbeat_next),
        reinterpret_cast<void*>(&watch_heartbeat))!=reinterpret_cast<void*>(&watch_heartbeat))return false;
    heartbeat_cell=nullptr;
    return true;
}

void watch_heartbeat(void* context,void* frame,void* result){
    heartbeat_next(context,frame,result);
    if(!frame||GetCurrentThreadId()!=heartbeat_thread||UE::field<Address>(frame,32))return;
    const auto args=UE::field<Address>(frame,40);
    const auto cue=args?at<Address>(args,0):0;
    constexpr auto path="/Game/Player/StatusEffectVisualizers/Weaken/Heartbeat_Cue.Heartbeat_Cue";
    if(!cue||!path_is(cue,path))return;
    const auto root=at<Address>(args,8),local_root=ui.object(ui.get(heartbeat_listener),"RootComponent");
    const auto attenuation=at<Address>(args,64);
    if(!root||!local_root||root==local_root||!attenuation||at<float>(args,52)!=.6f)return;
    const auto actor=at<Address>(root,32);
    if(!actor||ui.object(actor,"RootComponent")!=root||actor==ui.get(heartbeat_listener))return;
    std::string name;
    if(!UE::object_path(attenuation,image+TitleUi::name_pool,read_local,name))return;
    heartbeat_attached=true;
    if(auto file=_wfopen(proof(L"heartbeat-spatial").c_str(),L"wb")){
        std::fprintf(file,"remote_root=1 attenuation=%s volume=%.3f component=%u\n",name.c_str(),
            at<float>(args,52),result&&UE::field<Address>(result,0)!=0);
        std::fclose(file);
    }
}
bool check_heartbeat(Address instance,std::string_view status){
    const auto player=pawn(instance);
    if(!player||(!lan.host&&status.find("\"world_arrival_applied\":0")!=status.npos))return false;
    if(lan.host){
        if(heartbeat_attached)return restore_heartbeat();
        if(heartbeat_cell)return false;
        const auto attach=method(ui.get(TitleUi::gameplay),"SpawnSoundAttached",96);
        if(!ui.method(attach,96,{{"Sound","ObjectProperty",0,8},{"AttachToComponent","ObjectProperty",8,8},
            {"AttachPointName","NameProperty",16,8},{"Location","StructProperty",24,12},{"Rotation","StructProperty",36,12},
            {"LocationType","ByteProperty",48,1},{"bStopWhenAttachedToDestroyed","BoolProperty",49,1},
            {"VolumeMultiplier","FloatProperty",52,4},{"AttenuationSettings","ObjectProperty",64,8},
            {"PitchMultiplier","FloatProperty",56,4},{"StartTime","FloatProperty",60,4},{"ConcurrencySettings","ObjectProperty",72,8},
            {"bAutoDestroy","BoolProperty",80,1},{"ReturnValue","ObjectProperty",88,8}}))return false;
        auto cell=reinterpret_cast<void* volatile*>(attach+216);
        const auto next=*cell;
        MEMORY_BASIC_INFORMATION memory{};
        if(!next||!VirtualQuery(next,&memory,sizeof(memory))||memory.AllocationBase!=GetModuleHandleW(L"QDCoopNativeM1.dll"))return false;
        ui.remember(heartbeat_listener,player);
        heartbeat_next=reinterpret_cast<HeartbeatExec>(next);
        heartbeat_thread=GetCurrentThreadId();
        if(InterlockedCompareExchangePointer(cell,reinterpret_cast<void*>(&watch_heartbeat),next)!=next)return false;
        heartbeat_cell=cell;
        marker(proof(L"heartbeat-listening").c_str());
        return false;
    }
    static bool sent{};
    if(!sent&&marker_exists((directory()+L"/host-heartbeat-listening.txt").c_str())){
        const auto pulse=method(player,"LowHealthHeartbeat",0);
        if(!pulse)return false;
        TitleUi::event(player,pulse,nullptr);
        sent=true;
        marker(proof(L"heartbeat-stock-pulse").c_str());
    }
    return false;
}
bool check(Address instance){
    const auto test=mode();
    if(test==Mode::wipe)return check_wipe(instance);
    if(lan.host&&(test==Mode::host_saved||test==Mode::guest_saved))
        return joining&&joined&&marker_exists((directory()+L"/client-world-applied.txt").c_str());
    if(!status_read)if(const auto core=GetModuleHandleW(L"QDCoopNativeM1.dll"))load_export(core,"QDEnemyNativeStatus",status_read);
    if(!status_read)return false;
    std::array<char,32768> report{};
    if(!status_read(report.data(),report.size()))return false;
    const std::string_view status=report.data();
    if(auto out=_wfopen(proof(L"world-progress").c_str(),L"wb")){
        std::fprintf(out,"spawned=%u joining=%u joined=%u alive=%u/%u/%u\n%s\n",spawned,joining,joined,
            ui.get(fixtures[0])!=0,ui.get(fixtures[1])!=0,ui.get(fixtures[2])!=0,report.data());
        std::fclose(out);
    }
    if(!lan.host&&status.find("\"world_progress_applied\":0")!=status.npos)return false;
    if(test==Mode::duck||test==Mode::duck_pickup)return check_duck(instance,test==Mode::duck_pickup);
    if(test==Mode::door_trigger)return check_door_trigger(instance,status);
    if(test==Mode::special)return check_special(instance,status);
    if(test==Mode::enemy_lease)return check_enemy_lease(instance,status);
    if(test==Mode::heartbeat)return check_heartbeat(instance,status);
    if(test==Mode::rest){
        const auto player=pawn(instance);
        const auto rest=ui.member(player,"Resting","BoolProperty",1);
        if(!lan.host&&rest&&!marker_exists(proof(L"rest-fired").c_str())){
            UE::set_bool(reinterpret_cast<void*>(rest),reinterpret_cast<void*>(player),true,0);
            marker(proof(L"rest-fired").c_str());
        }
        if(lan.host&&status.find("\"enemy_rest_requests\":0")==status.npos&&
           status.find("\"enemy_rest_refreshes\":0")==status.npos&&status.find("\"enemy_rest_spawners\":0")==status.npos){
            marker(proof(L"world-applied").c_str());
            return true;
        }
        return false;
    }
    if(test==Mode::guest_saved){
        if(!seeded||!joining||!joined||status.find("\"persistent_guest_changes\":0")!=status.npos)return false;
        const auto save=ui.object(instance,"ActiveSaveSlot"),map=ui.member(save,"SavableActorStates","MapProperty",80);
        if(!map)return false;
        const auto find=reinterpret_cast<bool(*)(void*,const void*,const void*,void*)>(image+0x2756200);
        const auto address=reinterpret_cast<void*>(save+at<unsigned>(map,76));
        for(const auto& key:keys){
            int value{};
            if(find(address,reinterpret_cast<void*>(map),key.data(),&value)&&value>0)return false;
        }
        marker(proof(L"world-applied").c_str());
        return true;
    }
    if(!spawned){
        if(!begin_spawn||!finish_spawn||!actor_location||!collision)scan();
        const auto player=pawn(instance);
        if(!player)return false;
        for(unsigned i=0;i<(test==Mode::guest_breaks?1u:paths.size());++i)if(!ui.get(fixtures[i])){
            const auto type=ui.load_asset_path(paths[i]);
            const auto actor=type?spawn(player,type,i):0;
            if(!actor)return false;
            ui.remember(fixtures[i],actor);
        }
        spawned=true;
        if(test==Mode::guest_breaks)marker(proof(L"live-spawned").c_str());
        return false;
    }
    if(test==Mode::guest_breaks){
        const auto remote=directory()+L"/"+(lan.host?L"client":L"host")+L"-live-spawned.txt";
        if(!marker_exists(remote.c_str()))return false;
        const auto actor=ui.get(fixtures[0]);
        static ULONGLONG since{};
        if(!since)since=GetTickCount64();
        if(GetTickCount64()-since<1800)return false;
        if(!lan.host&&actor&&!marker_exists(proof(L"live-hit").c_str())){
            const auto hp=ui.member(actor,"HP","FloatProperty",4);
            const auto took_hit=method(actor,"TookHit",0);
            if(!hp||!took_hit)return false;
            const float zero{};
            std::memcpy(reinterpret_cast<void*>(actor+at<unsigned>(hp,76)),&zero,sizeof(zero));
            TitleUi::event(actor,took_hit,nullptr);
            marker(proof(L"live-hit").c_str());
        }
        unsigned char enabled=1;
        if(actor)TitleUi::event(actor,collision,&enabled);
        if((actor&&enabled)||!joining||!joined)return false;
        marker(proof(L"world-applied").c_str());
        return lan.host&&marker_exists((directory()+L"/client-world-applied.txt").c_str());
    }
    const auto prop=ui.get(fixtures[1]),door=ui.get(fixtures[2]);
    const auto field=ui.member(door,"Opened","BoolProperty",1);
    unsigned char enabled=1;
    if(prop&&collision)TitleUi::event(prop,collision,&enabled);
    const bool open=field&&UE::get_bool(reinterpret_cast<void*>(field),reinterpret_cast<void*>(door),0);
    if(ui.get(fixtures[0])||enabled||!open||!joining||!joined)return false;
    marker(proof(L"world-applied").c_str());
    return true;
}
}
