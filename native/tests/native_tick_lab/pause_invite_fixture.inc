namespace PauseFixture {
enum Symbol {pawn,open,close,exit_title,cached_geometry,local_to_absolute,absolute_size,slate_cdo,types};
constexpr const char* paths[]{
    "/Script/Engine.GameplayStatics.GetPlayerPawn",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.OpenPauseMenu",
    "/Game/UI/Menus/PauseMenu.PauseMenu_C.BndEvt__PauseMenu_ContinueButton_K2Node_ComponentBoundEvent_9_CommonButtonBaseClicked__DelegateSignature",
    "/Game/UI/Menus/PauseMenu.PauseMenu_C.ConfirmExitToMenu",
    "/Script/UMG.Widget.GetCachedGeometry",
    "/Script/UMG.SlateBlueprintLibrary.LocalToAbsolute",
    "/Script/UMG.SlateBlueprintLibrary.GetAbsoluteSize",
    "/Script/UMG.Default__SlateBlueprintLibrary"
};
std::array<UE::WeakKey,types> methods{};
UE::WeakKey menu{},entry{};
unsigned stage{},opens{};
bool invite{},restored{},visible_menu{},row_ready{},native_hook{},invite_order{},exit_opened{};
bool footer_inside{},both_arrows{},paged_back{};
ULONGLONG open_after{};
Social::Action overlay_result{};

void scan(){
    const auto array=at<UE::ObjectArray>(image+TitleUi::object_array,0);
    if(!UE::object_array_shape(array))return;
    for(unsigned i=0;i<static_cast<unsigned>(array.num_elements);++i){
        const auto chunk=at<Address>(array.chunks,(i/UE::objects_per_chunk)*8);
        if(!chunk)continue;
        const auto item=at<UE::ObjectItem>(chunk,(i%UE::objects_per_chunk)*sizeof(UE::ObjectItem));
        if(!item.object||(item.flags&0x30000000u))continue;
        const auto type=at<Address>(item.object,16);
        const auto kind=leaf(type);
        if(kind=="PauseMenu_C"&&!(at<unsigned>(item.object,8)&0x10u)&&ui.shown(item.object))ui.remember(menu,item.object);
        if(kind!="Function"&&kind!="SlateBlueprintLibrary")continue;
        const auto name=leaf(item.object);
        for(unsigned j=0;j<types;++j){
            const std::string_view path=paths[j];
            if(name==path.substr(path.rfind('.')+1)&&path_is(item.object,path))ui.remember(methods[j],item.object);
        }
    }
}
Address get(Symbol symbol){return ui.get(methods[symbol]);}
struct Rect {
    float x{},y{},w{},h{};
    float right() const {return x+w;}
    float bottom() const {return y+h;}
    bool contains(const Rect& child) const {
        return child.w>1.f&&child.h>1.f&&x<=child.x&&y<=child.y&&right()>=child.right()&&bottom()>=child.bottom()+3.f;
    }
};
Rect bounds(Address widget){
    if(!widget||!get(cached_geometry)||!get(slate_cdo))return {};
    const auto bytes=at<unsigned short>(get(cached_geometry),182);
    if(!bytes||bytes>96||!ui.method(get(cached_geometry),bytes,{{"ReturnValue","StructProperty",0,bytes}})||
       !ui.method(get(local_to_absolute),bytes+16,{{"Geometry","StructProperty",0,bytes},
           {"LocalCoordinate","StructProperty",bytes,8},{"ReturnValue","StructProperty",static_cast<unsigned>(bytes)+8,8}})||
       !ui.method(get(absolute_size),bytes+8,{{"Geometry","StructProperty",0,bytes},{"ReturnValue","StructProperty",bytes,8}}))return {};
    alignas(16) std::array<unsigned char,128> args{};
    TitleUi::event(widget,get(cached_geometry),args.data());
    TitleUi::event(get(slate_cdo),get(local_to_absolute),args.data());
    const auto memory=reinterpret_cast<Address>(args.data());
    const auto position=at<std::array<float,2>>(memory,bytes+8);
    TitleUi::event(get(slate_cdo),get(absolute_size),args.data());
    const auto size=at<std::array<float,2>>(memory,bytes);
    return {position[0],position[1],size[0],size[1]};
}
Rect footer_return{};
bool tick(Address instance) {
    if(restored)return true;
    if(!read_social){
        const auto module=GetModuleHandleW(L"QDNativeBootstrap.dll");
        if(!module||!load_export(module,"QDNativeReadSteamSocial",read_social))return false;
    }
    Social::Status social;
    if(!read_social(&social,sizeof(social)))return false;
    if((stage<2&&(!get(pawn)||!get(open)||!ui.get(menu)))||(stage>=3&&!get(slate_cdo)))scan();
    const auto active=ui.get(menu);
    const auto source=ui.object(active,"ContinueButton");
    const auto source_row=source?ui.parent(source):0;
    const auto box=source_row?ui.parent(source_row):0;
    const auto click=ui.get(TitleUi::click);
    visible_menu=active&&ui.shown(active);
    row_ready=ui.is(box,ui.get(TitleUi::vertical_box_type));
    native_hook=at<Address>(click,216)!=image+0x11a7e40;
    const auto now=GetTickCount64();
    if(stage==0||(stage==1&&!visible_menu&&now>=open_after)){
        if(!open_after){open_after=now+1800;return false;}
        if(now<open_after)return false;
        if(!get(pawn)||!get(open)||!parameter(get(pawn),"WorldContextObject",0,8)||!parameter(get(pawn),"PlayerIndex",8,4)||
           !parameter(get(pawn),"ReturnValue",16,8)||!parameter(get(open),"QuickJournal",0,1))return false;
        Address args[]{instance,0,0};
        TitleUi::event(ui.get(TitleUi::gameplay),get(pawn),args);
        if(!args[2])return false;
        bool quick{};
        TitleUi::event(args[2],get(open),&quick);
        ++opens;
        stage=1;
        open_after=now+1800;
        return false;
    }else if(stage==1){
        const auto button=named_button(box,localized(L"Invite players",L"邀请玩家"));
        if(!button||!source||social.action!=Social::Action::none)return false;
        const auto status_row=ui.parent(ui.object(active,"Statusbutton"));
        invite_order=next_row_is(box,status_row,ui.parent(button));
        if(!invite_order)return false;
        ui.remember(entry,button);
        TitleUi::event(source,click,nullptr);
    }else if(stage==2){
        const auto button=ui.get(entry);
        if(!button||social.action!=Social::Action::none)return false;
        TitleUi::event(button,click,nullptr);
    }else if(stage>=3&&stage<=6){
        const auto panel=ui.object(active,"ActiveOptionsMenu");
        const auto video=ui.object(panel,"TopLevel_VideoButton");
        const auto options=video?ui.parent(ui.parent(video)):0;
        const auto footer=friend_footer(options);
        if(!panel||!ui.shown(panel)||!footer.back||!heading_is(panel,localized(L"Invite players",L"邀请玩家"))||
           named_label(options,localized(L"Steam friends are unavailable.",L"暂时无法读取好友。"))||social.action!=Social::Action::none)return false;
        const auto sent=localized(L"Invitation sent.",L"Steam 邀请已发送。");
        if(stage==3){
            if(footer.left||!footer.right)return false;
            TitleUi::event(footer.right,click,nullptr);
        }else if(stage==4){
            if(!footer.left)return false;
            both_arrows=footer.right!=0;
            // Exercise the status geometry without sending a real Steam invitation.
            ui.label(footer.notice,sent);
        }else if(stage==5){
            if(!footer.left)return false;
            if(!text_is(footer.notice,sent)){ui.label(footer.notice,sent);return false;}
            const auto selection=ui.object(panel,"OptionsSelectMenu");
            const auto frame=bounds(selection?ui.child(selection,0):0);
            const auto notice=bounds(footer.notice),left=bounds(footer.left_slot),right=bounds(footer.right_slot);
            footer_return=bounds(footer.back);
            footer_inside=frame.contains(notice)&&frame.contains(left)&&frame.contains(right)&&frame.contains(footer_return)&&
                left.right()<=footer_return.x+1.f&&footer_return.right()<=right.x+1.f&&
                notice.bottom()<=footer_return.y+1.f;
            if(!footer_inside)return false;
            TitleUi::event(footer.left,click,nullptr);
        }else{
            if(footer.left||!footer.right)return false;
            const auto current=bounds(footer.back);
            if(current.y<footer_return.y-1.f||current.y>footer_return.y+1.f||current.h<=1.f)return false;
            paged_back=true;
            invite=true;
            overlay_result=social.action;
            TitleUi::event(footer.back,click,nullptr);
        }
    }else if(stage==7){
        const auto panel=ui.object(active,"ActiveOptionsMenu");
        if(!active||!visible_menu||(panel&&ui.shown(panel))||!source||!get(close)||
           !parameter(get(close),"Button",0,8))return false;
        auto argument=source;
        TitleUi::event(active,get(close),&argument);
    }else if(stage==8){
        const auto button=ui.get(entry);
        const auto row=button?ui.parent(button):0;
        if((active&&ui.shown(active))||(row&&ui.parent(row))||at<Address>(click,216)!=image+0x11a7e40)return false;
        restored=true;
        return true;
    }
    ++stage;
    return false;
}
bool exit_to_title(Address instance) {
    if(!get(pawn)||!get(open)||!get(exit_title))return false;
    if(!exit_opened){
        Address args[]{instance,0,0};
        TitleUi::event(ui.get(TitleUi::gameplay),get(pawn),args);
        if(!args[2])return false;
        bool quick{};
        TitleUi::event(args[2],get(open),&quick);
        exit_opened=true;
        return false;
    }
    scan();
    const auto active=ui.get(menu);
    if(!active||!ui.shown(active)||!ui.method(get(exit_title),0,{}))return false;
    TitleUi::event(active,get(exit_title),nullptr);
    return true;
}
}
