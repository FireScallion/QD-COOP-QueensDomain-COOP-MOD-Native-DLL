namespace PI=qdcoop::placed_item;
constexpr std::string_view source_herb="/Game/Items/Consumables/Herb/HealingHerb.HealingHerb";
int (*queue_source)(void*){};
UE::NameKey source_escrow{},source_room{};
ULONGLONG next_source{};
bool source_queued{};
bool source_opened_menu{},source_opened_inspect{},source_ui_verified{};
UE::WeakKey source_inventory_menu{},source_inspect{},source_button{};

Address source_method(Address object,std::string_view path){
    const auto expected=path.substr(path.rfind('.')+1);
    auto type=object?at<Address>(object,16):0;
    for(unsigned depth=0;type&&depth<32;type=at<Address>(type,64),++depth){
        unsigned checked{};
        for(auto child=at<Address>(type,72);child&&checked<2048;child=at<Address>(child,40),++checked)
            if(at<Address>(child,32)==type&&leaf(child)==expected&&path_is(child,path))return child;
    }
    return 0;
}

bool source_bool(Address object,const char* name){
    const auto field=ui.member(object,name,"BoolProperty",1);
    return field&&UE::get_bool(reinterpret_cast<void*>(field),reinterpret_cast<void*>(object),0);
}

bool source_click_ui(Address instance,ULONGLONG time){
    Address pawn[]{instance,0,0};
    TitleUi::event(ui.get(TitleUi::gameplay),ui.get(TitleUi::local_pawn),pawn);
    if(!pawn[2])return false;
    const auto game_menu=ui.object(pawn[2],"CurrentGameMenu");
    if(!source_opened_menu&&(!game_menu||!ui.shown(game_menu))){
        const auto input=source_method(pawn[2],"/Game/Player/FS_PlayerChar.FS_PlayerChar_C.InpActEvt_Status_K2Node_InputActionEvent_2");
        const auto field=input?UE::find_property(input,image+TitleUi::name_pool,"Key",read_local):0;
        const auto width=field?at<unsigned>(field,60):0;
        const auto type=field&&ui.kind(field,"StructProperty")?at<Address>(field,0x78):0;
        if(!width||width>32||!type||!path_is(type,"/Script/InputCore.Key")||at<unsigned>(type,88)!=width||
           !ui.method(input,width,{{"Key","StructProperty",0,width}})){
            if(auto stream=_wfopen(file(L"host-source-input-state.txt").c_str(),L"wb")){
                std::fprintf(stream,"input=%u key_width=%u key_type=%u pawn=%s existing_menu=%u\n",input!=0,width,type!=0,
                    leaf(at<Address>(pawn[2],16)).c_str(),game_menu!=0);
                std::fclose(stream);
            }
            return false;
        }
        alignas(16) std::array<unsigned char,32> key{};
        TitleUi::event(pawn[2],input,key.data());
        source_opened_menu=true;
        mark(L"source-stock-input");
        return false;
    }
    const auto modal=ui.object(game_menu,"FS_InspectItem");
    if(!modal||!path_is(at<Address>(game_menu,16),"/Game/UI/Menus/GameMenu.GameMenu_C")||
       !path_is(at<Address>(modal,16),"/Game/UI/Widgets/FS_InspectItem.FS_InspectItem_C")){
        if(auto stream=_wfopen(file(L"host-source-inspect-state.txt").c_str(),L"wb")){
            std::fprintf(stream,"menu=%s modal=%s\n",game_menu?leaf(at<Address>(game_menu,16)).c_str():"None",
                modal?leaf(at<Address>(modal,16)).c_str():"None");
            std::fclose(stream);
        }
        return false;
    }
    if(!source_opened_inspect){
        const auto open=source_method(modal,"/Game/UI/Widgets/FS_InspectItem.FS_InspectItem_C.OpenForItem");
        if(!ui.method(open,8,{{"ItemData","ObjectProperty",0,8}}))return false;
        auto item=ui.get(herb);
        TitleUi::event(modal,open,&item);
        source_opened_inspect=true;
        mark(L"source-stock-inspect");
        return false;
    }
    if(!source_bool(modal,"IsOpen")||ui.object(modal,"CurrentItem")!=ui.get(herb))return false;
    const auto box=ui.object(modal,"ConsumableActions");
    const auto button=named_button(box,ui.chinese()?L"放置":L"Place");
    const auto method=ui.get(TitleUi::click);
    if(!button||ui.parent(button)!=box||button==ui.object(modal,"UseButton")||
       !method||at<Address>(method,216)==image+0x11a7e40||!lock_suspend_saves(time))return false;
    ui.remember(source_inventory_menu,game_menu);
    ui.remember(source_inspect,modal);
    ui.remember(source_button,button);
    TitleUi::event(button,method,nullptr);
    TitleUi::event(button,method,nullptr);
    mark(L"source-stock-click");
    return true;
}

