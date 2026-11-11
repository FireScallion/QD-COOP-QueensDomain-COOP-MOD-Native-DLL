namespace TestWP=qdcoop::world_progress;
struct WorldProgressTestMap { Address field{},data{}; };
struct WorldProgressTestInventory {
    Address item;
    std::int32_t count;
    bool operator==(const WorldProgressTestInventory&) const = default;
};
static WorldProgressTestMap test_world_map(Address save,const char* name){
    const auto field=field_of(at<Address>(save,16),name,80);
    return {field,field?save+at<int>(field,76):0};
}
template<class Value=int,class Key> static std::optional<Value> test_world_find(const WorldProgressTestMap& map,const Key& key){
    Value value{};
    const auto find=reinterpret_cast<bool(*)(Address,Address,const void*,void*)>(image+0x2756200);
    return find(map.data,map.field,&key,&value)?std::optional<Value>{value}:std::nullopt;
}
template<class Key,class Value> static void test_world_put_value(const WorldProgressTestMap& map,const Key& key,std::optional<Value> value){
    if(value){
        const auto add=reinterpret_cast<void(*)(Address,Address,const void*,const void*)>(image+0x2755ec0);
        add(map.data,map.field,&key,&*value);
    }else{
        const auto remove=reinterpret_cast<bool(*)(Address,Address,const void*)>(image+0x2756630);
        remove(map.data,map.field,&key);
    }
}
template<class Key> static void test_world_put(const WorldProgressTestMap& map,const Key& key,std::optional<int> value){
    test_world_put_value(map,key,value);
}
static U::NameKey test_world_name(const std::wstring& text){
    struct String { const wchar_t* data; int count,capacity; };
    const auto count=static_cast<int>(text.size()+1);
    const String string{text.c_str(),count,count};
    alignas(16) Parameters args{};
    std::memcpy(args.data(),&string,sizeof(string));
    event(resolve(world_map_methods[5]),resolve(world_map_methods[6]),args.data());
    return U::field<U::NameKey>(args.data(),16);
}
static bool run_world_progress_stage(Address engine){
    static unsigned stage;
    static U::NameKey shared{},private_name{},native_name{},new_name{};
    static std::array<U::WeakKey,2> books{};
    static std::vector<WorldProgressTestInventory> inventory;
    if(stage==4)return all_player_markers(L"world-progress-done.txt");
    const auto gi=object_field(engine,"GameInstance");
    const auto save=gi?object_field(gi,"ActiveSaveSlot"):0;
    if(!save)return false;
    const auto actors=test_world_map(save,"SavableActorStates"),names=test_world_map(save,"NamedSaveData");
    const auto owned=test_world_map(save,"InventoryContents");
    const auto items=test_world_map(save,"SavableActorItems");
    if(!actors.field||!names.field||!owned.field||!items.field)return false;
    constexpr TestWP::Guid public_guid{33339801,-9162303,423901,11293},guest_guid{33218901,-625023,579023,2171};
    const auto& private_guid=TestWP::personal_guids[0];
    const auto personal_value=static_cast<int>(1000+role_index);
    constexpr TestWP::Guid null_guid{33218902,-625023,579023,2171};
    constexpr std::array paths{
        "/Game/Items/KeyItems/BookKeys/Alchemy/BookKey_00_Alchemy_Item.BookKey_00_Alchemy_Item",
        "/Game/Items/KeyItems/BookKeys/Dragons/BookKey_00_Dragons_Item.BookKey_00_Dragons_Item"
    };
    for(unsigned i=0;i<books.size();++i)if(!resolve(books[i])){
        const auto item=load_test_asset(paths[i]);
        if(!item)return false;
        remember(books[i],item);
    }
    const auto alchemy=resolve(books[0]),dragons=resolve(books[1]);
    const Address personal_item=host?0:role_index==2?dragons:alchemy;
    const auto set_item=[&](const auto& key,std::optional<Address> value){test_world_put_value(items,key,value);};
    const auto get_item=[&](const auto& key){return test_world_find<Address>(items,key);};
    if(stage==0){
        if(at<Address>(resolve(world_map_methods[0]),216)!=image+0x2d7f640||
           at<Address>(resolve(world_map_methods[1]),216)!=image+0x2d7fb10||
           at<Address>(resolve(world_map_methods[2]),216)!=image+0x2d7ff70||
           !TestWP::read_sparse_map<WorldProgressTestInventory,24>(owned.data,read_local,inventory))return false;
        shared=test_world_name(L"QDCoopNativeWorldFixtureShared");
        private_name=test_world_name(L"QDCoopPersonal~M40Fixture");
        native_name=test_world_name(L"QDCOOPNativeRescueV1_M40Fixture");
        new_name=test_world_name(L"QDCoopNativeWorldFixtureNew");
        if(!shared.index||!private_name.index||!native_name.index||!new_name.index)return false;
        test_world_put(actors,private_guid,personal_value);
        test_world_put(names,private_name,personal_value+1);
        test_world_put(names,native_name,personal_value+2);
        set_item(private_guid,personal_item);
        if(host){
            test_world_put(actors,public_guid,7);
            test_world_put(names,shared,-9);
            set_item(public_guid,alchemy);
            set_item(null_guid,Address{});
        }else{
            test_world_put(actors,guest_guid,111);
            set_item(guest_guid,dragons);
        }
        write_file(marker(role_index,L"world-progress-seeded.txt").c_str(),"ok",2);
        ++stage;
        return false;
    }
    std::vector<WorldProgressTestInventory> current;
    if(!TestWP::read_sparse_map<WorldProgressTestInventory,24>(owned.data,read_local,current)||current!=inventory||
       test_world_find(actors,private_guid)!=personal_value||test_world_find(names,private_name)!=personal_value+1||
       test_world_find(names,native_name)!=personal_value+2||get_item(private_guid)!=personal_item)return false;
    if(stage==1){
        if(!all_player_markers(L"world-progress-seeded.txt")||test_world_find(actors,public_guid)!=7||
           test_world_find(names,shared)!=-9||test_world_find(actors,guest_guid)||
           get_item(public_guid)!=alchemy||get_item(null_guid)!=Address{}||get_item(guest_guid))return false;
        write_file(marker(role_index,L"world-progress-added.txt").c_str(),"ok",2);
        ++stage;
    }else if(stage==2){
        if(!all_player_markers(L"world-progress-added.txt"))return false;
        static bool changed;
        if(host&&!changed){
            test_world_put(actors,public_guid,-17);
            test_world_put(names,shared,std::nullopt);
            test_world_put(names,new_name,35);
            set_item(public_guid,dragons);
            set_item(null_guid,alchemy);
            changed=true;
        }
        if(test_world_find(actors,public_guid)!=-17||test_world_find(names,shared)||test_world_find(names,new_name)!=35||
           get_item(public_guid)!=dragons||get_item(null_guid)!=alchemy)
            return false;
        write_file(marker(role_index,L"world-progress-changed.txt").c_str(),"ok",2);
        ++stage;
    }else if(stage==3){
        if(!all_player_markers(L"world-progress-changed.txt"))return false;
        static bool removed;
        if(!removed){
            test_world_put(actors,public_guid,host?std::nullopt:std::optional<int>{55});
            test_world_put(names,new_name,host?std::nullopt:std::optional<int>{77});
            set_item(public_guid,host?std::nullopt:std::optional<Address>{alchemy});
            set_item(null_guid,host?std::nullopt:std::optional<Address>{dragons});
            removed=true;
        }
        if(test_world_find(actors,public_guid)||test_world_find(names,new_name)||get_item(public_guid)||get_item(null_guid))return false;
        constexpr char item_proof[]="stock object map add, replace, null and remove; personal object and inventory preserved";
        write_file(marker(role_index,L"world-progress-items.txt").c_str(),item_proof,sizeof(item_proof)-1);
        write_file(marker(role_index,L"world-progress-done.txt").c_str(),"ok",2);
        ++stage;
    }
    return false;
}
