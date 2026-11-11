namespace CombatRewardFixture {
namespace E=qdcoop::enemy;
constexpr std::string_view damage_path="/Game/Enemies/EnemyBase.EnemyBase_C.TryApplyDamage";
constexpr std::string_view proxy_path="/Game/QDCoop/QDCoopGuestTarget.QDCoopGuestTarget_C";
constexpr std::string_view kill_path="/Game/Enemies/EnemyBase.EnemyBase_C.Kill";
constexpr std::string_view item_path="/Game/Items/Treasure/Coins/Coin.Coin";
constexpr std::string_view soft_path="/Script/Engine.KismetSystemLibrary.Conv_ObjectToSoftObjectReference";
constexpr std::string_view system_path="/Script/Engine.Default__KismetSystemLibrary";
unsigned (*status)(char*,unsigned){};
int (*inspect)(std::uint32_t,E::NativeBindingView*){};
Address damage{},proxy{},kill{},item{},soft{},system{},drop{};
Address begin_spawn{},finish_spawn{},actor_location{};
std::wstring directory;
unsigned target_id{},initial_xp{},phase{};
unsigned candidate_count{},alive_count{},data_count{},configured_count{},override_width{},toggle_width{},soft_width{};
ULONGLONG next{};

std::wstring proof(std::wstring_view name){return directory+L"/"+std::wstring(name);}
bool exists(std::wstring_view name){return marker_exists(proof(name).c_str());}
std::string core_status(){
    std::array<char,32768> data{};
    return status&&status(data.data(),data.size())?data.data():std::string{};
}
unsigned count(const std::string& data,const char* name){
    const auto key=std::string("\"")+name+"\":";
    const auto at=data.find(key);
    return at==std::string::npos?0:static_cast<unsigned>(std::strtoul(data.c_str()+at+key.size(),nullptr,10));
}
Address field(Address object,std::string_view name,unsigned size){
    const auto property=UE::find_inherited_property(at<Address>(object,16),image+TitleUi::name_pool,name,read_local);
    return property&&at<unsigned>(property,60)==size?property:0;
}
template<class Value>void write(Address object,Address property,const Value& value){
    std::memcpy(reinterpret_cast<void*>(object+at<unsigned>(property,76)),&value,sizeof(value));
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
        if(!damage&&name=="TryApplyDamage"&&path_is(entry.object,damage_path))damage=entry.object;
        if(!kill&&name=="Kill"&&path_is(entry.object,kill_path))kill=entry.object;
        if(!soft&&name=="Conv_ObjectToSoftObjectReference"&&path_is(entry.object,soft_path))soft=entry.object;
        if(!system&&name=="Default__KismetSystemLibrary"&&path_is(entry.object,system_path))system=entry.object;
        if(!item&&name=="Coin"&&path_is(entry.object,item_path))item=entry.object;
        if(!drop&&name=="TrySpawningItem")drop=entry.object;
        if(!begin_spawn&&name=="BeginDeferredActorSpawnFromClass"&&
           path_is(entry.object,"/Script/Engine.GameplayStatics.BeginDeferredActorSpawnFromClass"))begin_spawn=entry.object;
        if(!finish_spawn&&name=="FinishSpawningActor"&&
           path_is(entry.object,"/Script/Engine.GameplayStatics.FinishSpawningActor"))finish_spawn=entry.object;
        if(!actor_location&&name=="K2_GetActorLocation"&&
           path_is(entry.object,"/Script/Engine.Actor.K2_GetActorLocation"))actor_location=entry.object;
        if(lan.host&&!proxy&&!(at<unsigned>(entry.object,8)&0x10u)&&
           path_is(at<Address>(entry.object,16),proxy_path))proxy=entry.object;
    }
}
std::string stock_path(std::string_view package){
    return "/Game/Enemies/"+std::string(package)+"."+std::string(package.substr(package.rfind('/')+1))+"_C";
}
constexpr std::array<std::string_view,12> variants{
    "FishManer_00/Fishmaner_Boss_00_BP","Ribber_00/Ribber_Boss_00_BP",
    "Lizer_00/Default_Lizer_01","Lizer_00/Default_Lizer_02","Lizer_00/Default_Lizer_03",
    "Lizer_00/Default_Lizer_04","Lizer_00/Default_Lizer_05","Lizer_00/Default_Lizer_06","Lizer_00/Default_Lizer_07",
    "Lizer_00/Bomb/BombLizard_BP","Lizer_00/AggroLizard_Iron_BP","Lizer_00/AggroLizard_Berserk_BP"
};
constexpr std::array<std::string_view,2> boss_items{
    "/Game/Items/Treasure/Fishman_Boss_Crown/Fishman_Boss_Crown_Silver.Fishman_Boss_Crown_Silver",
    "/Game/Items/Treasure/RibberBoss_Tiara/RibberBoss_Silver_Tiara.RibberBoss_Silver_Tiara"
};
constexpr std::array<std::string_view,2> boss_pickups{
    "Fishman_Boss_Crown_Silver_Pickup_C","RibberBoss_Tiara_Silver_Pickup_C"
};
Address spawn_variant(Address pawn,Address type){
    if(!type||!actor_location||!begin_spawn||!finish_spawn||
       at<std::uint16_t>(begin_spawn,182)!=88||at<std::uint16_t>(finish_spawn,182)!=72)return 0;
    std::array<float,3> point{};
    TitleUi::event(pawn,actor_location,point.data());
    const std::array<float,12> transform{0,0,0,1,point[0]+130.f,point[1],point[2]+25.f,0,1,1,1,0};
    alignas(16) std::array<unsigned char,96> args{},finish{};
    std::memcpy(args.data(),&pawn,8);
    std::memcpy(args.data()+8,&type,8);
    std::memcpy(args.data()+16,transform.data(),48);
    args[64]=1;
    const auto gameplay=ui.get(TitleUi::gameplay);
    TitleUi::event(gameplay,begin_spawn,args.data());
    const auto actor=UE::field<Address>(args.data(),80);
    if(!actor)return 0;
    std::memcpy(finish.data(),&actor,8);
    std::memcpy(finish.data()+16,transform.data(),48);
    TitleUi::event(gameplay,finish_spawn,finish.data());
    return UE::field<Address>(finish.data(),64)==actor?actor:0;
}
Address active_pawn(Address instance){
    Address args[]{instance,0,0};
    TitleUi::event(ui.get(TitleUi::gameplay),ui.get(TitleUi::local_pawn),args);
    return args[2];
}
Address actor_method(Address actor,std::string_view name){
    for(auto type=at<Address>(actor,16);type;type=at<Address>(type,64))
        for(auto child=at<Address>(type,72);child;child=at<Address>(child,40))
            if(at<Address>(child,32)==type&&leaf(child)==name)return child;
    return 0;
}
Address find_pickup(std::string_view type){
    const auto array=at<UE::ObjectArray>(image+TitleUi::object_array,0);
    if(!UE::object_array_shape(array))return 0;
    for(int i=0;i<array.num_elements;++i){
        const auto chunk=at<Address>(array.chunks,(i/UE::objects_per_chunk)*8);
        if(!chunk)continue;
        const auto entry=at<UE::ObjectItem>(chunk,(i%UE::objects_per_chunk)*sizeof(UE::ObjectItem));
        if(entry.object&&!(entry.flags&0x30000000u)&&!(at<unsigned>(entry.object,8)&0x10u)&&
           leaf(at<Address>(entry.object,16))==type)return entry.object;
    }
    return 0;
}
bool check_boss_pickup(unsigned index,Address instance,Address pawn){
    const auto actor=find_pickup(boss_pickups[index]);
    const auto interact=actor?actor_method(actor,"Interact"):0;
    const auto asset=ui.load_asset_path(boss_items[index]),save=ui.object(instance,"ActiveSaveSlot");
    const auto map=field(save,"InventoryContents",80);
    if(!map||!asset||!interact||at<std::uint16_t>(interact,182)!=8||
       !parameter(interact,"PlayerInteracting",0,8))return false;
    const auto shape=at<std::array<int,6>>(map,0x88);
    const auto address=save+at<unsigned>(map,76);
    int before{},after{};
    if(!UE::object_count(address,asset,shape,read_local,before))return false;
    TitleUi::event(actor,interact,&pawn);
    return UE::object_count(address,asset,shape,read_local,after)&&after==before+1;
}
bool variants_step(Address instance,ULONGLONG now){
    static unsigned index{},before_items{},before_rolls{},enemy_id{};
    static ULONGLONG fired{},spawned{};
    static UE::WeakKey enemy;
    static bool load_done{},picked{};
    const auto peer=lan.host?L"client":L"host";
    if(index==variants.size())return exists(L"host-variants-passed.txt")&&exists(L"client-variants-passed.txt");
    const auto json=core_status();
    const auto pawn=active_pawn(instance);
    if(!pawn)return false;
    const auto stem=std::to_wstring(index);
    const auto ready=proof(lan.role+L"-variant-"+stem+L"-ready.txt");
    const auto peer_ready=std::wstring(peer)+L"-variant-"+stem+L"-ready.txt";
    const auto done=proof(lan.role+L"-variant-"+stem+L"-done.txt");
    const auto peer_done=std::wstring(peer)+L"-variant-"+stem+L"-done.txt";
    const auto roll_count=count(json,lan.host?"combat_drop_offers":"combat_drop_rolls");
    const auto items=count(json,"combat_drop_items");
    const auto path=stock_path(variants[index]);
    if(!load_done){
        const auto type=ui.load_asset_path(path);
        if(!type)return false;
        if(lan.host){
            const auto actor=spawn_variant(pawn,type);
            if(!actor)return false;
            ui.remember(enemy,actor);
        }
        before_rolls=roll_count;
        before_items=items;
        load_done=true;
        spawned=now;
        marker(ready.c_str());
    }
    if(!exists(peer_ready))return false;
    if(lan.host&&!fired){
        const auto actor=ui.get(enemy);
        if(!actor||now-spawned<500)return false;
        for(unsigned id=1;id<=E::capacity;++id){
            E::NativeBindingView view{};
            if(inspect(id,&view)&&view.confirmed&&ui.get(view.actor)==actor){enemy_id=id;break;}
        }
        const auto target=field(actor,"TargetPlayer",8);
        if(!target||!ui.object(actor,"Data"))return false;
        write(actor,target,proxy);
        const auto method=actor_method(actor,"Kill");
        if(!method||at<std::uint16_t>(method,182)!=2)return false;
        unsigned char args[2]{0,1};
        TitleUi::event(actor,method,args);
        fired=now;
    }else if(!lan.host&&!fired&&roll_count==before_rolls+1){
        fired=now;
    }
    if(auto out=_wfopen(proof(lan.role+L"-variants-progress.txt").c_str(),L"wb")){
        std::fprintf(out,"case=%u %s loaded=%u fired=%u id=%u rolls=%u/%u items=%u/%u picked=%u\n%s\n",
            index,path.c_str(),load_done,fired!=0,enemy_id,before_rolls,roll_count,before_items,items,picked,json.c_str());
        std::fclose(out);
    }
    if(!fired||roll_count!=before_rolls+1||count(json,"combat_reward_rejected"))return false;
    if(!lan.host&&index<2&&!picked&&items==before_items+1)picked=check_boss_pickup(index,instance,pawn);
    if(!lan.host&&((index<2&&!picked)||(index>=2&&index<=8&&items!=before_items+1)))return false;
    if(now-fired<1850)return false;
    if(auto out=_wfopen(done.c_str(),L"wb")){
        std::fprintf(out,"%s one_roll=%u new_items=%u picked=%u enrolled=%u\n",path.c_str(),roll_count-before_rolls,
            items-before_items,picked,enemy_id);
        std::fclose(out);
    }
    if(!exists(peer_done))return false;
    ++index;
    load_done=picked=false;
    enemy_id=0;
    fired=0;
    enemy={};
    if(index==variants.size())marker(proof(lan.role+L"-variants-passed.txt").c_str());
    return false;
}
unsigned xp(Address instance){
    const auto save=ui.object(instance,"ActiveSaveSlot"),prop=field(save,"XPAmount",4);
    return prop?at<unsigned>(save,at<unsigned>(prop,76)):0;
}
bool configure_drop(Address actor){
    const auto actor_type=at<Address>(actor,16);
    const auto toggle=UE::find_inherited_property(actor_type,image+TitleUi::name_pool,"OverrideItemDrop",read_local);
    const auto entry=UE::find_inherited_property(actor_type,image+TitleUi::name_pool,"ItemDropOverride",read_local);
    override_width=entry?at<unsigned>(entry,60):0;
    toggle_width=toggle?at<unsigned>(toggle,60):0;
    soft_width=soft?at<std::uint16_t>(soft,182):0;
    if(toggle_width!=1||override_width!=112||!system||!item||soft_width!=48)return false;
    alignas(16) std::array<unsigned char,48> reference{};
    std::memcpy(reference.data(),&item,8);
    TitleUi::event(system,soft,reference.data());
    alignas(16) std::array<unsigned char,112> args{};
    const float guaranteed=1.f;
    const int coins=3;
    std::memcpy(args.data(),&guaranteed,4);
    std::memcpy(args.data()+8,reference.data()+8,40);
    std::memcpy(args.data()+48,&coins,4);
    write(actor,entry,args);
    UE::set_bool(reinterpret_cast<void*>(toggle),reinterpret_cast<void*>(actor),true,0);
    return true;
}
bool attack(Address pawn,Address actor){
    if(at<std::uint16_t>(damage,182)!=590)return false;
    const auto hit=UE::find_property(damage,image+TitleUi::name_pool,"Hit",read_local);
    const auto input=UE::find_property(damage,image+TitleUi::name_pool,"DamageToApply",read_local);
    const auto applied=UE::find_property(damage,image+TitleUi::name_pool,"AppliedDamage",read_local);
    if(!hit||!input||!applied||at<int>(hit,76)!=0||at<int>(input,76)!=136||at<int>(applied,76)!=360)return false;
    const auto init=reinterpret_cast<void(*)(Address,void*)>(image+0x11914f0);
    const auto destroy=reinterpret_cast<void(*)(Address,void*)>(image+0x118e4c0);
    alignas(16) std::array<unsigned char,592> args{};
    init(hit,args.data());
    init(input,args.data()+136);
    init(applied,args.data()+360);
    const float amount=45.f;
    std::memcpy(args.data()+136,&amount,4);
    std::memcpy(args.data()+352,&pawn,8);
    TitleUi::event(actor,damage,args.data());
    destroy(applied,args.data()+360);
    destroy(input,args.data()+136);
    destroy(hit,args.data());
    return true;
}
void report(const std::string& json,unsigned current){
    if(auto file=_wfopen(proof(lan.role+L"-reward-progress.txt").c_str(),L"wb")){
        std::fprintf(file,"phase=%u target=%u xp_before=%u xp_now=%u damage=%u proxy=%u item=%u soft=%u kill=%u drop=%u\n%s\n",
            phase,target_id,initial_xp,current,damage!=0,proxy!=0,item!=0,soft!=0,kill!=0,drop!=0,json.c_str());
        std::fprintf(file,"candidates=%u alive=%u data=%u configured=%u widths=%u/%u/%u\n",
            candidate_count,alive_count,data_count,configured_count,override_width,toggle_width,soft_width);
        if(kill&&drop){
            const auto code=at<const unsigned char*>(kill,96),drop_code=at<const unsigned char*>(drop,96);
            std::fprintf(file,"drop length=%d parms=%u head=%02x GNatives 03/05/08=%llx/%llx/%llx\n",at<int>(drop,104),
                at<std::uint16_t>(drop,182),drop_code[0],at<unsigned long long>(image+0x4a62e10,3*8)-image,
                at<unsigned long long>(image+0x4a62e10,5*8)-image,at<unsigned long long>(image+0x4a62e10,8*8)-image);
            for(unsigned i=1018;i<1101;++i)std::fprintf(file,"%02x ",code[i]);
            std::fprintf(file,"\n");
        }
        std::fclose(file);
    }
}
bool step(Address instance,ULONGLONG now){
    if(now<next)return false;
    next=now+250;
    if(phase==3){
        if(!exists(L"host-reward-passed.txt")||!exists(L"client-reward-passed.txt"))return false;
        wchar_t enabled[2]{};
        return !environment_title(L"QD_REWARD_VARIANTS",enabled,2)||variants_step(instance,now);
    }
    if(directory.empty())directory=std::wstring(lan.listening).substr(0,std::wstring(lan.listening).find_last_of(L"/\\"));
    if(!inspect)if(const auto core=GetModuleHandleW(L"QDCoopNativeM1.dll")){
        load_export(core,"QDEnemyInspectNativeBinding",inspect);
        load_export(core,"QDEnemyNativeStatus",status);
    }
    if(!damage||!kill||!drop||!item||!soft||!system||(lan.host&&!proxy))scan();
    const auto json=core_status();
    const auto current=xp(instance);
    report(json,current);
    if(!inspect||!damage||!kill||!drop||count(json,"combat_reward_bind_stage")!=(lan.host?3:2))return false;
    Address pawn_args[]{instance,0,0};
    TitleUi::event(ui.get(TitleUi::gameplay),ui.get(TitleUi::local_pawn),pawn_args);
    const auto pawn=pawn_args[2];
    if(!pawn)return false;
    if(!phase){
        const auto plan=proof(L"reward-target.txt");
        initial_xp=current;
        if(lan.host){
            if(!proxy||!item)return false;
            candidate_count=alive_count=data_count=configured_count=0;
            for(unsigned id=1;id<=E::capacity;++id){
                E::NativeBindingView view{};
                if(!inspect(id,&view)||!view.confirmed)continue;
                ++candidate_count;
                const auto actor=ui.get(view.actor),hp=field(actor,"HP",4),target=field(actor,"TargetPlayer",8);
                if(!actor||!hp||!target||at<float>(actor,at<unsigned>(hp,76))<5.f)continue;
                ++alive_count;
                if(!ui.object(actor,"Data"))continue;
                ++data_count;
                if(!configure_drop(actor))continue;
                ++configured_count;
                write(actor,hp,1.f);
                write(actor,target,proxy);
                target_id=id;
                if(auto out=_wfopen(plan.c_str(),L"wb")){std::fprintf(out,"%u\n",id);std::fclose(out);}
                break;
            }
        }else if(auto in=_wfopen(plan.c_str(),L"rb")){
            const auto parsed=std::fscanf(in,"%u",&target_id);
            std::fclose(in);
            if(parsed!=1)return false;
        }
        if(!target_id)return false;
        phase=1;
    }
    E::NativeBindingView view{};
    if(phase==1&&!lan.host){
        if(!inspect(target_id,&view)||!view.confirmed||!ui.get(view.actor))return false;
        if(!attack(pawn,ui.get(view.actor)))return false;
        phase=2;
    }
    const bool paid=lan.host?count(json,"combat_host_xp")&&count(json,"combat_drop_offers")==1:
        count(json,"combat_guest_xp")&&count(json,"combat_drop_rolls")==1&&count(json,"combat_drop_items")==1;
    if(paid&&current>initial_xp&&!count(json,"combat_reward_rejected")){
        phase=3;
        report(json,current);
        marker(proof(lan.role+L"-reward-passed.txt").c_str());
    }
    return false;
}
}
