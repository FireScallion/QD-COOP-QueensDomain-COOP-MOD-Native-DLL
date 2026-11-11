constexpr std::string_view legacy_item="/Game/Items/Weapons/B33blade/B33Blade_Item.B33Blade_Item";
constexpr std::array legacy_text{
    L"QDCoopDrop~aLegacyForeign~OtherOldOwner~OldRoom~/Game/Items/Weapons/B33blade/B33Blade_Item.B33Blade_Item~/Game/Fixture.Fixture~0~0~0~0",
    L"QDCoopDrop~zLegacyOwn~OldLocalOwner~OldRoom~/Game/Items/Weapons/B33blade/B33Blade_Item.B33Blade_Item~/Game/Fixture.Fixture~0~0~0~0",
    L"QDCoopWinner~aLegacyForeign~OldLocalOwner",L"QDCoopWinner~zLegacyOwn~OldLocalOwner",
    L"QDCoopPersonal~id~OldLocalOwner",L"QDCoopRoom~OldRoom"
};
UE::WeakKey legacy_asset{},legacy_load{},legacy_write{};
UE::NameKey foreign_receipt{};
bool legacy_seeded{};

bool legacy_mode(){
    static const bool enabled=[](){wchar_t value[2]{};return environment_title(L"QD_PLACED_LEGACY",value,2)&&value[0]==L'1';}();
    return enabled;
}

bool legacy_keys(){
    for(unsigned i=0;i<keys.size();++i)if(!keys[i].index)keys[i]=name(legacy_text[i]);
    if(!receipt.index)receipt=name(L"QDCoopPersonal~got~zLegacyOwn");
    if(!foreign_receipt.index)foreign_receipt=name(L"QDCoopPersonal~got~aLegacyForeign");
    if(!ui.get(legacy_asset))if(const auto item=ui.load_asset_path(legacy_item))ui.remember(legacy_asset,item);
    return receipt.index&&foreign_receipt.index&&ui.get(legacy_asset)&&
        std::ranges::all_of(keys,[](const auto key){return key.index!=0;});
}

bool legacy_stock_calls(){
    if(!ui.get(legacy_load)||!ui.get(legacy_write)){
        const auto array=at<UE::ObjectArray>(image+TitleUi::object_array,0);
        if(!UE::object_array_shape(array))return false;
        for(int i=0;i<array.num_elements;++i){
            const auto chunk=at<Address>(array.chunks,(i/UE::objects_per_chunk)*8);
            if(!chunk)continue;
            const auto object=at<UE::ObjectItem>(chunk,(i%UE::objects_per_chunk)*sizeof(UE::ObjectItem));
            if(!object.object||(object.flags&0x30000000u))continue;
            const auto short_name=leaf(object.object);
            if(short_name=="LoadGameFromSlot"&&path_is(object.object,"/Script/Engine.GameplayStatics.LoadGameFromSlot"))
                ui.remember(legacy_load,object.object);
            if(short_name=="SaveGameToSlot"&&path_is(object.object,"/Script/Engine.GameplayStatics.SaveGameToSlot"))
                ui.remember(legacy_write,object.object);
        }
    }
    return ui.method(ui.get(legacy_load),32,{{"SlotName","StrProperty",0,16},{"UserIndex","IntProperty",16,4},
                     {"ReturnValue","ObjectProperty",24,8}})&&
        ui.method(ui.get(legacy_write),29,{{"SaveGameObject","ObjectProperty",0,8},{"SlotName","StrProperty",8,16},
                  {"UserIndex","IntProperty",24,4},{"ReturnValue","BoolProperty",28,1}});
}

bool legacy_seed_title(){
    if(root.empty()){
        wchar_t path[32768]{};
        if(!environment_title(L"QD_PLACED_ITEM_TITLE_ROOT",path,32768))return false;
        root=path;
    }
    if(legacy_seeded)return seen(L"legacy-baseline.txt");
    scan(GetTickCount64());
    if(!legacy_keys()||!legacy_stock_calls()||!fn(map_add)||!fn(map_remove)||
       at<Address>(fn(map_add),216)!=image+0x2d7f640||at<Address>(fn(map_remove),216)!=image+0x2d7ff70)return false;
    wchar_t path[32768]{};
    if(!environment_title(L"QD_PLACED_STOCK_SAVE_DIR",path,32768))return false;
    std::wstring directory=path,expected=root+L"/host/profile/Saved/SaveGames/";
    std::replace(directory.begin(),directory.end(),L'\\',L'/');
    std::replace(expected.begin(),expected.end(),L'\\',L'/');
    if(directory.size()!=expected.size()+17||_wcsnicmp(directory.c_str(),expected.c_str(),expected.size())||
       !std::all_of(directory.begin()+static_cast<std::ptrdiff_t>(expected.size()),directory.end(),
          [](wchar_t digit){return digit>=L'0'&&digit<=L'9';}))return false;
    const auto game=ui.get(TitleUi::gameplay),item=ui.get(legacy_asset);
    for(const auto tail:{L"World1.sav",L"World1_Suspend_0.sav",L"World1_Suspend_1.sav"}){
        const auto slot=L"FULLPATH:"+directory+L"/"+tail;
        const auto length=static_cast<int>(slot.size()+1);
        struct Read {TitleUi::String slot;int user{},padding{};Address save{};};
        struct Write {Address save;TitleUi::String slot;int user{};unsigned char ok{};};
        Read read{{slot.c_str(),length,length}};
        TitleUi::event(game,ui.get(legacy_load),&read);
        if(!read.save)return false;
        const auto names=save_map(read.save,"NamedSaveData"),items=save_map(read.save,"InventoryContents");
        std::vector<InventoryEntry> current;
        if(!names.data||at<std::array<int,6>>(names.field,0x88)!=std::array<int,6>{8,12,16,20,4,20}||
           !inventory(items,current)||std::ranges::find(current,item,&InventoryEntry::item)!=current.end())return false;
        struct Entry {UE::NameKey key;int value;};
        std::vector<Entry> prior;
        if(!WP::read_sparse_map<Entry,20>(names.data,read_local,prior))return false;
        for(const auto& entry:prior){
            std::string key;
            if(!UE::name_string(image+TitleUi::name_pool,entry.key,read_local,key))return false;
            if(PI::prefix(key,PI::identity_prefix)||PI::prefix(key,PI::room_prefix))set(names,entry.key,std::nullopt);
        }
        for(const auto key:keys)set(names,key,1);
        Write write{read.save,read.slot};
        TitleUi::event(game,ui.get(legacy_write),&write);
        if(write.ok!=1)return false;
    }
    legacy_seeded=true;
    mark(L"legacy-seeded");
    return false;
}

bool legacy_step(const SaveMap& names,const SaveMap& items,unsigned actors,ULONGLONG time){
    diagnostics(names,actors,time);
    if(stage==4)return all(L"clean");
    if(actors||!legacy_keys())return false;
    if(stage==0){stage=1;mark(L"prepared");}
    if(get(names,keys[0])!=1||get(names,keys[1])!=2||get(names,foreign_receipt)||
       (lan.host?get(names,receipt)!=1:get(names,receipt).has_value()))return false;
    std::vector<InventoryEntry> current;
    if(!inventory(items,current))return false;
    const auto found=std::ranges::find(current,ui.get(legacy_asset),&InventoryEntry::item);
    if(lan.host?(found==current.end()||found->count!=1):found!=current.end())return false;
    stage=4;
    next_report=0;
    diagnostics(names,actors,time);
    mark(L"retired");
    mark(L"clean");
    return all(L"clean");
}
