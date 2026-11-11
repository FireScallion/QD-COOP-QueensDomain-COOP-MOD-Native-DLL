struct LibraryBookValue {
    WP::Guid key;
    std::optional<std::int32_t> state;
    std::optional<std::string> item;
    bool operator==(const LibraryBookValue&) const=default;
};
struct LibraryBook {
    Weak actor,box;
    std::vector<LibraryBookValue> applied;
    std::uint8_t collision{};
    bool disabled{},restored{};
};
struct NativeLibrary {
    Weak book_type,gate_type,box_type;
    MedicMap guids,cache;
    void *disabled{},*quest_flag{};
    WorldArrayClear clear{};
    std::vector<LibraryBook> books;
    std::vector<Weak> gates;
    WP::Revision revision;
    WP::Streaming streams;
    std::size_t cursor{};
    double next_scan{},discover_until{};
    int box_at{};
};
static NativeLibrary native_library;

static bool library_book_bind(void* type){
    auto& state=native_library;
    if(state.clear&&type&&api.weakget(&state.book_type)==type)return true;
    const auto restore=world_input(world_actor_restore);
    const auto clear=world_actor_array_clear();
    if(!type||!restore||*api.parms(restore)||AnimU::field<int>(restore,104)!=40||!clear||
       !medic_params(world_actor_collision,1,{{"ReturnValue",1,0}})||
       !medic_params(world_component_collision,1,{{"NewType",1,0}}))return false;
    const auto guids=world_actor_array(type,"GUIDs",16,"/Script/CoreUObject.Guid");
    const auto cache=world_actor_array(type,"MySaveData",8,"/Game/Blueprints/SaveSystem/SavableActorState.SavableActorState");
    const auto disabled=catalog_field(type,"InteractIsDisabled",1),box=catalog_field(type,"InteractBox",8);
    if(!guids.field||!cache.field||!medic_field_class(disabled,"BoolProperty")||!medic_field_class(box,"ObjectProperty"))return false;
    const auto box_type=AnimU::field<void*>(box,0x78);
    std::string path;
    if(!catalog_path(box_type,path)||path!="/Script/Engine.BoxComponent"||
       !makeweak(state.book_type,type)||!makeweak(state.box_type,box_type))return false;
    state.disabled=disabled;
    state.box_at=*api.offset(box);
    state.guids=guids;
    state.cache=cache;
    state.clear=clear;
    return true;
}
static std::uint8_t library_collision(void* box){
    std::uint8_t mode=255;
    if(const auto get=world_input(world_actor_collision))api.event(box,get,&mode);
    return mode;
}
static void library_set_collision(void* box,std::uint8_t mode){
    if(const auto set=world_input(world_component_collision))api.event(box,set,&mode);
}
static void library_lock_book(LibraryBook& book,void* actor,void* box){
    const auto flag=native_library.disabled;
    if(!api.getbool(flag,actor,0)){
        book.disabled=false;
        api.setbool(flag,actor,true,0);
    }
    const auto mode=library_collision(box);
    if(mode&&mode<=3){
        book.collision=mode;
        library_set_collision(box,0);
    }
}
static bool library_release_book(const LibraryBook& book){
    const auto actor=api.weakget(&book.actor),box=api.weakget(&book.box);
    const auto disabled=native_library.disabled;
    if(actor&&disabled&&api.getbool(disabled,actor,0))api.setbool(disabled,actor,book.disabled,0);
    const auto collision=box?library_collision(box):0;
    if(box&&book.collision&&collision>3)return false;
    if(box&&book.collision&&!collision){
        library_set_collision(box,book.collision);
        return library_collision(box)==book.collision;
    }
    return true;
}
static bool world_library_cleanup(){
    auto& state=native_library;
    bool clean=true;
    std::erase_if(state.books,[&](const auto& book){
        if(library_release_book(book))return true;
        clean=false;
        return false;
    });
    for(const auto& key:state.gates)
        if(const auto actor=api.weakget(&key))api.setbool(state.quest_flag,actor,true,0);
    state.gates.clear();
    if(clean)state={};
    return clean;
}
static void library_scan_books(void* pawn,void* type){
    auto& state=native_library;
    if(!library_book_bind(type))return;
    std::erase_if(state.books,[](const auto& book){return !api.weakget(&book.actor);});
    const auto box_type=api.weakget(&state.box_type);
    NativeActorArray actors{};
    native_get_actors(pawn,type,&actors);
    if(actors.data&&actors.count>=0&&actors.count<=256&&actors.capacity>=actors.count&&box_type)
        for(int i=0;i<actors.count;++i){
            const auto actor=actors.data[i];
            if(!actor||AnimU::field<void*>(actor,16)!=type||actor_is_destroying(actor))continue;
            const auto box=AnimU::field<void*>(actor,state.box_at);
            if(!box||!api.isa(box,box_type))continue;
            const auto found=std::ranges::find_if(state.books,[&](const auto& lease){return api.weakget(&lease.actor)==actor;});
            if(found!=state.books.end()){
                if(api.weakget(&found->box)==box)library_lock_book(*found,actor,box);
                continue;
            }
            LibraryBook book{};
            book.disabled=api.getbool(state.disabled,actor,0);
            book.collision=library_collision(box);
            if(book.collision>3||!makeweak(book.actor,actor)||!makeweak(book.box,box))continue;
            library_lock_book(book,actor,box);
            state.books.push_back(std::move(book));
            ++world_library_locks;
        }
    if(actors.data)native_free(actors.data);
}
static void library_scan_gates(void* pawn,void* type){
    auto& state=native_library;
    if(!type)return;
    if(api.weakget(&state.gate_type)!=type||!state.quest_flag){
        const auto flag=catalog_field(type,"SetPuzzleNSD",1);
        if(!medic_field_class(flag,"BoolProperty")||!makeweak(state.gate_type,type))return;
        state.quest_flag=flag;
    }
    std::erase_if(state.gates,[](const auto& gate){return !api.weakget(&gate);});
    NativeActorArray actors{};
    native_get_actors(pawn,type,&actors);
    if(actors.data&&actors.count>=0&&actors.count<=128&&actors.capacity>=actors.count)
        for(int i=0;i<actors.count;++i){
            const auto actor=actors.data[i];
            if(!actor||AnimU::field<void*>(actor,16)!=type||!api.getbool(state.quest_flag,actor,0))continue;
            if(std::ranges::none_of(state.gates,[&](const auto& gate){return api.weakget(&gate)==actor;})){
                Weak key{};
                if(!makeweak(key,actor))continue;
                state.gates.push_back(key);
                ++world_library_gate_locks;
            }
            api.setbool(state.quest_flag,actor,false,0);
        }
    if(actors.data)native_free(actors.data);
}
static void library_restore_book(LibraryBook& book){
    const auto actor=api.weakget(&book.actor),box=api.weakget(&book.box),restore=world_input(world_actor_restore);
    const auto& state=native_library;
    if(!actor||!box||!restore||!state.clear)return;
    const auto guids=AnimU::field<WorldArray>(actor,state.guids.offset);
    if(!guids.valid(32)||!guids.count||!AnimU::field<WorldArray>(actor,state.cache.offset).valid(64))return;
    std::vector<LibraryBookValue> values;
    const auto& desired=world_progress.authoritative;
    const auto keys=static_cast<const WP::Guid*>(guids.data);
    for(int i=0;i<guids.count;++i){
        const auto& key=keys[i];
        if(WP::personal(key))return;
        LibraryBookValue entry{key,{},{}};
        const auto field=std::ranges::lower_bound(desired.actors,key,{},&WP::ActorState::key);
        const auto item=std::ranges::lower_bound(desired.items,key,{},&WP::ActorItemState::key);
        if(field!=desired.actors.end()&&field->key==key)entry.state=field->value;
        if(item!=desired.items.end()&&item->key==key)entry.item=item->path;
        values.push_back(std::move(entry));
    }
    if(book.restored&&values==book.applied)return;
    if(!library_release_book(book))return;
    state.clear(state.cache.address(actor),state.cache.field);
    api.event(actor,restore,nullptr);
    book.disabled=api.getbool(state.disabled,actor,0);
    const auto mode=library_collision(box);
    if(mode<=3)book.collision=mode;
    library_lock_book(book,actor,box);
    book.applied=std::move(values);
    book.restored=true;
    ++world_library_restores;
}
static void world_library_step(void* pawn,double time){
    auto& state=native_library;
    if(host_role||!active_peer_mask||!native_get_actors||!native_free)return;
    if(time>=state.next_scan){
        state.next_scan=time+.35;
        const auto& streams=world_progress.authoritative.streaming;
        if(state.streams!=streams){state.streams=streams;state.discover_until=time+4.;}
        if(time<state.discover_until&&(!world_input(world_library_book)||!world_input(world_library_gate)))
            scan_world_inputs(4096);
        const auto gate=world_input(world_library_gate),book=world_input(world_library_book);
        if(gate||book||!state.books.empty()||!state.gates.empty()){
            library_scan_gates(pawn,gate);
            library_scan_books(pawn,book);
            state.cursor=0;
        }
    }
    if(!world_progress_ready())return;
    if(state.revision!=world_progress.applied){
        state.revision=world_progress.applied;
        state.cursor=0;
    }
    for(unsigned i=0;i<2&&state.cursor<state.books.size();++i)library_restore_book(state.books[state.cursor++]);
}
