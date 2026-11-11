static Address test_world_method(Address type,std::string_view wanted){
    for(unsigned depth=0;type&&depth<128;++depth,type=at<Address>(type,64)){
        auto fn=at<Address>(type,72);
        for(unsigned i=0;fn&&i<1024;++i,fn=at<Address>(fn,40))if(name(fn)==wanted)return fn;
    }
    return 0;
}
static constexpr const char* chest_probe_path="/Game/RuinsLevel/Maps/100_GR.100_GR.PersistentLevel.Chest11";
static U::WeakKey chest_probe_actor;
static bool probe_world_actors(Address engine){
    static bool done;
    if(done)return true;
    const auto type=resolve(world_actor_methods[0]),fn=resolve(world_actor_methods[1]);
    const auto pawn=local_test_pawn();
    const auto gi=object_field(engine,"GameInstance"),save=gi?object_field(gi,"ActiveSaveSlot"):0;
    if(!type||!fn||!pawn||!save||at<std::uint16_t>(fn,182)!=32)return false;
    alignas(16) Parameters args{};
    if(!put(args,fn,"WorldContextObject",pawn)||!put(args,fn,"ActorClass",type))return false;
    event(resolve(gameplay),fn,args.data());
    struct ActorArray { Address* data; int count,capacity; };
    const auto actors=U::field<ActorArray>(args.data(),16);
    std::string report="live="+std::to_string(actors.count)+"\n";
    const auto base_type=type;
    for(const auto property:{"GUIDs","MySaveData"}){
        const auto field=U::find_inherited_property(base_type,image+0x4a27f00,property,read_local);
        if(!field)continue;
        const auto inner=at<Address>(field,0x78);
        std::string inner_path;
        if(inner&&world_map_property_type(inner)=="StructProperty")
            U::object_path(at<Address>(inner,0x78),image+0x4a27f00,read_local,inner_path);
        report+=std::string(property)+" offset="+std::to_string(at<int>(field,76))+" type="+
            world_map_property_type(field)+" size="+std::to_string(at<int>(field,60))+" inner="+
            world_map_property_type(inner)+" inner_size="+std::to_string(inner?at<int>(inner,60):0)+
            " struct="+inner_path+"\n";
    }
    const auto clear=resolve(world_actor_methods[2]);
    char entry[40]{};
    if(clear)std::snprintf(entry,sizeof(entry),"0x%llx",static_cast<unsigned long long>(at<Address>(clear,216)-image));
    report+="Array_Clear exec="+std::string(entry)+"\n";
    const auto guid_field=field_of(type,"GUIDs",16),map=test_world_map(save,"SavableActorStates").data;
    unsigned written{};
    if(actors.data&&actors.count>=0&&actors.count<=8192&&actors.capacity>=actors.count&&guid_field&&map){
        const auto offset=at<int>(guid_field,76);
        const auto saved=test_world_map(save,"SavableActorStates");
        for(int i=0;i<actors.count;++i){
            const auto actor=actors.data[i];
            if(!actor)continue;
            const auto actor_type=at<Address>(actor,16);
            const auto actor_name=name(actor_type);
            const bool interesting=actor_name.find("Door")!=std::string::npos||actor_name.find("Lever")!=std::string::npos||
                actor_name.find("Barricade")!=std::string::npos||actor_name.find("Secret")!=std::string::npos||
                actor_name.find("Chest")!=std::string::npos||actor_name.find("Elevator")!=std::string::npos||
                actor_name.find("Switch")!=std::string::npos||actor_name.find("Gate")!=std::string::npos;
            if(!interesting)continue;
            std::string path,method_path;
            U::object_path(actor,image+0x4a27f00,read_local,path);
            if(path==chest_probe_path)remember(chest_probe_actor,actor);
            if(written>=60)continue;
            const auto method=test_world_method(actor_type,"RestoreFromSave");
            const auto guids=at<ActorArray>(actor,offset);
            if(method)U::object_path(method,image+0x4a27f00,read_local,method_path);
            const auto point=location(actor);
            report+=path+" type="+actor_name+" restore="+method_path+" code="+
                    std::to_string(method?at<int>(method,104):0)+" params="+
                    std::to_string(method?at<std::uint16_t>(method,182):0)+" xyz="+
                    std::to_string(point[0])+","+std::to_string(point[1])+","+std::to_string(point[2])+" guids=";
            if(guids.data&&guids.count>0&&guids.count<=32&&guids.capacity>=guids.count){
                const auto keys=reinterpret_cast<const TestWP::Guid*>(guids.data);
                for(int g=0;g<guids.count;++g){
                    for(const auto part:keys[g])report+=std::to_string(part)+",";
                    const auto value=test_world_find(saved,keys[g]);
                    report+="="+(value?std::to_string(*value):"absent")+";";
                }
            }
            report+="\n";
            ++written;
        }
    }
    if(actors.data)reinterpret_cast<void(*)(void*)>(image+0xeae0b0)(actors.data);
    if(!written||((world_chest_lab||world_chest_probe)&&!resolve(chest_probe_actor)))return false;
    write_file(marker(role_index,L"world-actor-layout.txt").c_str(),report.data(),static_cast<unsigned>(report.size()));
    done=true;
    return true;
}
static bool probe_stock_chest(Address engine){
    static bool done;
    if(done)return true;
    const auto chest=resolve(chest_probe_actor),gi=object_field(engine,"GameInstance");
    const auto save=gi?object_field(gi,"ActiveSaveSlot"):0;
    if(!chest||!save)return false;
    const auto type=at<Address>(chest,16);
    const auto restore=test_world_method(type,"RestoreFromSave");
    const auto data=field_of(type,"MySaveData",16),guids=field_of(type,"GUIDs",16);
    const auto box=object_field(chest,"InteractBox"),lid=object_field(chest,"OpenLid");
    const auto collision=resolve(world_actor_methods[3]),position=resolve(world_actor_methods[4]);
    if(!restore||!data||!guids||!box||!lid||!collision||!position||at<int>(restore,104)!=40||
       at<std::uint16_t>(restore,182)||at<std::uint16_t>(collision,182)!=1||at<std::uint16_t>(position,182)!=4)return false;
    std::string report;
    for(unsigned depth=0;depth<4&&type;++depth){
        auto cls=type;
        for(unsigned i=0;i<depth;++i)cls=at<Address>(cls,64);
        if(!cls)break;
        auto fn=at<Address>(cls,72);
        for(unsigned i=0;fn&&i<1024;++i,fn=at<Address>(fn,40)){
            const auto leaf=name(fn);
            if(leaf.find("Save")==std::string::npos&&leaf.find("Open")==std::string::npos&&
               leaf.find("Restore")==std::string::npos&&leaf.find("Interact")==std::string::npos)continue;
            report+=name(cls)+"."+leaf+" code="+std::to_string(at<int>(fn,104))+" params="+
                    std::to_string(at<std::uint16_t>(fn,182))+"\n";
        }
    }
    const auto save_at=at<int>(data,76),guid_at=at<int>(guids,76);
    const auto key_ptr=at<const TestWP::Guid*>(chest,guid_at);
    if(!key_ptr||at<int>(chest,guid_at+8)!=1)return false;
    const auto key=*key_ptr;
    const auto states=test_world_map(save,"SavableActorStates");
    const auto previous=test_world_find(states,key);
    std::uint8_t before_collision{},after_collision{};
    float before{},after{};
    event(box,collision,&before_collision);
    event(lid,position,&before);
    report+="original="+(previous?std::to_string(*previous):"absent")+" cache="+
             std::to_string(at<int>(chest,save_at+8))+"\n";
    test_world_put(states,key,1);
    const int zero=0;
    std::memcpy(reinterpret_cast<void*>(chest+save_at+8),&zero,sizeof(zero));
    event(chest,restore,nullptr);
    event(box,collision,&after_collision);
    event(lid,position,&after);
    report+="put1 before_collision="+std::to_string(before_collision)+" after_collision="+
             std::to_string(after_collision)+" before_pos="+std::to_string(before)+" after_pos="+std::to_string(after)+
             " cache="+std::to_string(at<int>(chest,save_at+8))+"\n";
    test_world_put(states,key,previous);
    std::memcpy(reinterpret_cast<void*>(chest+save_at+8),&zero,sizeof(zero));
    event(chest,restore,nullptr);
    std::uint8_t reverted_collision{};
    float reverted{};
    event(box,collision,&reverted_collision);
    event(lid,position,&reverted);
    report+="reverted_collision="+std::to_string(reverted_collision)+" reverted_pos="+std::to_string(reverted)+"\n";
    write_file(marker(role_index,L"stock-chest-restore.txt").c_str(),report.data(),static_cast<unsigned>(report.size()));
    done=true;
    return true;
}
static bool run_world_chest_stage(Address engine){
    static bool ready,opened,verified;
    static std::vector<WorldProgressTestInventory> inventory;
    if(verified)return all_player_markers(L"world-chest-open.txt");
    if(!probe_world_actors(engine))return false;
    const auto chest=resolve(chest_probe_actor),gi=object_field(engine,"GameInstance");
    const auto save=gi?object_field(gi,"ActiveSaveSlot"):0;
    if(!chest||!save)return false;
    const auto type=at<Address>(chest,16);
    const auto data=field_of(type,"MySaveData",16),guids=field_of(type,"GUIDs",16);
    const auto box=object_field(chest,"InteractBox"),lid=object_field(chest,"OpenLid");
    const auto restore=test_world_method(type,"RestoreFromSave");
    const auto collision=resolve(world_actor_methods[3]),position=resolve(world_actor_methods[4]);
    if(!data||!guids||!box||!lid||!restore||!collision||!position)return false;
    const auto guid_at=at<int>(guids,76);
    const auto key_ptr=at<const TestWP::Guid*>(chest,guid_at);
    if(!key_ptr||at<int>(chest,guid_at+8)!=1||TestWP::personal(*key_ptr))return false;
    const auto& key=*key_ptr;
    const auto states=test_world_map(save,"SavableActorStates"),owned=test_world_map(save,"InventoryContents");
    std::uint8_t collides{};
    float pos{};
    event(box,collision,&collides);
    event(lid,position,&pos);
    if(!ready){
        if(!collides||pos!=0.f||test_world_find(states,key)||
           !TestWP::read_sparse_map<WorldProgressTestInventory,24>(owned.data,read_local,inventory))return false;
        ready=true;
        write_file(marker(role_index,L"world-chest-closed.txt").c_str(),"ok",2);
    }
    if(!all_player_markers(L"world-chest-closed.txt"))return false;
    if(host&&!opened){
        test_world_put(states,key,1);
        const auto clear=reinterpret_cast<void(*)(Address,Address)>(image+0x297d910);
        clear(chest+at<int>(data,76),data);
        event(chest,restore,nullptr);
        opened=true;
        return false;
    }
    std::vector<WorldProgressTestInventory> current;
    if(collides||test_world_find(states,key)!=1||(!host&&pos<.04f)||
       !TestWP::read_sparse_map<WorldProgressTestInventory,24>(owned.data,read_local,current)||current!=inventory)return false;
    char report[120]{};
    const auto size=std::snprintf(report,sizeof(report),"shared=1 collision=%u lid=%.4f inventory_items=%zu\n",
                                  static_cast<unsigned>(collides),pos,current.size());
    if(size>0)write_file(marker(role_index,L"world-chest-open.txt").c_str(),report,static_cast<unsigned>(size));
    verified=size>0;
    return verified&&all_player_markers(L"world-chest-open.txt");
}
