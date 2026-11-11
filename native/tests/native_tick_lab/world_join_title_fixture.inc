namespace WorldJoinFixture {
using Guid=std::array<int,4>;
constexpr std::array<Guid,3> keys{{{157137,190137,217137,227137},{157138,190138,217138,227138},{157139,190139,217139,227139}}};
constexpr std::array<std::string_view,3> paths{
    "/Game/Blueprints/BreakableBarricade/BreakableBarricade.BreakableBarricade_C",
    "/Game/Props/Containers/Permabreakable_base.Permabreakable_base_C",
    "/Game/Blueprints/Door_Base.Door_Base_C"
};
bool seeded{},joining{},joined{},left{},failed{},spawned{};
std::array<UE::WeakKey,3> fixtures;
Address begin_spawn{},finish_spawn{},actor_location{},collision{};
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
        if(name=="GetActorEnableCollision"&&path_is(entry.object,"/Script/Engine.Actor.GetActorEnableCollision"))collision=entry.object;
    }
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
Address spawn(Address player,Address type,unsigned index){
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
    if(!prop||UE::field<Address>(done.data(),64)!=actor)return 0;
    struct Array {Guid* data;int count,capacity;};
    const auto guids=at<Array>(actor,at<unsigned>(prop,76));
    if(!guids.data||guids.count<1||guids.capacity<guids.count)return 0;
    guids.data[0]=keys[index];
    return actor;
}
bool check(Address instance){
    if(lan.host)return joining&&joined&&marker_exists((directory()+L"/client-world-applied.txt").c_str());
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
    if(status.find("\"world_progress_applied\":0")!=status.npos)return false;
    if(!spawned){
        if(!begin_spawn||!finish_spawn||!actor_location||!collision)scan();
        const auto player=pawn(instance);
        if(!player)return false;
        for(unsigned i=0;i<paths.size();++i)if(!ui.get(fixtures[i])){
            const auto type=ui.load_asset_path(paths[i]);
            const auto actor=type?spawn(player,type,i):0;
            if(!actor)return false;
            ui.remember(fixtures[i],actor);
        }
        spawned=true;
        return false;
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