void source_check_ui(){
    const auto modal=ui.get(source_inspect),button=ui.get(source_button),method=ui.get(TitleUi::click);
    const bool open=modal&&source_bool(modal,"IsOpen");
    const bool attached=button&&ui.parent(button);
    const bool restored=method&&at<Address>(method,216)==image+0x11a7e40;
    if(auto stream=_wfopen(file(L"host-source-close-state.txt").c_str(),L"wb")){
        std::fprintf(stream,"modal=%u open=%u button=%u attached=%u callback=%u\n",modal!=0,open,button!=0,attached,restored);
        std::fclose(stream);
    }
    if(open||attached||!restored)return;
    source_ui_verified=true;
    mark(L"source-stock-restored");
    const auto game_menu=ui.get(source_inventory_menu);
    const auto close=source_method(game_menu,"/Game/UI/Menus/GameMenu.GameMenu_C.Close");
    if(ui.method(close,0,{}))TitleUi::event(game_menu,close,nullptr);
}

bool source_inventory(const SaveMap& items){
    if(!ui.get(herb))if(const auto item=ui.load_asset_path(source_herb))ui.remember(herb,item);
    const auto item=ui.get(herb);
    if(!item||!inventory(items,inventory_before))return false;
    if(!lan.host)return expected_inventory();
    auto selected=std::ranges::find(inventory_before,item,&InventoryEntry::item);
    if(selected==inventory_before.end()||selected->count<1){
        // The copied test save can lack herbs. Set its starting inventory
        // before sampling it, then let production code perform the debit.
        const int quantity=2;
        const auto add=reinterpret_cast<void(*)(Address,Address,const void*,const void*)>(image+0x2755ec0);
        add(items.data,items.field,&item,&quantity);
        mark(L"source-seeded-inventory");
        if(!inventory(items,inventory_before))return false;
        selected=std::ranges::find(inventory_before,item,&InventoryEntry::item);
        if(selected==inventory_before.end()||selected->count!=quantity)return false;
    }
    const auto offset=selected-inventory_before.begin();
    inventory_expected=inventory_before;
    if(!--inventory_expected[offset].count)inventory_expected.erase(inventory_expected.begin()+offset);
    return true;
}

bool source_record(const SaveMap& names){
    struct Entry {UE::NameKey key;int value;};
    std::vector<Entry> entries;
    if(!WP::read_sparse_map<Entry,20>(names.data,read_local,entries))return false;
    for(const auto& entry:entries){
        std::string key;
        if(!UE::name_string(image+TitleUi::name_pool,entry.key,read_local,key)||!PI::prefix(key,PI::drop_prefix))continue;
        const auto raw=std::string_view(key).substr(PI::drop_prefix.size());
        PI::Drop drop;
        if(!PI::decode(raw,drop)||!WP::name_equal(drop.item,source_herb)||
           drop.id.size()!=32||drop.owner.size()!=32)return false;
        std::ranges::transform(drop.position,expected_position.begin(),[](auto value){return static_cast<float>(value);});
        const auto make=[&](std::string_view text){return name(std::wstring(text.begin(),text.end()));};
        source_escrow=make(std::string(PI::escrow_prefix)+std::string(raw));
        source_room=make(std::string(PI::room_prefix)+drop.room);
        receipt=make("QDCoopPersonal~got~"+drop.id);
        if(!source_escrow.index||!source_room.index||!receipt.index)return false;
        keys[0]=entry.key;
        return true;
    }
    return false;
}

