namespace Medic=qdcoop::rescue;
static constexpr std::array<std::string_view,Medic::medicines> medicine_paths{
    "/Game/Items/Consumables/HealingPotion/HealingPotion.HealingPotion",
    "/Game/Items/Consumables/HealingPotion/HealingPotion_02.HealingPotion_02",
    "/Game/Items/Consumables/HealingPotion/HealingPotion_03.HealingPotion_03",
    "/Game/Items/Consumables/Herb/HealingHerb.HealingHerb",
    "/Game/Items/Consumables/Herb/HealingHerb_02.HealingHerb_02",
    "/Game/Items/Consumables/Herb/HealingHerb_03.HealingHerb_03"
};
static void* load_player_asset(std::string_view path,unsigned type,bool class_asset);
struct MedicMap {
    void* field{};
    int offset{};
    void* address(void* save) const {return static_cast<unsigned char*>(save)+offset;}
};
using MedicMapAdd=void(*)(void*,const void*,const void*,const void*);
using MedicMapFind=bool(*)(const void*,const void*,const void*,void*);
static unsigned medic_bind_check;
static unsigned medic_save_bind_check;
struct MedicSave {
    Weak pawn,gi,save,type;
    MedicMap markers,inventory;
    MedicMapAdd add{};
    MedicMapFind find{};
    int active_at{},selected_at{},slot_index{};
    std::wstring primary;
    std::array<AnimU::NameKey,4> identity_names{};
    AnimU::NameKey branch_name{};
    std::array<AnimU::NameKey,Medic::medicines> revisions{},uses{};
};
static bool medic_params(unsigned id,unsigned bytes,
                         std::initializer_list<std::tuple<std::string_view,int,int>> fields){
    const auto fn=world_input(id);
    medic_bind_check=100+id;
    if(!fn||*api.parms(fn)!=bytes)return false;
    for(const auto& [name,width,at]:fields)
        if(factory_parameter(fn,name,width)!=at)return false;
    return true;
}
static bool medic_field_class(void* field,std::string_view expected){
    const auto type=field?AnimU::field<void*>(field,8):nullptr;
    std::string name;
    return type&&AnimU::name_string(catalog_name_pool,AnimU::field<AnimU::NameKey>(type,0),catalog_read,name)&&
           name==expected;
}
static MedicMap medic_map(void* type,std::string_view name,bool object_key){
    const auto field=catalog_field(type,name,80);
    if(!medic_field_class(field,"MapProperty"))return {};
    const auto key=AnimU::field<void*>(field,0x78),value=AnimU::field<void*>(field,0x80);
    const auto shape=object_key?std::array<int,6>{8,16,20,24,8,24}:std::array<int,6>{8,12,16,20,4,20};
    if(!medic_field_class(key,object_key?"ObjectProperty":"NameProperty")||
       !medic_field_class(value,"IntProperty")||*api.size(key)!=8||*api.size(value)!=4||
       AnimU::field<unsigned>(field,0xa0)||AnimU::field<std::array<int,6>>(field,0x88)!=shape)return {};
    return {field,*api.offset(field)};
}
static bool medic_native_maps(MedicSave& save){
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto add=world_input(world_medic_map_add),find=world_input(world_medic_map_find);
    const auto add_entry=image+0x2755ec0,find_entry=image+0x2756200;
    constexpr unsigned char add_prefix[]{0x48,0x85,0xc9,0x0f,0x84,0x70,0x02,0,0,0x48,0x89,0x5c,0x24,0x08};
    constexpr unsigned char find_prefix[]{0x40,0x55,0x53,0x57,0x48,0x8d,0x6c,0x24,0xb9,0x48,0x81,0xec,0xc0,0,0,0};
    constexpr unsigned char add_call[]{0xe8,0x71,0x66,0x9d,0xff},find_call[]{0xe8,0xc6,0x64,0x9d,0xff};
    if(AnimU::field<std::uintptr_t>(add,216)!=image+0x2d7f640||
       AnimU::field<std::uintptr_t>(find,216)!=image+0x2d7fb10||
       std::memcmp(reinterpret_cast<const void*>(image+0x2d7f84a),add_call,sizeof(add_call))||
       std::memcmp(reinterpret_cast<const void*>(image+0x2d7fd35),find_call,sizeof(find_call))||
       std::memcmp(reinterpret_cast<const void*>(add_entry),add_prefix,sizeof(add_prefix))||
       std::memcmp(reinterpret_cast<const void*>(find_entry),find_prefix,sizeof(find_prefix)))return false;
    save.add=reinterpret_cast<MedicMapAdd>(add_entry);
    save.find=reinterpret_cast<MedicMapFind>(find_entry);
    return true;
}
struct MedicString {
    const wchar_t* data{};
    int size{},capacity{};
};
static MedicString medic_string(const std::wstring& value){
    const auto size=static_cast<int>(value.size()+1);
    return {value.c_str(),size,size};
}
static AnimU::NameKey medic_name(const std::wstring& text){
    alignas(16) FactoryParameters args{};
    factory_put(args,0,medic_string(text));
    api.event(world_input(world_strings),world_input(world_string_name),args.data());
    return AnimU::field<AnimU::NameKey>(args.data(),16);
}
static bool medic_name_is(const AnimU::NameKey& key,const std::wstring& expected){
    std::string text;
    return AnimU::name_string(catalog_name_pool,key,catalog_read,text)&&
           std::equal(text.begin(),text.end(),expected.begin(),expected.end());
}
static bool medic_stock_path(void* gi,MedicSave& save){
    medic_bind_check=20;
    int index=-1;
    api.event(gi,world_input(world_medic_save_index),&index);
    if(index<0)return false;
    alignas(16) FactoryParameters args{};
    factory_put(args,0,index);
    api.event(world_input(world_medic_utils),world_input(world_medic_save_name),args.data());
    const auto result=AnimU::field<MedicString>(args.data(),16);
    medic_bind_check=21;
    std::wstring path;
    if(result.data&&result.size>0&&result.size<=2048&&result.capacity>=result.size&&result.data[result.size-1]==0)
        path.assign(result.data,static_cast<std::size_t>(result.size-1));
    if(result.data)native_free(const_cast<wchar_t*>(result.data));
    if(!path.starts_with(L"FULLPATH:"))return false;
    path.erase(0,9);
    medic_bind_check=22;
    std::replace(path.begin(),path.end(),L'/',L'\\');
    if(path.size()<4||path[1]!=L':'||path[2]!=L'\\'||path.find(L':',2)!=std::wstring::npos||
       path.find(L'\0')!=std::wstring::npos)return false;
    wchar_t canonical[2048]{};
    medic_bind_check=23;
    const auto length=GetFullPathNameW(path.c_str(),static_cast<DWORD>(std::size(canonical)),canonical,nullptr);
    if(!length||length>=std::size(canonical)||_wcsicmp(canonical,path.c_str()))return false;
    medic_bind_check=24;
    const auto tail_matches=[](std::wstring_view value,std::wstring_view suffix){
        return value.size()>suffix.size()&&!_wcsnicmp(value.data()+value.size()-suffix.size(),suffix.data(),suffix.size());
    };
    const auto filename=L"\\World"+std::to_wstring(index)+L".sav";
    if(!tail_matches(path,filename))return false;
    const auto parent=std::wstring_view(path).substr(0,path.size()-filename.size());
    constexpr std::wstring_view save_folder=L"\\Saved\\SaveGames";
    if(!tail_matches(parent,save_folder)){
        const auto separator=parent.rfind(L'\\');
        if(separator==std::wstring_view::npos||!tail_matches(parent.substr(0,separator),save_folder))return false;
        const auto account=parent.substr(separator+1);
        if(account.empty()||account.size()>20||account.front()==L'0'||
           !std::ranges::all_of(account,[](wchar_t digit){return digit>=L'0'&&digit<=L'9';}))return false;
        const std::string digits(account.begin(),account.end());
        qdcoop::steam_room::SteamId steam_id{};
        const auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),steam_id);
        if(parsed.ec!=std::errc{}||parsed.ptr!=digits.data()+digits.size()||!qdcoop::steam_room::user_id(steam_id))return false;
    }
    save.primary=std::move(path);
    save.slot_index=index;
    return true;
}
static bool medic_bind_save(void* pawn,void* gi,MedicSave& save){
    medic_bind_check=1;
    if(!native_free||!gi||!world_input(world_strings)||!world_input(world_kismet)||
       !world_input(static_cast<unsigned>(world_object_count)+factory_gameplay))return false;
    for(unsigned i=world_medic_first;i<world_view_first;++i)if(!world_input(i)){
        medic_bind_check=200+i;
        return false;
    }
    if(!medic_params(world_medic_save_index,4,{{"ReturnValue",4,0}})||
       !medic_params(world_medic_save_name,32,{{"SlotIndex",4,0},{"bForceDemoName",1,4},{"bBackup",1,5},
            {"bSuspend",1,6},{"SuspendIndex",4,8},{"ReturnValue",16,16}})||
       !medic_params(world_medic_write,29,{{"SaveGameObject",8,0},{"SlotName",16,8},{"UserIndex",4,24},{"ReturnValue",1,28}})||
       !medic_params(world_medic_read,32,{{"SlotName",16,0},{"UserIndex",4,16},{"ReturnValue",8,24}})||
       !medic_params(world_medic_soft,48,{{"Object",8,0},{"ReturnValue",40,8}})||
       !medic_params(world_medic_consume,57,{{"Item",40,0},{"Count",4,40},{"__WorldContext",8,48},{"Success",1,56}})||
       !medic_params(world_medic_credit,56,{{"Item",40,0},{"Count",4,40},{"__WorldContext",8,48}})||
       !medic_params(world_medic_refresh,0,{})||!medic_params(world_medic_count_use,0,{})||
       (!medic_params(world_medic_potion,4,{{"Level",4,0}})&&!medic_params(world_medic_potion,4,{{"level",4,0}}))||
       (!medic_params(world_medic_herb,4,{{"Level",4,0}})&&!medic_params(world_medic_herb,4,{{"level",4,0}}))||
       !medic_params(world_string_name,24,{{"inString",16,0},{"ReturnValue",8,16}})||
       !medic_native_maps(save))return false;
    medic_bind_check=2;
    const auto type=world_input(world_medic_save_type);
    save.active_at=life_offset(AnimU::field<void*>(gi,16),"ActiveSaveSlot",8);
    if(save.active_at<0)return false;
    const auto active=AnimU::field<void*>(gi,save.active_at);
    if(!active||AnimU::field<void*>(active,16)!=type||!makeweak(save.type,type)||
       !makeweak(save.pawn,pawn)||!makeweak(save.gi,gi)||!makeweak(save.save,active))return false;
    medic_bind_check=3;
    save.selected_at=life_offset(type,"SelectedConsumable",8);
    save.markers=medic_map(type,"NamedSaveData",false);
    save.inventory=medic_map(type,"InventoryContents",true);
    if(save.selected_at<0||!save.markers.field||!save.inventory.field||!medic_stock_path(gi,save))return false;
    medic_bind_check=4;
    for(unsigned i=0;i<save.identity_names.size();++i){
        const auto text=L"QDCOOPNativeRescueV1_Identity"+std::to_wstring(i);
        save.identity_names[i]=medic_name(text);
        if(!medic_name_is(save.identity_names[i],text))return false;
    }
    save.branch_name=medic_name(L"QDCOOPNativeRescueV1_Branch");
    if(!medic_name_is(save.branch_name,L"QDCOOPNativeRescueV1_Branch"))return false;
    for(unsigned i=0;i<Medic::medicines;++i){
        const auto revision=L"QDCOOPNativeRescueV1_Revision"+std::to_wstring(i+1);
        const auto uses=L"QDCOOPNativeRescueV1_Uses"+std::to_wstring(i+1);
        save.revisions[i]=medic_name(revision);
        save.uses[i]=medic_name(uses);
        if(!medic_name_is(save.revisions[i],revision)||!medic_name_is(save.uses[i],uses))return false;
    }
    medic_bind_check=5;
    return true;
}
static void* medic_active(const MedicSave& binding,void* pawn){
    const auto gi=api.weakget(&binding.gi),save=api.weakget(&binding.save);
    return pawn&&gi&&save&&api.weakget(&binding.pawn)==pawn&&
           AnimU::field<void*>(gi,binding.active_at)==save&&
           AnimU::field<void*>(save,16)==api.weakget(&binding.type)?save:nullptr;
}
static bool medic_identity(const MedicSave& binding,void* save,qdcoop::Id& id){
    std::array<std::uint32_t,4> words{};
    unsigned present{};
    for(unsigned i=0;i<words.size();++i)
        present+=binding.find(binding.markers.address(save),binding.markers.field,&binding.identity_names[i],&words[i]);
    if(!present){id={};return true;}
    if(present!=words.size())return false;
    id={words[0]|(static_cast<std::uint64_t>(words[1])<<32),words[2]|(static_cast<std::uint64_t>(words[3])<<32)};
    return id.valid();
}
static bool medic_set_identity(const MedicSave& binding,void* save,const qdcoop::Id& id){
    const std::array<std::uint32_t,4> words{static_cast<std::uint32_t>(id.high),static_cast<std::uint32_t>(id.high>>32),
        static_cast<std::uint32_t>(id.low),static_cast<std::uint32_t>(id.low>>32)};
    for(unsigned i=0;i<words.size();++i)
        binding.add(binding.markers.address(save),binding.markers.field,&binding.identity_names[i],&words[i]);
    qdcoop::Id read{};
    return medic_identity(binding,save,read)&&read==id;
}
static int medic_count(const MedicSave& binding,void* item){
    const auto gi=api.weakget(&binding.gi),count=world_input(world_life_count);
    if(!gi||!count||!item)return -1;
    alignas(16) FactoryParameters args{};
    factory_put(args,0,item);
    api.event(gi,count,args.data());
    return AnimU::field<int>(args.data(),8);
}
static bool medic_inventory_delta(void* pawn,void* item,int delta){
    const auto soft=world_input(world_medic_soft),system=world_input(world_kismet);
    const auto fn=world_input(delta<0?world_medic_consume:world_medic_credit);
    const auto cdo=world_input(world_medic_functions);
    if(!soft||!system||!fn||!cdo||!delta)return false;
    alignas(16) FactoryParameters reference{},args{};
    factory_put(reference,0,item);
    api.event(system,soft,reference.data());
    std::memcpy(args.data(),reference.data()+8,40);
    factory_put(args,40,delta<0?-delta:delta);
    factory_put(args,48,pawn);
    api.event(cdo,fn,args.data());
    return delta>0||args[56]==1;
}
static int medic_marker(const MedicSave& binding,void* save,const AnimU::NameKey& key){
    int value{};
    binding.find(binding.markers.address(save),binding.markers.field,&key,&value);
    return value;
}
static void medic_set_marker(const MedicSave& binding,void* save,const AnimU::NameKey& key,int value){
    binding.add(binding.markers.address(save),binding.markers.field,&key,&value);
}
static bool medic_branch(const MedicSave& binding,void* save,qdcoop::Id& id){
    if(!save)return false;
    const auto branch=medic_marker(binding,save,binding.branch_name);
    if(!branch){id={};return true;}
    return branch==1&&medic_identity(binding,save,id)&&id.valid();
}
static bool medic_fork_save(const MedicSave& binding,void* save,const qdcoop::Id& branch){
    if(!branch.valid()||!medic_set_identity(binding,save,branch))return false;
    medic_set_marker(binding,save,binding.branch_name,1);
    for(unsigned i=0;i<Medic::medicines;++i){
        medic_set_marker(binding,save,binding.revisions[i],0);
        medic_set_marker(binding,save,binding.uses[i],0);
    }
    qdcoop::Id observed{};
    return medic_branch(binding,save,observed)&&observed==branch;
}
static bool medic_reconcile_item(const MedicSave& binding,void* pawn,unsigned index,
                                 const Medic::Inventory& desired,void* item){
    const auto save=medic_active(binding,pawn);
    if(!save||!item)return false;
    const auto& revision_key=binding.revisions[index];
    const auto& uses_key=binding.uses[index];
    const auto revision=medic_marker(binding,save,revision_key),uses=medic_marker(binding,save,uses_key);
    if(revision<0||revision>desired.revision||uses<0||uses>desired.uses)return false;
    if(revision==desired.revision)return uses==desired.uses;
    const auto quantity=medic_count(binding,item);
    const auto refresh=world_input(world_medic_refresh),count_use=world_input(world_medic_count_use);
    if(quantity<0||!refresh||!count_use||desired.uses-uses>4096)return false;
    // Both markers must be visible to any save triggered by the stock inventory callback.
    medic_set_marker(binding,save,revision_key,desired.revision);
    medic_set_marker(binding,save,uses_key,desired.uses);
    const auto delta=desired.quantity-quantity;
    const bool changed=!delta||medic_inventory_delta(pawn,item,delta);
    if(!changed||medic_active(binding,pawn)!=save||medic_count(binding,item)!=desired.quantity){
        if(medic_active(binding,pawn)==save){
            medic_set_marker(binding,save,revision_key,revision);
            medic_set_marker(binding,save,uses_key,uses);
        }
        return false;
    }
    api.event(pawn,refresh,nullptr);
    for(int i=uses;i<desired.uses;++i)api.event(pawn,count_use,nullptr);
    return true;
}
static bool medic_save_snapshot(const MedicSave& binding,void* pawn,const std::wstring& path){
    const auto save=medic_active(binding,pawn),gs=world_input(static_cast<unsigned>(world_object_count)+factory_gameplay);
    const auto write=world_input(world_medic_write);
    if(!save||!gs||!write||path.size()>2048)return false;
    const auto slot=L"FULLPATH:"+path;
    alignas(16) FactoryParameters args{};
    factory_put(args,0,save);
    factory_put(args,8,medic_string(slot));
    api.event(gs,write,args.data());
    return args[28]==1;
}
static void* medic_load_snapshot(const MedicSave& binding,const std::wstring& path){
    const auto gs=world_input(static_cast<unsigned>(world_object_count)+factory_gameplay),load=world_input(world_medic_read);
    if(!gs||!load||path.size()>2048)return nullptr;
    const auto slot=L"FULLPATH:"+path;
    alignas(16) FactoryParameters args{};
    factory_put(args,0,medic_string(slot));
    api.event(gs,load,args.data());
    const auto saved=AnimU::field<void*>(args.data(),24);
    return saved&&saved!=api.weakget(&binding.save)&&AnimU::field<void*>(saved,16)==api.weakget(&binding.type)?
           saved:nullptr;
}
