struct PlacedSaveMarker {
    AnimU::NameKey key{};
    int value{};
};

enum class PlacedSaveStage { idle,waiting,reading,verifying,proved };
struct PlacedSaveProof {
    Weak origin;
    std::array<std::wstring,2> slots;
    std::array<PlacedSaveMarker,4> markers{};
    std::string item;
    Medic::SaveProof hash{};
    unsigned count{},slot{};
    int quantity{-1};
    double next{},retry_save{};
    PlacedSaveStage stage{};
};
static PlacedSaveProof placed_save;
static Medic::StorageWorker placed_save_worker;
struct PlacedParkedSave {
    PlacedSaveProof proof;
    PlacedOperation operation{};
};
static std::vector<PlacedParkedSave> placed_parked_saves;

static bool placed_suspend_paths(const MedicSave& binding,std::array<std::wstring,2>& slots){
    const auto utility=world_input(world_medic_utils),name=world_input(world_medic_save_name);
    if(!utility||!name||!binding.primary.ends_with(L".sav")||!native_free)return false;
    const auto stem=binding.primary.substr(0,binding.primary.size()-4);
    for(unsigned i=0;i<slots.size();++i){
        alignas(16) FactoryParameters args{};
        factory_put(args,0,binding.slot_index);
        args[6]=1;
        factory_put(args,8,i);
        api.event(utility,name,args.data());
        const auto returned=AnimU::field<MedicString>(args.data(),16);
        std::wstring actual;
        if(returned.data&&returned.size>0&&returned.size<=2048&&returned.capacity>=returned.size&&
           returned.data[returned.size-1]==0)actual.assign(returned.data,static_cast<unsigned>(returned.size-1));
        if(returned.data)native_free(const_cast<wchar_t*>(returned.data));
        const auto path=stem+L"_Suspend_"+std::to_wstring(i)+L".sav";
        std::replace(actual.begin(),actual.end(),L'/',L'\\');
        const auto expected=L"FULLPATH:"+path;
        if(actual.size()!=expected.size()||_wcsicmp(actual.c_str(),expected.c_str()))return false;
        slots[i]=path;
    }
    return true;
}

static bool placed_save_prepare(void* pawn,std::span<const PlacedSaveMarker> markers,std::string_view item,
                                int quantity,PlacedSaveProof& proof){
    const auto& binding=medic_runtime.save;
    const auto gi=api.weakget(&binding.gi);
    const auto type=gi?AnimU::field<void*>(gi,16):nullptr;
    const auto blocked=catalog_field(type,"SuspendIsBlocked",4);
    const auto writing=placed_item_bool(type,"SuspendSaveCurrentlyWriting");
    const auto active=medic_active(binding,pawn);
    placed_pickup_stage=50;
    if(placed_save.stage!=PlacedSaveStage::idle||markers.empty()||markers.size()>proof.markers.size()||
       !active||!checkpoint_medic_idle()||!makeweak(proof.origin,active))return false;
    placed_pickup_stage=51;
    if(!medic_field_class(blocked,"IntProperty")||AnimU::field<int>(gi,*api.offset(blocked))||
       !writing||api.getbool(writing,gi,0))return false;
    placed_pickup_stage=52;
    if(!medic_params(world_placed_suspend,1,{{"CancelSaveLoop",1,0}})||!placed_suspend_paths(binding,proof.slots))return false;
    placed_pickup_stage=53;
    if((quantity>=0)!=(!item.empty())||std::ranges::any_of(markers,[](const auto& marker){return !marker.key.index;}))return false;
    std::ranges::copy(markers,proof.markers.begin());
    proof.count=static_cast<unsigned>(markers.size());
    proof.item=item;
    proof.quantity=quantity;
    return true;
}

static void placed_save_begin(PlacedSaveProof proof,double time){
    placed_pickup_stage=60;
    placed_save=std::move(proof);
    placed_save.next=time+.15;
    placed_save.retry_save=time+1.;
    placed_save.stage=PlacedSaveStage::waiting;
    unsigned char cancel=1;
    ++placed_pickup_save_attempts;
    api.event(api.weakget(&medic_runtime.save.gi),world_input(world_placed_suspend),&cancel);
}

