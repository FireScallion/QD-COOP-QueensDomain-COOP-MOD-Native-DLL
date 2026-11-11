static std::string world_map_property_type(Address property){
    std::string name;
    const auto type=property?at<Address>(property,8):0;
    if(type)U::name_string(image+0x4a27f00,at<U::NameKey>(type,0),read_local,name);
    return name;
}
static bool probe_world_maps(Address engine){
    static bool done;
    if(done)return true;
    const auto gi=object_field(engine,"GameInstance");
    const auto save=gi?object_field(gi,"ActiveSaveSlot"):0;
    if(!save)return false;
    const auto type=at<Address>(save,16);
    std::string report;
    constexpr std::string_view maps[]{"SavableActorStates","SavableActorItems","NamedSaveData","InventoryContents"};
    for(const auto name:maps){
        const auto field=field_of(type,name.data(),80);
        if(!field||world_map_property_type(field)!="MapProperty")return false;
        const auto key=at<Address>(field,0x78),value=at<Address>(field,0x80);
        const auto shape=at<std::array<int,6>>(field,0x88);
        const auto offset=at<int>(field,76);
        if(!key||!value||offset<0)return false;
        report+=name;
        report+=" offset="+std::to_string(offset)+" key="+world_map_property_type(key)+
                "/"+std::to_string(at<int>(key,60))+" value="+world_map_property_type(value)+
                "/"+std::to_string(at<int>(value,60))+" layout=";
        for(const auto item:shape)report+=std::to_string(item)+",";
        const auto address=save+offset;
        report+=" sparse="+std::to_string(at<int>(address,8))+" free="+std::to_string(at<int>(address,52));
        if(world_map_property_type(key)=="StructProperty"){
            std::string path;
            if(!U::object_path(at<Address>(key,0x78),image+0x4a27f00,read_local,path))return false;
            report+=" struct="+path;
        }
        report+="\n";
    }
    for(unsigned i=0;i<5;++i){
        const auto fn=resolve(world_map_methods[i]);
        if(!fn)return false;
        char hex[24]{};
        std::snprintf(hex,sizeof(hex),"0x%llx",static_cast<unsigned long long>(at<Address>(fn,216)-image));
        report+=std::string(world_map_paths[i])+" exec="+hex+" parms="+std::to_string(at<std::uint16_t>(fn,182))+"\n";
    }
    write_file(marker(role_index,L"world-map-layout.txt").c_str(),report.data(),static_cast<unsigned>(report.size()));
    done=true;
    return true;
}
