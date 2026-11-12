namespace TitleUi {
struct PlaceMenu {
    UE::WeakKey game_menu{},inspect{},button{},pending_item{};
    UE::WeakKey game_type{},inspect_type{},close{},close_internal{},update{},equipment{};
    std::uint64_t token{};
    ULONGLONG next{},pending_until{},failure_until{};
    int enabled{-1};

    bool type(Address object,UE::WeakKey& key,std::string_view path) {
        if(!object)return false;
        auto& ui=menu.ui;
        const auto actual=at<Address>(object,16);
        if(const auto expected=ui.get(key))return actual==expected;
        std::string observed;
        if(!UE::object_path(actual,ui.image+name_pool,read_local,observed)||observed!=path)return false;
        ui.remember(key,actual);
        return true;
    }

    Address method(Address object,UE::WeakKey& key,std::string_view short_name) {
        auto& ui=menu.ui;
        if(const auto known=ui.get(key))return known;
        const auto type=at<Address>(object,16);
        unsigned checked{};
        for(auto fn=at<Address>(type,72);fn&&checked<2048;fn=at<Address>(fn,40),++checked){
            std::string leaf;
            if(at<Address>(fn,32)!=type||!UE::name_string(ui.image+name_pool,at<UE::NameKey>(fn,24),read_local,leaf)||
               leaf!=short_name||!ui.method(fn,0,{}))continue;
            ui.remember(key,fn);
            return fn;
        }
        return 0;
    }

    void clear() {
        auto& ui=menu.ui;
        if(const auto widget=ui.get(button))event(widget,ui.get(remove_parent),nullptr);
        if(pause_invite.place_click_retained){
            pause_invite.place_click_retained=false;
            if(!ui.get(pause_invite.button)&&!pause_invite.modal_ticket)
                pause_invite.unhook(pause_invite.hooked_function,&observe_pause_click,pause_invite.original);
        }
        button=inspect=game_menu=pending_item={};
        token=0;
        pending_until=failure_until=0;
        enabled=-1;
    }

    bool mount(Address inventory,Address modal) {
        auto& ui=menu.ui;
        const auto box=ui.object(modal,"ConsumableActions"),source=ui.object(modal,"UseButton");
        if(!source||!ui.is(box,ui.get(vertical_box_type))||ui.parent(source)!=box||
           !ui.method(set_enabled,1,{{"bInIsEnabled","BoolProperty",0,1}})||
           !method(modal,close,"CloseInspectModal")||!method(modal,close_internal,"Close_Internal")||
           !method(inventory,update,"UpdateInventory")||
           !method(inventory,equipment,"RefreshEquipSlotsAndStats"))return false;
        const auto added=ui.button(modal,source,ui.chinese()?L"放置":L"Place");
        if(!added||!pause_invite.attach(&observe_pause_click))return false;
        pause_invite.place_click_retained=true;
        ui.remember(button,added);
        const auto slot=ui.add(box,added,add_vertical);
        if(!slot){clear();return false;}
        ui.copy_struct(slot,ui.object(source,"Slot"),"Padding",16);
        ui.remember(game_menu,inventory);
        ui.remember(inspect,modal);
        return true;
    }

    void receive(Address object) {
        auto& ui=menu.ui;
        if(token||enabled!=1||ui.get(button)!=object)return;
        const auto modal=ui.get(inspect);
        if(modal&&pause_invite.flag(modal,"IsOpen"))
            if(const auto item=ui.object(modal,"CurrentItem")){
                ui.remember(pending_item,item);
                pending_until=GetTickCount64()+1500;
            }
    }

    void tick(Address engine,ULONGLONG now) {
        auto& ui=menu.ui;
        if(!ui.ready||menu.on_title||!core.place_available||status.result!=Room::Result::running||
           world_phase.load(std::memory_order_acquire)!=World::Phase::active){clear();return;}
        if(now<next&&!ui.get(pending_item)&&!token)return;
        next=now+150;
        const auto instance=ui.object(engine,"GameInstance");
        Address args[]{instance,0,0};
        if(instance)event(ui.get(gameplay),ui.get(local_pawn),args);
        const auto inventory=ui.object(args[2],"CurrentGameMenu");
        const auto modal=ui.object(inventory,"FS_InspectItem");
        if(!type(inventory,game_type,"/Game/UI/Menus/GameMenu.GameMenu_C")||
           !type(modal,inspect_type,"/Game/UI/Widgets/FS_InspectItem.FS_InspectItem_C")||
           !pause_invite.flag(modal,"IsOpen")){clear();return;}
        const auto item=ui.object(modal,"CurrentItem");
        if(ui.get(inspect)&&ui.get(inspect)!=modal)clear();
        if(token){
            const auto result=core.place_result(token);
            if(!result)return;
            token=0;
            if(result>0&&ui.get(inspect)==modal&&ui.get(game_menu)==inventory){
                event(modal,ui.get(close),nullptr);
                event(modal,ui.get(close_internal),nullptr);
                event(inventory,ui.get(update),nullptr);
                event(inventory,ui.get(equipment),nullptr);
                clear();
                return;
            }
            if(result<0){
                const auto widget=ui.get(button);
                const auto text=result==-2?(ui.chinese()?L"此处无法放置":L"Cannot place here"):
                    (ui.chinese()?L"放置失败，请重试":L"Placement failed. Try again.");
                if(widget)menu.button_caption(widget,text);
                failure_until=now+2200;
            }
        }
        const auto available=core.place_available(reinterpret_cast<void*>(item));
        if(!available){clear();return;}
        if(!ui.get(button)&&!mount(inventory,modal))return;
        if(failure_until&&now>=failure_until){
            failure_until=0;
            menu.button_caption(ui.get(button),ui.chinese()?L"放置":L"Place");
        }
        if(available!=enabled){
            unsigned char state=available==1;
            event(ui.get(button),ui.get(set_enabled),&state);
            enabled=available;
        }
        const auto request=ui.get(pending_item);
        if(!request||request!=item||now>=pending_until)pending_item={};
        else if(available==1){
            token=core.place_queue(reinterpret_cast<void*>(modal),reinterpret_cast<void*>(item));
            if(token)pending_item={};
        }
    }
};
PlaceMenu place_menu;
void observe_place_button(Address object){place_menu.receive(object);}
}
