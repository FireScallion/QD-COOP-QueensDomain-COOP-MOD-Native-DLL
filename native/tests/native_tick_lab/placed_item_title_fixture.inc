namespace PlacedItemFixture {
namespace WP=qdcoop::world_progress;
enum Symbol {strings,make_name,map_add,map_find,map_remove,actor_location,actor_collision,component_collision,visible,interact,
             camera_location,forward,player_press,player_release,interaction_update,count};
constexpr std::array<std::string_view,count> paths{
    "/Script/Engine.Default__KismetStringLibrary","/Script/Engine.KismetStringLibrary.Conv_StringToName",
    "/Script/Engine.BlueprintMapLibrary.Map_Add","/Script/Engine.BlueprintMapLibrary.Map_Find",
    "/Script/Engine.BlueprintMapLibrary.Map_Remove","/Script/Engine.Actor.K2_GetActorLocation",
    "/Script/Engine.Actor.GetActorEnableCollision","/Script/Engine.PrimitiveComponent.GetCollisionEnabled",
    "/Script/Engine.SceneComponent.IsVisible",
    "/Game/QDCoop/QDCoopPlacedItem.QDCoopPlacedItem_C.Interact",
    "/Script/Engine.SceneComponent.K2_GetComponentLocation","/Script/Engine.SceneComponent.GetForwardVector",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.InpActEvt_Interact_K2Node_InputActionEvent_25",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.InpActEvt_Interact_K2Node_InputActionEvent_26",
    "/Game/Player/PlayerInteractionComponent.PlayerInteractionComponent_C.Update"
};
constexpr std::string_view placed_type_path="/Game/QDCoop/QDCoopPlacedItem.QDCoopPlacedItem_C";
constexpr std::string_view herb_mesh="/Game/Items/Consumables/Herb/HealingHerbs_00_SM.HealingHerbs_00_SM";
constexpr wchar_t item_path[]=L"/Game/Items/Consumables/Herb/HealingHerb.HealingHerb";
std::array<float,3> expected_position{};
constexpr std::array roles{L"host",L"client",L"client2",L"client3"};
std::array<UE::WeakKey,count> symbols{};
UE::WeakKey placed_type{},visual{},herb{};
UE::WeakKey original_save{};
std::array<UE::NameKey,6> keys{};
UE::NameKey receipt{};
std::wstring root;
unsigned stage{};
ULONGLONG next_scan{};
ULONGLONG next_report{};
ULONGLONG next_click{};
bool seeded{},claimed{},removed_keys{};
bool recovery_mode{};
bool input_mode{};
bool remote_mode{};
bool reopen_mode{};
bool source_mode{};
bool source_ui_mode{};
bool save_retry_mode{},save_lock_started{};
std::array<HANDLE,2> save_locks{};
ULONGLONG save_unlock_at{};
struct InventoryEntry {
    Address item{};
    int count{};
    bool operator==(const InventoryEntry&) const=default;
};
std::vector<InventoryEntry> inventory_before;
std::vector<InventoryEntry> inventory_expected;
unsigned (*read_core)(char*,unsigned){};
int (*read_medic)(qdcoop::rescue::BindingView*){};

Address fn(Symbol index){return ui.get(symbols[index]);}
std::wstring file(std::wstring_view name){return root+L"/"+std::wstring(name);}
bool seen(std::wstring_view name){return marker_exists(file(name).c_str());}
void mark(std::wstring_view stage_name){marker(file(lan.role+L"-"+std::wstring(stage_name)+L".txt").c_str());}
bool all(std::wstring_view stage_name){
    return std::ranges::all_of(roles,[&](const auto role){return seen(std::wstring(role)+L"-"+std::wstring(stage_name)+L".txt");});
}
struct SaveMap {
    Address field{},data{};
};
SaveMap save_map(Address save,const char* name){
    const auto field=ui.member(save,name,"MapProperty",80);
    return {field,field?save+at<unsigned>(field,76):0};
}
std::optional<int> get(const SaveMap& map,const UE::NameKey& key){
    int value{};
    const auto find=reinterpret_cast<bool(*)(Address,Address,const void*,void*)>(image+0x2756200);
    return find(map.data,map.field,&key,&value)?std::optional<int>{value}:std::nullopt;
}
void set(const SaveMap& map,const UE::NameKey& key,std::optional<int> value){
    if(value){
        const auto add=reinterpret_cast<void(*)(Address,Address,const void*,const void*)>(image+0x2755ec0);
        add(map.data,map.field,&key,&*value);
    }else{
        const auto remove=reinterpret_cast<bool(*)(Address,Address,const void*)>(image+0x2756630);
        remove(map.data,map.field,&key);
    }
}
UE::NameKey name(const std::wstring& text){
    if(!fn(strings)||!ui.method(fn(make_name),24,{{"inString","StrProperty",0,16},{"ReturnValue","NameProperty",16,8}}))return {};
    const auto size=static_cast<int>(text.size()+1);
    struct Args {const wchar_t* data;int size,capacity;UE::NameKey result{};};
    Args args{text.c_str(),size,size};
    TitleUi::event(fn(strings),fn(make_name),&args);
    std::string actual;
    return UE::name_string(image+TitleUi::name_pool,args.result,read_local,actual)&&
        std::equal(actual.begin(),actual.end(),text.begin(),text.end())?args.result:UE::NameKey{};
}
bool init_keys(Address instance){
    Address pawn[]{instance,0,0};
    TitleUi::event(ui.get(TitleUi::gameplay),ui.get(TitleUi::local_pawn),pawn);
    const auto level=pawn[2]?at<Address>(pawn[2],32):0;
    const auto world=level?at<Address>(level,32):0;
    std::string map;
    if(!world||!UE::object_path(world,image+TitleUi::name_pool,read_local,map)||!map.starts_with("/Game/"))return false;
    const auto path=file(L"placed-pickup-origin.bin");
    if(lan.host&&!recovery_mode){
        if(!ui.method(fn(actor_location),12,{{"ReturnValue","StructProperty",0,12}}))return false;
        if(input_mode){
            const auto camera=ui.object(pawn[2],"PlayerInteractionComponent");
            if(!camera||!ui.method(fn(camera_location),12,{{"ReturnValue","StructProperty",0,12}})||
               !ui.method(fn(forward),12,{{"ReturnValue","StructProperty",0,12}}))return false;
            std::array<float,3> direction{};
            TitleUi::event(camera,fn(camera_location),expected_position.data());
            TitleUi::event(camera,fn(forward),direction.data());
            for(unsigned i=0;i<direction.size();++i)expected_position[i]+=direction[i]*80.f;
        }else{
            TitleUi::event(pawn[2],fn(actor_location),expected_position.data());
            expected_position[0]+=70.f;
            expected_position[2]-=30.f;
        }
        for(auto& coordinate:expected_position){
            if(!std::isfinite(coordinate)||std::abs(coordinate)>10000000.f)return false;
            coordinate=std::floor(coordinate);
        }
        const auto stream=_wfopen(path.c_str(),L"wb");
        if(!stream)return false;
        const bool stored=std::fwrite(expected_position.data(),sizeof(float),expected_position.size(),stream)==expected_position.size();
        std::fclose(stream);
        if(!stored)return false;
    }else{
        const auto stream=_wfopen(path.c_str(),L"rb");
        if(!stream)return false;
        const bool loaded=std::fread(expected_position.data(),sizeof(float),expected_position.size(),stream)==expected_position.size();
        std::fclose(stream);
        if(!loaded)return false;
    }
    const std::wstring current(map.begin(),map.end());
    const std::wstring item=L"~FixtureOwner~FixtureRoom~"+std::wstring(item_path)+L"~";
    std::wstring coordinates;
    for(const auto coordinate:expected_position)coordinates+=L"~"+std::to_wstring(static_cast<int>(coordinate));
    coordinates+=L"~0";
    keys={name(L"QDCoopDrop~fixtureGround"+item+current+coordinates),
          name(L"QDCoopDrop~fixtureForeign"+item+L"/Game/Foreign.Foreign"+coordinates),
          name(L"QDCoopDrop~fixtureClaimed"+item+current+coordinates),
          name(L"QDCoopDrop~fixtureMalformed"+item+current+L"~0~broken~0~0"),
          name(L"QDCoopDrop~fixtureDuplicate"+item+current+coordinates),
          name(L"QDCoopDrop~fixtureDuplicate"+item+current+L"~"+
               std::to_wstring(static_cast<int>(expected_position[0])+10)+L"~"+
               std::to_wstring(static_cast<int>(expected_position[1]))+L"~"+
               std::to_wstring(static_cast<int>(expected_position[2]))+L"~0")};
    receipt=name(L"QDCoopPersonal~got~fixtureGround");
    return receipt.index&&std::ranges::all_of(keys,[](const auto key){return key.index!=0;});
}
bool inventory(const SaveMap& map,std::vector<InventoryEntry>& items){
    if(!map.data||at<std::array<int,6>>(map.field,0x88)!=std::array<int,6>{8,16,20,24,8,24}||
       !WP::read_sparse_map<InventoryEntry,24>(map.data,read_local,items))return false;
    std::ranges::sort(items,{},&InventoryEntry::item);
    return true;
}
bool expected_inventory(){
    inventory_expected=inventory_before;
    if(recovery_mode||(remote_mode?lan.host:!lan.host))return true;
    constexpr std::string_view asset="/Game/Items/Consumables/Herb/HealingHerb.HealingHerb";
    if(!ui.get(herb))if(const auto item=ui.load_asset_path(asset))ui.remember(herb,item);
    const auto item=ui.get(herb);
    if(!item)return false;
    const auto it=std::ranges::find(inventory_expected,item,&InventoryEntry::item);
    if(it==inventory_expected.end()){
        inventory_expected.push_back({item,1});
        std::ranges::sort(inventory_expected,{},&InventoryEntry::item);
    }else if(it->count<INT_MAX)++it->count;
    else return false;
    return true;
}
unsigned scan(ULONGLONG time){
    if(time<next_scan)return UINT_MAX;
    next_scan=time+250;
    const auto array=at<UE::ObjectArray>(image+TitleUi::object_array,0);
    if(!UE::object_array_shape(array))return UINT_MAX;
    const auto type=ui.get(placed_type);
    const bool bind=std::ranges::any_of(symbols,[](const auto key){return !ui.get(key);});
    unsigned actors{};
    for(int i=0;i<array.num_elements;++i){
        const auto chunk=at<Address>(array.chunks,(i/UE::objects_per_chunk)*8);
        if(!chunk)continue;
        const auto item=at<UE::ObjectItem>(chunk,(i%UE::objects_per_chunk)*sizeof(UE::ObjectItem));
        if(!item.object||(item.flags&0x30000000u))continue;
        if(type&&at<Address>(item.object,16)==type&&!(at<unsigned>(item.object,8)&0x10u)){
            if(!actors)ui.remember(visual,item.object);
            ++actors;
        }
        if(!bind)continue;
        const auto short_name=leaf(item.object);
        for(unsigned j=0;j<paths.size();++j)
            if(!ui.get(symbols[j])&&short_name==paths[j].substr(paths[j].rfind('.')+1)&&path_is(item.object,paths[j]))
                ui.remember(symbols[j],item.object);
    }
    return actors;
}
bool flags(Address actor){
    for(const auto property:{"GUIDs","ItemsToGrant","ActorsToActivateOnPickup"}){
        const bool grants=std::strcmp(property,"ItemsToGrant")==0;
        const auto field=ui.member(actor,property,grants?"MapProperty":"ArrayProperty",grants?80:16);
        if(!field||at<int>(actor+at<unsigned>(field,76),8)!=0)return false;
    }
    const auto temporary=ui.member(actor,"TransientPickup","BoolProperty",1);
    const auto hidden=ui.member(actor,"bHidden","BoolProperty",1);
    return temporary&&hidden&&UE::get_bool(reinterpret_cast<void*>(temporary),reinterpret_cast<void*>(actor),0)&&
        !UE::get_bool(reinterpret_cast<void*>(hidden),reinterpret_cast<void*>(actor),0);
}
bool geometry(Address actor){
    const auto graphics=ui.object(actor,"PickupGraphics"),interact=ui.object(actor,"InteractCollision");
    const auto mesh=ui.object(graphics,"StaticMesh");
    if(!graphics||!interact||!mesh||!path_is(mesh,herb_mesh)||!flags(actor)||
       !ui.method(fn(actor_location),12,{{"ReturnValue","StructProperty",0,12}})||
       !ui.method(fn(actor_collision),1,{{"ReturnValue","BoolProperty",0,1}})||
       !ui.method(fn(visible),1,{{"ReturnValue","BoolProperty",0,1}})||
       !fn(component_collision)||at<std::uint16_t>(fn(component_collision),182)!=1||
       !parameter(fn(component_collision),"ReturnValue",0,1))return false;
    std::array<float,3> point{};
    TitleUi::event(actor,fn(actor_location),point.data());
    for(unsigned i=0;i<point.size();++i)if(std::abs(point[i]-expected_position[i])>1.f)return false;
    unsigned char actor_enabled=255,mesh_enabled=255,interact_enabled=255,shown{};
    TitleUi::event(actor,fn(actor_collision),&actor_enabled);
    TitleUi::event(graphics,fn(component_collision),&mesh_enabled);
    TitleUi::event(interact,fn(component_collision),&interact_enabled);
    TitleUi::event(graphics,fn(visible),&shown);
    const unsigned expected_mode=1;
    if(actor_enabled!=expected_mode||mesh_enabled||interact_enabled!=expected_mode||!shown)return false;
    if(auto stream=_wfopen(file(lan.role+L"-geometry.txt").c_str(),L"wb")){
        std::fprintf(stream,"xyz=%.2f,%.2f,%.2f mesh=%.*s visible=%u collision=%u,%u,%u empty=1\n",
            static_cast<double>(point[0]),static_cast<double>(point[1]),static_cast<double>(point[2]),
            static_cast<int>(herb_mesh.size()),herb_mesh.data(),shown,actor_enabled,mesh_enabled,interact_enabled);
        std::fclose(stream);
        return true;
    }
    return false;
}
bool input_method(Symbol symbol){
    const auto method=fn(symbol);
    const auto field=method?UE::find_property(method,image+TitleUi::name_pool,"Key",read_local):0;
    const auto size=field?at<unsigned>(field,60):0;
    const auto type=field&&ui.kind(field,"StructProperty")?at<Address>(field,0x78):0;
    return size&&size<=32&&type&&path_is(type,"/Script/InputCore.Key")&&at<unsigned>(type,88)==size&&
        ui.method(method,size,{{"Key","StructProperty",0,size}});
}
void release_save_locks(){
    for(auto& handle:save_locks){
        if(handle&&handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);
        handle=nullptr;
    }
}
bool lock_suspend_saves(ULONGLONG time){
    if(!lan.host||!save_retry_mode||save_lock_started)return true;
    wchar_t path[32768]{};
    if(!environment_title(L"QD_PLACED_STOCK_SAVE_DIR",path,32768))return false;
    std::wstring directory=path,expected=root+L"/host/profile/Saved/SaveGames/";
    std::replace(directory.begin(),directory.end(),L'\\',L'/');
    std::replace(expected.begin(),expected.end(),L'\\',L'/');
    if(directory.size()!=expected.size()+17||_wcsnicmp(directory.c_str(),expected.c_str(),expected.size())||
       !std::all_of(directory.begin()+static_cast<std::ptrdiff_t>(expected.size()),directory.end(),
          [](wchar_t digit){return digit>=L'0'&&digit<=L'9';}))return false;
    for(unsigned i=0;i<save_locks.size();++i){
        const auto slot=directory+L"/World1_Suspend_"+std::to_wstring(i)+L".sav";
        save_locks[i]=CreateFileW(slot.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(save_locks[i]==INVALID_HANDLE_VALUE){release_save_locks();return false;}
    }
    save_lock_started=true;
    save_unlock_at=reopen_mode?0:time+1800;
    marker(file(L"host-save-locked.txt").c_str());
    return true;
}
bool click(Address instance,Address actor,ULONGLONG time){
    if((remote_mode?lan.host:!lan.host)||!actor||time<next_click||!ui.method(fn(interact),8,{{"PlayerInteracting","ObjectProperty",0,8}})||
       !(at<unsigned>(fn(interact),176)&0x400u))return false;
    next_click=time+600;
    Address pawn[]{instance,0,0};
    TitleUi::event(ui.get(TitleUi::gameplay),ui.get(TitleUi::local_pawn),pawn);
    if(!pawn[2])return false;
    std::array<float,3> point{};
    if(fn(actor_location))TitleUi::event(pawn[2],fn(actor_location),point.data());
    if(auto stream=_wfopen(file(lan.role+L"-pickup-pawn.txt").c_str(),L"wb")){
        std::fprintf(stream,"xyz=%.2f,%.2f,%.2f\n",static_cast<double>(point[0]),
            static_cast<double>(point[1]),static_cast<double>(point[2]));
        std::fclose(stream);
    }
    Address foreign=actor;
    TitleUi::event(actor,fn(interact),&foreign);
    if(input_mode){
        const auto interaction=ui.object(pawn[2],"PlayerInteractionComponent");
        const bool methods=interaction&&ui.method(fn(interaction_update),0,{})&&
            input_method(player_press)&&input_method(player_release);
        if(!methods){
            if(auto stream=_wfopen(file(L"host-stock-binding-missing.txt").c_str(),L"wb")){
                for(const auto symbol:{interaction_update,player_press,player_release}){
                    const auto method=fn(symbol);
                    std::fprintf(stream,"symbol=%u bound=%u size=%u count=%u\n",static_cast<unsigned>(symbol),method!=0,
                        method?at<std::uint16_t>(method,182):0,method?at<unsigned char>(method,180):0);
                }
                std::fclose(stream);
            }
            return false;
        }
        TitleUi::event(interaction,fn(interaction_update),nullptr);
        const auto selected=ui.object(interaction,"Current Interact Actor");
        if(auto stream=_wfopen(file(L"host-stock-target-state.txt").c_str(),L"wb")){
            std::fprintf(stream,"selected=%s matching=%u\n",selected?leaf(at<Address>(selected,16)).c_str():"None",selected==actor);
            std::fclose(stream);
        }
        if(selected!=actor)return false;
        if(!lock_suspend_saves(time))return false;
        marker(file(L"host-stock-target.txt").c_str());
        alignas(16) std::array<unsigned char,32> key{};
        TitleUi::event(pawn[2],fn(player_press),key.data());
        TitleUi::event(pawn[2],fn(player_press),key.data());
        TitleUi::event(pawn[2],fn(player_release),key.data());
        return true;
    }
    if(!lock_suspend_saves(time))return false;
    TitleUi::event(actor,fn(interact),&pawn[2]);
    TitleUi::event(actor,fn(interact),&pawn[2]);
    return true;
}
bool cycle_ready(){return reopen_mode&&seen(L"host-cycle-requested.txt");}

bool restored(bool cycling=false){
    const auto method=fn(interact);
    const bool valid=method&&at<unsigned>(method,176)==0x0c020000&&at<Address>(method,216)==image+0x11a7e40;
    const bool ready=valid&&(cycling||(!save_locks[0]&&!save_locks[1]));
    if(ready)mark(cycling?L"cycle-hook-restored":L"hook-restored");
    return ready;
}
void diagnostics(const SaveMap& names,unsigned actors,ULONGLONG time){
    if(time<next_report)return;
    next_report=time+1000;
    if(auto stream=_wfopen(file(lan.role+L"-placed-state.txt").c_str(),L"wb")){
        std::fprintf(stream,"stage=%u seeded=%u clicked=%u actors=%u receipt=%d names=",stage,seeded,claimed,actors,
            receipt.index?get(names,receipt).value_or(-1):-1);
        for(const auto key:keys)std::fprintf(stream,"%d,",key.index?get(names,key).value_or(-1):-1);
        std::fputc('\n',stream);
        if(!read_medic)if(const auto dll=GetModuleHandleW(L"QDCoopNativeM1.dll"))
            load_export(dll,"QDEnemyInspectNativeMedic",read_medic);
        qdcoop::rescue::BindingView medic{};
        if(read_medic&&read_medic(&medic))std::fprintf(stream,"medic_phase=%u bind_check=%u errors=%llu\n",
            medic.phase,medic.bind_check,static_cast<unsigned long long>(medic.errors));
        std::fclose(stream);
    }
    if(!read_core){
        const auto dll=GetModuleHandleW(L"QDCoopNativeM1.dll");
        if(!dll||!load_export(dll,"QDEnemyNativeStatus",read_core))return;
    }
    const auto size=read_core(nullptr,0);
    if(!size||size>32768)return;
    std::string result(size,'\0');
    if(read_core(result.data(),size)!=size)return;
    if(auto stream=_wfopen(file(lan.role+L"-placed-core.json").c_str(),L"wb")){
        std::fputs(result.c_str(),stream);
        std::fclose(stream);
    }
}
#include "placed_item_source_title_fixture.inc"
#include "placed_item_legacy_title_fixture.inc"

bool step(Address instance,ULONGLONG time){
    if(root.empty()){
        wchar_t path[32768]{};
        if(!environment_title(L"QD_PLACED_ITEM_TITLE_ROOT",path,32768))return false;
        root=path;
        wchar_t mode[2]{};
        recovery_mode=environment_title(L"QD_PLACED_RECOVERY",mode,2)&&mode[0]==L'1';
        input_mode=environment_title(L"QD_PLACED_INPUT",mode,2)&&mode[0]==L'1';
        remote_mode=environment_title(L"QD_PLACED_REMOTE",mode,2)&&mode[0]==L'1';
        source_mode=remote_mode&&environment_title(L"QD_PLACED_SOURCE",mode,2)&&mode[0]==L'1';
        source_ui_mode=source_mode&&environment_title(L"QD_PLACED_STOCK_UI",mode,2)&&mode[0]==L'1';
        reopen_mode=remote_mode&&environment_title(L"QD_PLACED_REOPEN",mode,2)&&mode[0]==L'1';
        save_retry_mode=reopen_mode||(environment_title(L"QD_PLACED_SAVE_RETRY",mode,2)&&mode[0]==L'1');
    }
    if(stage==4)return all(L"clean");
    if(!ui.get(placed_type))
        if(const auto type=ui.load_asset_path(placed_type_path))ui.remember(placed_type,type);
    const auto actors=scan(time);
    if(actors==UINT_MAX||!ui.get(placed_type)||!fn(map_add)||!fn(map_find)||!fn(map_remove)||
       at<Address>(fn(map_add),216)!=image+0x2d7f640||at<Address>(fn(map_find),216)!=image+0x2d7fb10||
       at<Address>(fn(map_remove),216)!=image+0x2d7ff70)return false;
    const auto save=ui.object(instance,"ActiveSaveSlot");
    const auto names=save_map(save,"NamedSaveData"),items=save_map(save,"InventoryContents");
    if(!names.data||!items.data||at<std::array<int,6>>(names.field,0x88)!=std::array<int,6>{8,12,16,20,4,20})return false;
    if(source_mode)return source_step(instance,names,items,actors,time);
    if(legacy_mode())return legacy_step(names,items,actors,time);
    if(reopen_mode&&lan.cycle==3){
        if(ui.get(original_save)!=save)return false;
        if(!seen(lan.role+L"-same-save.txt"))mark(L"same-save");
        if(!lan.host&&!seen(L"host-save-unlocked.txt")&&
           (get(names,keys[0])==1||get(names,keys[0])==2||get(names,receipt)))mark(L"save-premature");
    }
    if(lan.host&&save_locks[0]){
        if(keys[0].index&&get(names,keys[0])==2)marker(file(L"host-save-premature.txt").c_str());
        if(reopen_mode&&!save_unlock_at&&lan.cycle==3&&all(L"same-save"))save_unlock_at=time+1800;
        if(save_unlock_at&&time>=save_unlock_at){
            marker(file(L"host-save-unlocked.txt").c_str());
            release_save_locks();
        }
    }
    if(stage==0){
        if((!recovery_mode&&actors)||!init_keys(instance)||!inventory(items,inventory_before)||!expected_inventory())return false;
        if(reopen_mode){
            ui.remember(original_save,save);
            if(ui.get(original_save)!=save)return false;
        }
        if(recovery_mode){
            if(lan.host?get(names,receipt)!=1:get(names,receipt).has_value())return false;
            seeded=lan.host;
        }else if(get(names,receipt)||std::ranges::any_of(keys,[&](const auto key){return get(names,key).has_value();}))return false;
        stage=1;
        mark(L"prepared");
    }
    if(lan.host&&!seeded&&all(L"prepared")){
        for(unsigned i=0;i<keys.size();++i)set(names,keys[i],i==2?2:0);
        seeded=true;
    }
    if(recovery_mode&&stage==1&&!actors&&get(names,keys[0])==2){
        std::vector<InventoryEntry> current;
        if(!inventory(items,current)||current!=inventory_expected)return false;
        stage=3;
        mark(L"retired");
    }
    if(!recovery_mode&&stage==1&&actors==1&&get(names,keys[0])==0&&get(names,keys[1])==0&&get(names,keys[2])==2&&
       get(names,keys[3])==0&&get(names,keys[4])==0&&get(names,keys[5])==0&&geometry(ui.get(visual))){
        std::vector<InventoryEntry> current;
        if(!inventory(items,current)||current!=inventory_before)return false;
        stage=2;
        mark(L"rendered");
    }
    if(reopen_mode&&lan.host&&!lan.cycle&&stage==2&&all(L"rendered")){
        if(!lock_suspend_saves(time))return false;
        if(get(names,keys[0])==1&&seen(L"client-attempted.txt")&&seen(L"client2-attempted.txt")&&seen(L"client3-attempted.txt"))
            mark(L"cycle-requested");
    }
    if(!recovery_mode&&!claimed&&stage==2&&get(names,keys[0])==0&&!get(names,receipt)&&all(L"rendered")&&
       (!reopen_mode||seen(L"host-save-locked.txt"))&&
       click(instance,ui.get(visual),time)){
        claimed=true;
        mark(L"attempted");
    }
    if(remote_mode&&stage==2&&get(names,keys[0])==1&&!seen(lan.role+L"-reserved.txt"))mark(L"reserved");
    if(stage==2&&!actors&&get(names,keys[0])==2){
        stage=3;
        mark(L"retired");
    }
    if(lan.host&&!removed_keys&&all(L"retired")){
        for(const auto key:keys)set(names,key,std::nullopt);
        removed_keys=true;
    }
    if(stage==3&&!actors&&std::ranges::none_of(keys,[&](const auto key){return get(names,key).has_value();})){
        std::vector<InventoryEntry> current;
        const auto received=get(names,receipt);
        const bool winner=remote_mode?!lan.host&&received==1:lan.host;
        if(!inventory(items,current)||current!=(winner?inventory_expected:inventory_before)||
           (winner?received!=1:received.has_value()))return false;
        if(remote_mode&&winner)mark(L"winner");
        stage=4;
        next_report=0;
        mark(L"clean");
    }
    diagnostics(names,actors,time);
    return stage==4&&all(L"clean");
}
}