bool source_step(Address instance,const SaveMap& names,const SaveMap& items,unsigned actors,ULONGLONG time){
    diagnostics(names,actors,time);
    if(!stage){
        qdcoop::rescue::BindingView binding{};
        if(actors||!read_medic||!read_medic(&binding)||binding.phase!=2||!source_inventory(items))return false;
        stage=1;
        mark(L"prepared");
    }
    if(lan.host&&stage==1&&!source_queued&&all(L"prepared")&&time>=next_source){
        next_source=time+300;
        if(source_ui_mode){
            if(source_click_ui(instance,time)){
                source_queued=true;
                mark(L"source-queued");
            }
        }else{
            if(!queue_source)if(const auto dll=GetModuleHandleW(L"QDCoopNativeM1.dll"))
                load_export(dll,"QDEnemyQueueNativePlacedItem",queue_source);
            if(!queue_source||!lock_suspend_saves(time))return false;
            const auto item=reinterpret_cast<void*>(ui.get(herb));
            if(queue_source(item)){
                source_queued=true;
                mark(L"source-queued");
                if(queue_source(item))mark(L"source-duplicate-accepted");
            }
        }
    }
    if(lan.host&&source_ui_mode&&source_queued&&!source_ui_verified)source_check_ui();
    if(lan.host&&save_locks[0]&&time>=save_unlock_at){
        mark(L"save-unlocked");
        release_save_locks();
    }
    if(!lan.host&&!seen(L"host-save-unlocked.txt")&&(actors||keys[0].index||source_record(names)))mark(L"save-premature");
    if(!keys[0].index&&!source_record(names))return false;
    if(lan.host&&save_locks[0]&&get(names,source_escrow)==1)mark(L"source-debit-before-publish");
    const auto ground=get(names,keys[0]);
    if(stage==1&&ground==0&&actors==1&&get(names,source_room)==1&&geometry(ui.get(visual))&&
       (!lan.host||!source_ui_mode||source_ui_verified)){
        std::vector<InventoryEntry> current;
        if(!inventory(items,current)||current!=(lan.host?inventory_expected:inventory_before)||
           (lan.host?get(names,source_escrow)!=1:get(names,source_escrow).has_value()))return false;
        if(lan.host){
            const auto stream=_wfopen(file(L"placed-pickup-origin.bin").c_str(),L"wb");
            if(!stream)return false;
            const auto written=std::fwrite(expected_position.data(),sizeof(float),expected_position.size(),stream);
            std::fclose(stream);
            if(written!=expected_position.size())return false;
        }
        stage=2;
        mark(L"rendered");
    }
    if(lan.role==L"client"&&stage==2&&ground==0&&!claimed&&all(L"rendered")&&click(instance,ui.get(visual),time)){
        claimed=true;
        mark(L"attempted");
    }
    if(stage==2&&ground==1&&!seen(lan.role+L"-reserved.txt"))mark(L"reserved");
    const bool published=!lan.host||(seen(L"client-retired.txt")&&seen(L"client2-retired.txt")&&seen(L"client3-retired.txt"));
    if(stage==2&&!actors&&ground==2&&published){
        const auto got=get(names,receipt);
        const bool won=!lan.host&&got==1;
        std::vector<InventoryEntry> current;
        if(!inventory(items,current)||current!=(lan.host||won?inventory_expected:inventory_before)||
           (won?got!=1:got.has_value())||get(names,source_room)!=1||
           (lan.host?get(names,source_escrow)!=1:get(names,source_escrow).has_value()))return false;
        if(won)mark(L"winner");
        mark(L"retired");
        mark(L"clean");
        stage=4;
        next_report=0;
        diagnostics(names,actors,time);
    }
    return stage==4&&all(L"clean");
}