static bool placed_save_contents(void* saved){
    const auto& binding=medic_runtime.save;
    qdcoop::Id identity{};
    if(!saved||!medic_identity(binding,saved,identity)||identity!=medic_runtime.ledger.identity)return false;
    for(unsigned i=0;i<placed_save.count;++i){
        const auto& marker=placed_save.markers[i];
        int current{};
        if(!binding.find(binding.markers.address(saved),binding.markers.field,&marker.key,&current)||
           current!=marker.value)return false;
    }
    if(placed_save.quantity<0)return true;
    const auto item=placed_item_data(placed_save.item);
    int quantity{};
    const bool found=item&&binding.find(binding.inventory.address(saved),binding.inventory.field,&item,&quantity);
    return item&&(found||!placed_save.quantity)&&quantity==placed_save.quantity;
}

static void placed_save_retry(double time){
    placed_save.slot=1-placed_save.slot;
    placed_save.next=time+.15;
    placed_save.stage=PlacedSaveStage::waiting;
}

static void placed_save_poll(void* pawn,double time){
    auto& proof=placed_save;
    if(proof.stage==PlacedSaveStage::idle||proof.stage==PlacedSaveStage::proved)return;
    if(proof.stage==PlacedSaveStage::waiting){
        const auto& binding=medic_runtime.save;
        const auto active=medic_active(binding,pawn),gi=api.weakget(&binding.gi);
        if(active&&gi&&time>=proof.retry_save){
            proof.retry_save=time+1.;
            const auto writing=placed_item_bool(AnimU::field<void*>(gi,16),"SuspendSaveCurrentlyWriting");
            if(local_life.layout.suspend>=0&&!AnimU::field<int>(gi,local_life.layout.suspend)&&
               writing&&!api.getbool(writing,gi,0)){
                const auto item=proof.item.empty()?nullptr:placed_item_data(proof.item);
                const auto current=proof.item.empty()?-1:medic_count(binding,item);
                const auto previous=proof.quantity;
                if(proof.item.empty()||current>=0){
                    proof.quantity=current;
                    if(placed_save_contents(active)){
                        unsigned char cancel=1;
                        ++placed_pickup_save_attempts;
                        api.event(gi,world_input(world_placed_suspend),&cancel);
                    }else proof.quantity=previous;
                }
            }
        }
        if(time>=proof.next&&medic_active(medic_runtime.save,pawn)&&placed_save_worker.proof(proof.slots[proof.slot]))
            proof.stage=PlacedSaveStage::reading;
        return;
    }
    Medic::StorageResult result{};
    if(!placed_save_worker.poll(result))return;
    if(proof.stage==PlacedSaveStage::verifying&&result.ok&&result.proof==proof.hash){
        placed_pickup_stage=63;
        proof.stage=PlacedSaveStage::proved;
        return;
    }
    if(proof.stage==PlacedSaveStage::reading&&result.ok&&medic_active(medic_runtime.save,pawn)&&
       placed_save_contents(medic_load_snapshot(medic_runtime.save,proof.slots[proof.slot]))&&
       placed_save_worker.proof(proof.slots[proof.slot])){
        placed_pickup_stage=62;
        proof.hash=result.proof;
        proof.stage=PlacedSaveStage::verifying;
        return;
    }
    placed_pickup_stage=61;
    placed_save_retry(time);
}

static bool placed_save_cleanup(){
    if(placed_save.stage==PlacedSaveStage::reading||placed_save.stage==PlacedSaveStage::verifying){
        Medic::StorageResult result{};
        if(!placed_save_worker.poll(result))return false;
    }
    if(placed_save.stage!=PlacedSaveStage::idle&&
       (placed_operation==PlacedOperation::reserve||placed_operation==PlacedOperation::settle||
        placed_operation==PlacedOperation::reopen||placed_operation==PlacedOperation::place||
        placed_operation==PlacedOperation::register_drop)&&api.weakget(&placed_save.origin)){
        // Closing the room can leave the same ActiveSaveSlot alive. Keep an
        // unpublished custody change fenced until its suspend proof completes.
        placed_parked_saves.push_back({std::move(placed_save),placed_operation});
    }
    placed_save={};
    return true;
}

static void native_placed_item_save_resume(void* save,double time){
    if(placed_save.stage!=PlacedSaveStage::idle||placed_parked_saves.empty())return;
    std::erase_if(placed_parked_saves,[](auto& entry){return !api.weakget(&entry.proof.origin);});
    const auto found=std::ranges::find_if(placed_parked_saves,[&](auto& entry){
        return api.weakget(&entry.proof.origin)==save;
    });
    if(found==placed_parked_saves.end())return;
    placed_save=std::move(found->proof);
    placed_operation=found->operation;
    placed_parked_saves.erase(found);
    placed_save.stage=PlacedSaveStage::waiting;
    placed_save.next=placed_save.retry_save=time;
}
