namespace SkinFixture {
unsigned stage{};
bool persisted{},preview_only{},image_bound{};

bool reload() {
    wchar_t value[12]{};
    return GetEnvironmentVariableW(L"QD_SKIN_TEST_PHASE",value,12)&&std::wstring_view(value)==L"reload";
}
bool cli() {
    wchar_t value[12]{};
    return GetEnvironmentVariableW(L"QD_SKIN_TEST_PHASE",value,12)&&std::wstring_view(value)==L"cli";
}
std::string preference(){
    auto path=ui.project_saved_directory()+L"/QDCoopNative/player-skin.txt";
    const auto file=_wfopen(path.c_str(),L"rb");
    if(!file)return {};
    char contents[64]{};
    const auto count=std::fread(contents,1,sizeof(contents),file);
    std::fclose(file);
    return std::string(contents,count);
}
bool displayed(Address box,std::wstring_view caption,std::string_view path){
    if(!ui.is(box,ui.get(TitleUi::vertical_box_type)))return false;
    bool label=false,texture=false;
    for(int i=0,count=ui.count(box);i<count&&i<24;++i){
        const auto row=ui.child(box,i);
        if(ui.is(row,ui.get(TitleUi::horizontal_box_type))){
            for(int j=0;j<ui.count(row);++j)if(text_is(ui.child(ui.child(row,j),0),caption))label=true;
            continue;
        }
        if(!ui.is(row,ui.get(TitleUi::size_type))||ui.visibility(row)==1)continue;
        const auto image_widget=ui.child(row,0);
        if(!ui.is(image_widget,ui.get(TitleUi::image_type)))continue;
        const auto brush=UE::find_inherited_property(at<Address>(image_widget,16),image+TitleUi::name_pool,"Brush",read_local);
        if(!brush||!ui.kind(brush,"StructProperty"))continue;
        const auto type=at<Address>(brush,120);
        if(!type||!path_is(type,"/Script/SlateCore.SlateBrush"))continue;
        const auto resource=UE::find_property(type,image+TitleUi::name_pool,"ResourceObject",read_local);
        if(!resource||!ui.kind(resource,"ObjectProperty")||at<unsigned>(resource,60)!=8||
           at<unsigned>(resource,76)+8>at<unsigned>(brush,60))continue;
        const auto asset=at<Address>(image_widget,at<unsigned>(brush,76)+at<unsigned>(resource,76));
        texture=ui.is(asset,ui.get(TitleUi::texture_type))&&path_is(asset,path);
    }
    image_bound=texture;
    return label&&texture;
}
bool tick(Address menu_box,Address options,Address panel,bool panel_shown){
    constexpr std::string_view prince="/Game/QDCoopSkins/Portraits/Prince.Prince",
        arjaot="/Game/QDCoopSkins/Portraits/Arjaot.Arjaot",ippibi="/Game/QDCoopSkins/Portraits/Ippibi.Ippibi",
        imba="/Game/QDCoopSkins/Portraits/Imba.Imba";
    constexpr std::string_view chosen="QDSKIN1\narjaot\n";
    const auto click=ui.get(TitleUi::click);
    const auto selection=ui.object(panel,"OptionsSelectMenu");
    const auto skin=selection&&ui.count(selection)>1?ui.child(selection,1):0;
    const auto arrows=skin&&ui.count(skin)==3?ui.child(skin,0):0;
    const auto prev=arrows?ui.child(ui.child(arrows,0),0):0;
    const auto next=arrows?ui.child(ui.child(arrows,2),0):0;
    const auto choose=named_button(skin,L"Select");
    if(stage==0){
        const auto multiplayer=named_button(menu_box,L"Multiplayer");
        if(!multiplayer)return false;
        ui.remember(title_entry,multiplayer);
        TitleUi::event(multiplayer,click,nullptr);
    }else if(stage==1){
        if(!panel_shown||!prev||!next||!choose||ui.visibility(choose)!=2||
           !caption_is(prev,L"‹")||!caption_is(next,L"›"))return false;
        if(reload()||cli()){
            if(!displayed(skin,cli()?L"Imba":L"Arjaot",cli()?imba:arjaot)||preference()!=chosen)return false;
            persisted=true;
            preview_only=true;
            return true;
        }
        if(!displayed(skin,L"Prince",prince)||!preference().empty())return false;
        TitleUi::event(prev,click,nullptr);
    }else if(stage==2){
        if(!choose||ui.visibility(choose)!=0||!displayed(skin,L"Imba",imba))return false;
        TitleUi::event(next,click,nullptr);
    }else if(stage==3){
        if(!choose||ui.visibility(choose)!=2||!displayed(skin,L"Prince",prince))return false;
        TitleUi::event(next,click,nullptr);
    }else if(stage==4){
        if(!choose||ui.visibility(choose)!=0||!displayed(skin,L"Arjaot",arjaot)||!preference().empty())return false;
        TitleUi::event(choose,click,nullptr);
    }else if(stage==5){
        if(!choose||!next||ui.visibility(choose)!=2||preference()!=chosen)return false;
        persisted=true;
        TitleUi::event(next,click,nullptr);
    }else if(stage==6){
        const auto back=named_button(options,L"Back");
        if(!back||!displayed(skin,L"Ippibi",ippibi)||preference()!=chosen)return false;
        preview_only=true;
        TitleUi::event(back,click,nullptr);
    }else if(stage==7){
        if(panel_shown||!ui.get(title_entry))return false;
        TitleUi::event(ui.get(title_entry),click,nullptr);
    }else if(stage==8){
        if(!panel_shown||!choose||ui.visibility(choose)!=2||!displayed(skin,L"Arjaot",arjaot)||preference()!=chosen)return false;
        return persisted&&preview_only;
    }
    ++stage;
    return false;
}
void report(FILE* file){
    std::fprintf(file,"\"skin_stage\":%u,\"skin_persisted\":%u,\"skin_preview_only\":%u,\"skin_image\":%u,",
                 stage,persisted,preview_only,image_bound);
}
}
