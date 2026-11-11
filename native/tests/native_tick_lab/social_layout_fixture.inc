namespace SocialLayoutFixture {
unsigned stage{},avatars{},friend_rows{},empty_actions{};
bool chinese{},other_language{},host_layout{},join_layout{},input_kept{};

bool set_language(const wchar_t* language){
    const auto cdo=ui.get(TitleUi::culture_library);
    const auto type=at<Address>(cdo,16);
    for(auto child=at<Address>(type,72);child;child=at<Address>(child,40)){
        if(!path_is(child,"/Script/Engine.KismetInternationalizationLibrary.SetCurrentLanguage"))continue;
        if(!ui.method(child,18,{{"Culture","StrProperty",0,16},{"SaveToConfig","BoolProperty",16,1},
                                {"ReturnValue","BoolProperty",17,1}}))return false;
        const auto length=static_cast<int>(std::wcslen(language)+1);
        struct Args {TitleUi::String name;bool save{},changed{};} args{{language,length,length}};
        TitleUi::event(cdo,child,&args);
        return args.changed;
    }
    return false;
}

Address field(Address rows,unsigned wanted){
    if(!ui.is(rows,ui.get(TitleUi::vertical_box_type)))return 0;
    for(int i=0;i<ui.count(rows);++i){
        const auto row=ui.child(rows,i);
        if(!ui.is(row,ui.get(TitleUi::size_type))||ui.visibility(row)==1)continue;
        const auto content=ui.child(row,0);
        if(ui.is(content,ui.get(TitleUi::edit_type))&&wanted--==0)return content;
    }
    return 0;
}

bool height_is(Address panel,Address rows,float height){
    const auto selection=ui.object(panel,"OptionsSelectMenu");
    const auto frame=selection?ui.child(selection,0):0;
    const auto frame_slot=ui.object(frame,"Slot"),rows_slot=ui.object(rows,"Slot");
    if(!frame_slot||!rows_slot)return false;
    TitleUi::CanvasLayout frame_layout{},rows_layout{};
    TitleUi::event(frame_slot,ui.get(TitleUi::canvas_layout),&frame_layout);
    TitleUi::event(rows_slot,ui.get(TitleUi::canvas_layout),&rows_layout);
    return frame_layout.offsets[0]==-265.f&&frame_layout.offsets[2]==516.f&&frame_layout.offsets[3]==height&&
           rows_layout.offsets==TitleUi::Margin{0.f,68.f,0.f,10.f}&&
           rows_layout.anchors==TitleUi::Margin{0.f,0.f,1.f,1.f}&&ui.count(selection)==2;
}

bool home_order(Address rows,bool zh){
    if(!ui.is(rows,ui.get(TitleUi::vertical_box_type)))return false;
    const wchar_t* labels[4]{
        zh?L"主持STEAM游戏":L"Host Steam game",zh?L"加入STEAM游戏":L"Join Steam game",
        zh?L"主持局域网游戏":L"Host LAN game",zh?L"加入局域网游戏":L"Join LAN game"};
    unsigned found{};
    for(int i=0;i<ui.count(rows)&&found<std::size(labels);++i){
        const auto row=ui.child(rows,i);
        if(!ui.is(row,ui.get(TitleUi::size_type))||ui.visibility(row)==1)continue;
        if(!caption_is(ui.child(row,0),labels[found++]))return false;
    }
    return found==std::size(labels);
}

bool has_texture(Address picture){
    const auto brush=UE::find_inherited_property(at<Address>(picture,16),image+TitleUi::name_pool,"Brush",read_local);
    if(!brush||!ui.kind(brush,"StructProperty"))return false;
    const auto resource=UE::find_property(at<Address>(brush,120),image+TitleUi::name_pool,"ResourceObject",read_local);
    if(!resource||!ui.kind(resource,"ObjectProperty")||at<unsigned>(resource,60)!=8)return false;
    return ui.is(at<Address>(picture,at<unsigned>(brush,76)+at<unsigned>(resource,76)),ui.get(TitleUi::texture_type));
}

bool friend_columns(Address rows,bool zh){
    if(!ui.is(rows,ui.get(TitleUi::vertical_box_type)))return false;
    const auto footer=friend_footer(rows);
    if(!footer.back)return false;
    const auto footer_bar=ui.parent(footer.back);
    unsigned total{},pictures{},empty{};
    for(int i=0;i<ui.count(rows);++i){
        const auto outer=ui.child(rows,i);
        if(!ui.is(outer,ui.get(TitleUi::size_type))||ui.visibility(outer)==1)continue;
        const auto row=ui.child(outer,0);
        if(row==footer_bar||!ui.is(row,ui.get(TitleUi::horizontal_box_type)))continue;
        if(ui.count(row)!=4)return false;
        const auto avatar=ui.child(row,0),name=ui.child(row,1),state=ui.child(row,2),action=ui.child(row,3);
        if(!ui.is(avatar,ui.get(TitleUi::size_type))||!ui.is(name,ui.get(TitleUi::scale_type))||
           !ui.is(state,ui.get(TitleUi::size_type))||!ui.is(action,ui.get(TitleUi::size_type)))return false;
        const auto picture=ui.child(avatar,0),status=ui.child(state,0);
        if(!ui.is(picture,ui.get(TitleUi::image_type))||!ui.is(ui.child(name,0),ui.get(TitleUi::text_type))||
           !(text_is(status,zh?L"在线":L"Online")||text_is(status,zh?L"离线":L"Offline")||
             text_is(status,zh?L"游戏中":L"Playing")))return false;
        const auto width=ui.member(action,"WidthOverride","FloatProperty",4);
        if(!width||at<float>(action,at<unsigned>(width,76))!=98.f)return false;
        const auto action_count=ui.count(action);
        if(action_count==0)++empty;
        else if(action_count!=1||!caption_is(ui.child(action,0),zh?L"加入":L"Join"))return false;
        if(has_texture(picture))++pictures;
        ++total;
    }
    if(!total||!pictures||!empty)return false;
    avatars=std::max(avatars,pictures);
    friend_rows=std::max(friend_rows,total);
    empty_actions=std::max(empty_actions,empty);
    return true;
}

bool tick(Address title_box,Address panel,Address rows,bool visible){
    const auto click=ui.get(TitleUi::click);
    if(stage==0){
        const auto entry=named_button(title_box,localized(L"Multiplayer",L"多人游戏"));
        if(!entry)return false;
        ui.remember(title_entry,entry);
        if(!set_language(L"zh-Hans"))return false;
    }else if(stage==1){
        const auto entry=ui.get(title_entry);
        if(!caption_is(entry,L"多人游戏"))return false;
        TitleUi::event(entry,click,nullptr);
    }else if(stage==2){
        if(!visible||!heading_is(panel,L"多人游戏")||!home_order(rows,true))return false;
        chinese=true;
        TitleUi::event(named_button(rows,L"加入STEAM游戏"),click,nullptr);
    }else if(stage==3){
        if(!visible||!heading_is(panel,L"加入好友")||!friend_columns(rows,true)||!set_language(L"ja"))return false;
    }else if(stage==4){
        const auto back=friend_footer(rows).back;
        if(!back||!heading_is(panel,L"Join Steam friends")||!friend_columns(rows,false))return false;
        other_language=caption_is(ui.get(title_entry),L"Multiplayer");
        TitleUi::event(back,click,nullptr);
    }else if(stage==5){
        if(!visible||!home_order(rows,false))return false;
        TitleUi::event(named_button(rows,L"Host LAN game"),click,nullptr);
    }else if(stage==6){
        const auto port=field(rows,0);
        if(!port||!heading_is(panel,L"Host game")||!height_is(panel,rows,440.f)||!named_label(rows,L"Room port")||
           !named_button(rows,L"Players: 2")||!named_button(rows,L"Host game"))return false;
        host_layout=true;
        ui.label(port,L"45678",TitleUi::edit_set);
        if(!set_language(L"zh-Hans"))return false;
    }else if(stage==7){
        const auto back=named_button(rows,L"返回"),port=field(rows,0);
        if(!back||!port||!heading_is(panel,L"主持游戏")||!named_label(rows,L"房间端口")||
           ui.read_field(port,5)!=L"45678")return false;
        input_kept=true;
        TitleUi::event(back,click,nullptr);
    }else if(stage==8){
        const auto join=named_button(rows,L"加入局域网游戏");
        if(!join||!home_order(rows,true))return false;
        TitleUi::event(join,click,nullptr);
    }else if(stage==9){
        const auto address=field(rows,0),port=field(rows,1),connect=named_button(rows,L"连接");
        if(!address||!port||!connect||!heading_is(panel,L"加入游戏")||!height_is(panel,rows,680.f)||
           !named_label(rows,L"房主 IP 地址")||!named_label(rows,L"端口")||
           !named_label(rows,L"同机测试用 127.0.0.1；其他电脑填写房主的局域网或 Radmin IP。"))return false;
        join_layout=true;
        ui.label(address,L"invalid",TitleUi::edit_set);
        TitleUi::event(connect,click,nullptr);
    }else if(stage==10){
        if(!named_label(rows,L"请输入有效的 IPv4 地址。")||!set_language(L"ja"))return false;
    }else if(stage==11){
        const auto address=field(rows,0);
        if(!named_label(rows,L"Enter a valid IPv4 address.")||!named_button(rows,L"Connect")||
           !address||ui.read_field(address,15)!=L"invalid")return false;
        return chinese&&other_language&&host_layout&&join_layout&&input_kept;
    }
    ++stage;
    return false;
}

void report(FILE* file){
    std::fprintf(file,"\"social_layout_stage\":%u,\"friend_rows\":%u,\"avatars\":%u,\"empty_action_slots\":%u,"
        "\"chinese\":%u,\"other_language_english\":%u,\"host_layout\":%u,\"join_layout\":%u,\"input_kept\":%u,",
        stage,friend_rows,avatars,empty_actions,chinese,other_language,host_layout,join_layout,input_kept);
}
}
