#define DllMain UnusedBootFixtureDllMain
#include "native_boot_probe.cpp"
#undef DllMain
#include <algorithm>

namespace {
#include "../../adapters/windows/native_title_widgets.inc"

TitleUi::Bindings ui;
std::atomic<Tick> title_original{};
ULONGLONG title_began{},not_before{};
unsigned title_stage{};
bool title_finished{},stock_options{},save_canceled{},stock_load{},room_opened{},room_closed{};
bool friends_requested{},friends_opened{},friends_paged{};
bool entry_order{},return_entry{},save_visible{};
UE::WeakKey title_entry{};
Address get_text{},text_to_string{},quit_fn{},quit_cdo{};

const wchar_t* localized(const wchar_t* en,const wchar_t* zh){return ui.chinese()?zh:en;}

void find_caption_methods(){
    const auto array=at<UE::ObjectArray>(image+TitleUi::object_array,0);
    if(!UE::object_array_shape(array))return;
    for(unsigned i=0;i<static_cast<unsigned>(array.num_elements)&&!(get_text&&text_to_string&&quit_fn&&quit_cdo);++i){
        const auto chunk=at<Address>(array.chunks,(i/UE::objects_per_chunk)*8);
        if(!chunk)continue;
        const auto item=at<UE::ObjectItem>(chunk,(i%UE::objects_per_chunk)*sizeof(UE::ObjectItem));
        if(!item.object||(item.flags&0x30000000u))continue;
        const auto name=leaf(item.object);
        if(name=="GetText"&&path_is(item.object,"/Script/UMG.TextBlock.GetText")&&
           at<std::uint16_t>(item.object,182)==24&&parameter(item.object,"ReturnValue",0,24))get_text=item.object;
        if(name=="Conv_TextToString"&&path_is(item.object,"/Script/Engine.KismetTextLibrary.Conv_TextToString")&&
           at<std::uint16_t>(item.object,182)==40&&parameter(item.object,"InText",0,24)&&parameter(item.object,"ReturnValue",24,16))
            text_to_string=item.object;
        if(name=="Default__KismetSystemLibrary"&&path_is(item.object,"/Script/Engine.Default__KismetSystemLibrary"))quit_cdo=item.object;
        if(name=="QuitGame"&&path_is(item.object,"/Script/Engine.KismetSystemLibrary.QuitGame")&&
           at<std::uint16_t>(item.object,182)==18&&parameter(item.object,"WorldContextObject",0,8)&&
           parameter(item.object,"SpecificPlayer",8,8)&&parameter(item.object,"QuitPreference",16,1)&&
           parameter(item.object,"bIgnorePlatformRestrictions",17,1))quit_fn=item.object;
    }
}

bool text_is(Address text,std::wstring_view expected){
    if(!ui.is(text,ui.get(TitleUi::text_type))||!get_text||!text_to_string)return false;
    struct Convert {alignas(8) std::array<unsigned char,24> text{};const wchar_t* data{};int count{},capacity{};} args;
    const auto text_prop=UE::find_property(get_text,image+TitleUi::name_pool,"ReturnValue",read_local);
    const auto string_prop=UE::find_property(text_to_string,image+TitleUi::name_pool,"ReturnValue",read_local);
    reinterpret_cast<TitleUi::ValueLifetime>(image+0x11a4420)(reinterpret_cast<const void*>(text_prop),args.text.data());
    TitleUi::event(text,get_text,args.text.data());
    TitleUi::event(ui.get(TitleUi::text_library),text_to_string,&args);
    const bool same=args.data&&args.count==static_cast<int>(expected.size()+1)&&
        std::wstring_view(args.data,expected.size())==expected;
    reinterpret_cast<TitleUi::ValueLifetime>(image+0x1170390)(reinterpret_cast<const void*>(string_prop),&args.data);
    reinterpret_cast<TitleUi::ValueLifetime>(image+0x119e940)(reinterpret_cast<const void*>(text_prop),args.text.data());
    return same;
}
bool caption_is(Address button,std::wstring_view expected){
    if(!ui.is(button,ui.get(TitleUi::button_type)))return false;
    return text_is(ui.first_text(ui.object(button,"Subcontent")),expected);
}

Address named_button(Address box,std::wstring_view caption){
    if(!ui.is(box,ui.get(TitleUi::vertical_box_type)))return 0;
    const auto count=ui.count(box);
    for(int i=0;i<count&&i<32;++i){
        auto child=ui.child(box,i);
        if(ui.is(child,ui.get(TitleUi::size_type)))child=ui.child(child,0);
        if(caption_is(child,caption))return child;
    }
    return 0;
}

bool named_label(Address box,std::wstring_view caption){
    if(!ui.is(box,ui.get(TitleUi::panel_type)))return false;
    const auto count=ui.count(box);
    for(int i=0;i<count&&i<32;++i){
        auto child=ui.child(box,i);
        if(ui.is(child,ui.get(TitleUi::size_type)))child=ui.child(child,0);
        if(text_is(child,caption))return true;
    }
    return false;
}
bool heading_is(Address panel,std::wstring_view caption){
    const auto selection=ui.object(panel,"OptionsSelectMenu");
    return selection&&named_label(ui.child(ui.child(selection,0),0),caption);
}
bool next_row_is(Address box,Address anchor,Address next){
    for(int i=1;i<ui.count(box);++i)
        if(ui.child(box,i-1)==anchor)return ui.child(box,i)==next;
    return false;
}
struct FriendFooter {Address notice{},left{},back{},right{},left_slot{},right_slot{};};
FriendFooter friend_footer(Address rows){
    if(!ui.is(rows,ui.get(TitleUi::vertical_box_type)))return {};
    const auto count=ui.count(rows);
    if(count<2)return {};
    const auto box=ui.child(rows,count-1),status=ui.child(rows,count-2);
    if(!ui.is(box,ui.get(TitleUi::size_type))||!ui.is(status,ui.get(TitleUi::size_type)))return {};
    const auto bar=ui.child(box,0),notice=ui.child(status,0);
    if(!ui.is(bar,ui.get(TitleUi::horizontal_box_type))||ui.count(bar)!=3||
       !ui.is(notice,ui.get(TitleUi::text_type)))return {};
    const auto left=ui.child(bar,0),right=ui.child(bar,2),back=ui.child(bar,1);
    if(!ui.is(left,ui.get(TitleUi::size_type))||!ui.is(right,ui.get(TitleUi::size_type))||
       !caption_is(back,localized(L"Back",L"返回")))return {};
    const auto previous=ui.child(left,0),next=ui.child(right,0);
    if((previous&&!caption_is(previous,L"‹"))||(next&&!caption_is(next,L"›")))return {};
    return {notice,previous,back,next,left,right};
}

#ifdef QDCOOP_PAUSE_TITLE_FIXTURE
#include "pause_invite_fixture.inc"
#endif
#ifdef QDCOOP_STEAM_INVITATION_UI_FIXTURE
#include "pause_invitation_fixture.inc"
#endif
#ifdef QDCOOP_SKIN_TITLE_FIXTURE
#include "skin_title_fixture.inc"
#endif
#ifdef QDCOOP_SOCIAL_LAYOUT_FIXTURE
#include "social_layout_fixture.inc"
#endif

void report_title(bool passed,Address instance){
    wchar_t path[32768]{};
    if(const auto size=GetEnvironmentVariableW(L"QD_BOOT_REPORT",path,32768);size&&size<32768){
        if(auto file=_wfopen(path,L"wb")){
            std::fprintf(file,"{\"passed\":%u,\"stage\":%u,\"stock_options\":%u,\"save_canceled\":%u,"
                "\"stock_load\":%u,\"room_opened\":%u,\"room_closed\":%u,\"title_bindings\":%u,"
                "\"live_title\":%u,\"title_level\":%u,\"entry\":%u,\"friends_opened\":%u,\"friends_paged\":%u,"
                "\"entry_order\":%u,\"return_entry\":%u,\"save_visible\":%u,",
                passed,title_stage,stock_options,save_canceled,stock_load,room_opened,room_closed,ui.ready,ui.get(ui.active_title)!=0,
                instance&&ui.ready&&ui.title_level(instance),ui.get(title_entry)!=0,friends_opened,friends_paged,
                entry_order,return_entry,save_visible);
#ifdef QDCOOP_PAUSE_TITLE_FIXTURE
            std::fprintf(file,"\"pause_stage\":%u,\"pause_invite\":%u,\"pause_restored\":%u,\"pause_opens\":%u,"
                         "\"pause_shown\":%u,\"pause_box\":%u,\"pause_hook\":%u,\"pause_overlay_result\":%u,"
                         "\"pause_footer_inside\":%u,\"pause_both_arrows\":%u,\"pause_paged_back\":%u,",
                         PauseFixture::stage,PauseFixture::invite,PauseFixture::restored,PauseFixture::opens,
                         PauseFixture::visible_menu,PauseFixture::row_ready,PauseFixture::native_hook,
                         static_cast<unsigned>(PauseFixture::overlay_result),PauseFixture::footer_inside,
                         PauseFixture::both_arrows,PauseFixture::paged_back);
#endif
#ifdef QDCOOP_STEAM_INVITATION_UI_FIXTURE
            InvitationFixture::report(file);
#endif
#ifdef QDCOOP_SKIN_TITLE_FIXTURE
            SkinFixture::report(file);
#endif
#ifdef QDCOOP_SOCIAL_LAYOUT_FIXTURE
            SocialLayoutFixture::report(file);
#endif
            std::fputs("\"missing\":[",file);
            bool comma=false;
            for(unsigned i=0;i<ui.symbols.size();++i){
                if(ui.get(ui.symbols[i]))continue;
                std::fprintf(file,"%s\"%s\"",comma?",":"",TitleUi::specs[i].leaf);
                comma=true;
            }
            std::fprintf(file,"]}\n");
            std::fclose(file);
        }
    }
}

void end_title(bool passed,Address instance){
    report_title(passed,instance);
    if(instance&&quit_fn&&quit_cdo){
        alignas(8) unsigned char args[18]{};
        std::memcpy(args,&instance,8);
        args[17]=1;
        title_finished=true;
        TitleUi::event(quit_cdo,quit_fn,args);
    }else title_finished=request_unreal_exit();
}

void title_probe_tick(void* engine,float dt,bool idle){
    const auto original=title_original.load(std::memory_order_acquire);
    original(engine,dt,idle);
    if(title_finished)return;
    const auto now=GetTickCount64();
    if(!title_began)title_began=now;
    const auto instance=ui.image?ui.object(reinterpret_cast<Address>(engine),"GameInstance"):0;
    if(now-title_began>85000){
        Room::Status room;
        if(now-title_began<93000&&read_status(&room,sizeof(room))){
            if(room.result==Room::Result::running||room.result==Room::Result::preparing)leave_room();
            if(room.result==Room::Result::running||room.result==Room::Result::preparing||room.result==Room::Result::stopping)return;
        }
        end_title(false,instance);
        return;
    }
    if(now<not_before)return;
    not_before=now+150;
    if(title_stage>=11){
        Room::Status room;
        if(!instance||!read_status(&room,sizeof(room)))return;
#ifdef QDCOOP_STEAM_INVITATION_UI_FIXTURE
        if(InvitationFixture::tick(instance,room)){
            ++title_stage;
            room_opened=true;
            room_closed=true;
            end_title(!GetModuleHandleW(L"UE4SS.dll"),instance);
        }
#else
        if(title_stage==11){
            if(ui.title_level(instance)||room.result!=Room::Result::running||
               room.room.phase!=static_cast<unsigned>(Steam::Phase::waiting)||room.room.limit!=4||
               room.room.members!=1||!real_waiting_epoch(room))return;
            room_opened=true;
#ifdef QDCOOP_PAUSE_TITLE_FIXTURE
            if(!PauseFixture::tick(instance))return;
            if(!PauseFixture::exit_to_title(instance))return;
#else
            if(!leave_room())return;
#endif
            ++title_stage;
        }else if(room.result==Room::Result::closed){
            room_closed=room.room.left==1;
#ifdef QDCOOP_PAUSE_TITLE_FIXTURE
            if(!ui.title_level(instance))return;
            const auto title=ui.find_title();
            const auto source=ui.object(title,"LoadGameButton"),box=source?ui.parent(source):0;
            const auto entry=named_button(box,localized(L"Multiplayer",L"多人游戏"));
            if(!entry||!next_row_is(box,source,entry))return;
            return_entry=true;
            ui.remember(title_entry,entry);
#endif
            end_title(stock_options&&save_canceled&&stock_load&&room_opened&&room_closed&&friends_opened&&
                entry_order&&save_visible&&!GetModuleHandleW(L"UE4SS.dll"),instance);
        }
#endif
        return;
    }
    if(!ui.ready){ui.step(8192);return;}
    if(!ui.get(ui.active_title)&&!ui.find_title())return;
    if(!get_text||!text_to_string||!quit_cdo||!quit_fn)find_caption_methods();
    const auto title=ui.get(ui.active_title);
    const auto source=ui.object(title,"LoadGameButton");
    const auto box=source?ui.parent(source):0;
    const auto entry=ui.get(title_entry);
    const auto panel=ui.object(instance,"CurrentOptionsMenu");
    const bool visible=ui.is(panel,ui.get(TitleUi::options_type))&&ui.shown(panel);
    const auto video=visible?ui.object(panel,"TopLevel_VideoButton"):0;
    const auto video_row=video?ui.parent(video):0;
    const auto options=video_row?ui.parent(video_row):0;
    const auto save=ui.member(title,"InSaveMenu","BoolProperty",1);
    const bool in_save=save&&UE::get_bool(reinterpret_cast<void*>(save),reinterpret_cast<const void*>(title),0);
    const auto select=ui.get(TitleUi::click);
#ifdef QDCOOP_SKIN_TITLE_FIXTURE
    if(SkinFixture::tick(box,options,panel,visible)){end_title(!GetModuleHandleW(L"UE4SS.dll"),instance);return;}
    return;
#endif
#ifdef QDCOOP_SOCIAL_LAYOUT_FIXTURE
    if(SocialLayoutFixture::tick(box,panel,options,visible)){end_title(!GetModuleHandleW(L"UE4SS.dll"),instance);return;}
    return;
#endif
    if(title_stage==0){
        const auto multiplayer=named_button(box,localized(L"Multiplayer",L"多人游戏"));
        if(!multiplayer||ui.parent(multiplayer)!=box)return;
        entry_order=next_row_is(box,source,multiplayer);
        ui.remember(title_entry,multiplayer);
        TitleUi::event(multiplayer,select,nullptr);
    }else if(title_stage==1){
        const auto back=named_button(options,localized(L"Back",L"返回"));
        if(!friends_opened){
            const auto footer=friend_footer(options);
            if(!friends_requested){
                const auto join=named_button(options,localized(L"Join Steam game",L"加入STEAM游戏"));
                if(!join)return;
                friends_requested=true;
                TitleUi::event(join,select,nullptr);
            }else if(GetEnvironmentVariableW(L"QD_TEST_NO_STEAM",nullptr,0)){
                if(footer.back&&heading_is(panel,localized(L"Join Steam friends",L"加入好友"))&&
                   named_label(options,localized(L"Launch QueensDomain from your Steam library.",L"请从 Steam 库启动游戏。")))
                    end_title(entry_order&&!GetModuleHandleW(L"UE4SS.dll"),instance);
            }else if(footer.back&&heading_is(panel,localized(L"Join Steam friends",L"加入好友"))&&
                     !named_label(options,localized(L"Steam friends are unavailable.",L"暂时无法读取好友。"))&&
                     !named_label(options,localized(L"Connecting to Steam friends…",L"正在连接 Steam 好友……"))&&
                     !named_label(options,localized(L"Launch QueensDomain from your Steam library.",L"请从 Steam 库启动游戏。"))){
                if(footer.right&&!friends_paged){
                    if(footer.left)return;
                    TitleUi::event(footer.right,select,nullptr);
                    friends_paged=true;
                    return;
                }
                if(friends_paged&&!footer.left)return;
                friends_opened=true;
                TitleUi::event(footer.back,select,nullptr);
            }
            return;
        }
        if(!back||!named_button(options,localized(L"Host Steam game",L"主持STEAM游戏"))||!named_button(options,localized(L"Join Steam game",L"加入STEAM游戏"))||
           !video_row||ui.visibility(video_row)!=1)return;
        TitleUi::event(back,select,nullptr);
    }else if(title_stage==2){
        if(visible)return;
        auto stock=ui.object(title,"Options");
        if(!stock)return;
        TitleUi::event(title,ui.get(TitleUi::title_options),&stock);
    }else if(title_stage==3){
        if(!visible||!video_row||ui.visibility(video_row)==1||named_button(options,localized(L"Host Steam game",L"主持STEAM游戏")))return;
        stock_options=true;
        TitleUi::event(panel,ui.get(TitleUi::options_close),nullptr);
    }else if(title_stage==4){
        if(visible||!entry)return;
        TitleUi::event(entry,select,nullptr);
    }else if(title_stage==5){
        const auto host=named_button(options,localized(L"Host Steam game",L"主持STEAM游戏"));
        if(host){TitleUi::event(host,select,nullptr);return;}
        const auto choose=named_button(options,localized(L"Choose save",L"选择存档"));
        if(!choose||!named_button(options,localized(L"Players: 2",L"玩家人数：2")))return;
        TitleUi::event(choose,select,nullptr);
    }else if(title_stage==6){
        if(!in_save)return;
        const auto field=ui.member(title,"AnyValidSaves","BoolProperty",1);
        save_visible=field&&UE::get_bool(reinterpret_cast<void*>(field),reinterpret_cast<const void*>(title),0);
        TitleUi::event(title,ui.get(TitleUi::title_back),nullptr);
    }else if(title_stage==7){
        const auto back=named_button(options,localized(L"Back",L"返回"));
        if(in_save||!back)return;
        Room::Status room;
        save_canceled=read_status&&read_status(&room,sizeof(room))&&room.result==Room::Result::idle;
        TitleUi::event(back,select,nullptr);
    }else if(title_stage==8){
        if(visible)return;
        TitleUi::event(entry,select,nullptr);
    }else if(title_stage==9){
        const auto host=named_button(options,localized(L"Host Steam game",L"主持STEAM游戏"));
        if(host){TitleUi::event(host,select,nullptr);return;}
        auto seats=named_button(options,localized(L"Players: 2",L"玩家人数：2"));
        if(!seats)seats=named_button(options,localized(L"Players: 3",L"玩家人数：3"));
        if(seats){TitleUi::event(seats,select,nullptr);return;}
        const auto choose=named_button(options,localized(L"Choose save",L"选择存档"));
        if(!choose||!named_button(options,localized(L"Players: 4",L"玩家人数：4")))return;
        TitleUi::event(choose,select,nullptr);
    }else if(title_stage==10){
        if(!in_save)return;
        struct Args {int slot;bool backup;unsigned char padding[3];int backup_slot;} args{1,false,{},0};
        TitleUi::event(title,ui.get(TitleUi::load_slot),&args);
        stock_load=true;
    }
    ++title_stage;
}

bool attach_title_observer(HMODULE module,Tick observer=title_probe_tick){
    image=reinterpret_cast<Address>(GetModuleHandleW(nullptr));
    auto cell=reinterpret_cast<void**>(image+0x3e49ee8);
    const auto value=*cell;
    HMODULE owner{},self{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCWSTR>(value),&owner)||owner!=module||
       !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                          reinterpret_cast<LPCWSTR>(observer),&self))return false;
    DWORD protect{},ignored{};
    if(!VirtualProtect(cell,sizeof(void*),PAGE_READWRITE,&protect))return false;
    title_original.store(reinterpret_cast<Tick>(value),std::memory_order_release);
    const auto hook=reinterpret_cast<void*>(observer);
    const auto prior=InterlockedCompareExchangePointer(cell,hook,value);
    if(!VirtualProtect(cell,sizeof(void*),protect,&ignored)){
        InterlockedCompareExchangePointer(cell,value,hook);
        VirtualProtect(cell,sizeof(void*),protect,&ignored);
        return false;
    }
    return prior==value;
}

DWORD WINAPI start_title_menu_probe(void*){
    const auto end=GetTickCount64()+30000;
    do{
        const auto module=GetModuleHandleW(L"QDNativeBootstrap.dll");
        if(module&&load_export(module,"QDNativeReadStatus",read_status)&&load_export(module,"QDNativeLeaveRoom",leave_room)&&
           attach_title_observer(module))return 0;
        Sleep(100);
    }while(GetTickCount64()<end);
    report_title(false,0);
    return 1;
}
}

#ifdef QDCOOP_LAN_TITLE_FIXTURE
BOOL WINAPI UnusedTitleFixtureDllMain(HINSTANCE,DWORD reason,void*){
#else
BOOL WINAPI DllMain(HINSTANCE,DWORD reason,void*){
#endif
    if(reason==DLL_PROCESS_ATTACH)
        if(const auto thread=CreateThread(nullptr,0,start_title_menu_probe,nullptr,0,nullptr))CloseHandle(thread);
    return TRUE;
}
