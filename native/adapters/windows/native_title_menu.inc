#include "native_title_widgets.inc"
#include "native_title_preferences.inc"

namespace TitleUi {
using Exec=void(*)(void*,void*,void*);
enum class Action { none,open,host,friends,host_lan,join_lan,fewer_seats,more_seats,confirm_host,connect,
                    previous_skin,next_skin,select_skin,friend_pick,friend_previous,friend_next,back,closed,canceled };
enum class Screen { home,host_steam,host_lan,join_lan,join_friends,invite_friends };
enum class SaveMode { none,host,guest };
struct Control {UE::WeakKey widget,row;Action action{};Steam::SteamId friend_id{};};
struct Hidden {UE::WeakKey widget;unsigned char visibility{};};
struct Phrase {const wchar_t *english{},*chinese{};};
struct FriendPortrait {Steam::SteamId id{};UE::WeakKey image;int handle{};};
struct CachedPortrait {Steam::SteamId id{};UE::WeakKey texture;int handle{};};
struct Menu {
    static constexpr unsigned friends_per_page=5;
    static constexpr const wchar_t* left_arrow=L"‹";
    static constexpr const wchar_t* right_arrow=L"›";
    static constexpr float arrow_width=55.f,arrow_height=45.f;
    Bindings ui;
    Preferences preferences;
    SaveBackup backup;
    UE::WeakKey entry{},panel{},previous_focus{},rows{},model{},address_field{},port_field{},seat_picker{},notice{};
    UE::WeakKey portrait_image{},skin_name{},skin_choose{},skin_root{},skin_left{},skin_right{};
    UE::WeakKey frame_slot{},heading{};
    std::array<float,2> frame_position{};
    CanvasLayout stock_frame{},stock_rows{};
    std::array<FriendPortrait,friends_per_page> friend_portraits{};
    std::array<CachedPortrait,24> avatar_cache{};
    std::array<Control,11> controls{};
    std::array<Hidden,16> hidden{};
    std::array<wchar_t,16> lan_address{L'1',L'2',L'7',L'.',L'0',L'.',L'0',L'.',L'1'};
    Action pending{};
    Screen screen{};
    SaveMode save_mode{};
    Room::Request host_request{};
    Steam::SteamId steam_target{};
    Steam::SteamId friend_choice{},invite_room{},save_account{};
    std::vector<Social::Friend> friends;
    std::vector<Steam::SteamId> invited;
    std::wstring original_heading;
    Phrase feedback{};
    unsigned thread{},control_count{},lan_seats{2},steam_seats{2},host_port{Room::default_lan_port},join_port{Room::default_lan_port},preview_skin{1};
    unsigned friend_page{},avatar_cursor{};
    Steam::SteamId avatar_account{};
    ULONGLONG next_step{},next_friends{},next_language{};
    FriendAccess friend_access{FriendAccess::unavailable};
    bool lan_layout{},invite_has_space{},save_selected{},joining{},zh{},on_title{},level_known{},loaded_preferences{};
    std::atomic<Exec> original{},original_open{};
    void* volatile* slot{};
    void* volatile* open_slot{};
    const unsigned char *click_code{},*close_code{},*back_code{},*load_code{};

    const wchar_t* message(const wchar_t* en,const wchar_t* cn) const {return zh?cn:en;}
    const wchar_t* message(Phrase phrase) const {return message(phrase.english,phrase.chinese);}
    void focus(Address context,Address widget) const {
        if(!widget)return;
        if(const auto field=ui.member(context,"LastSelectedWidget","ObjectProperty",8)){
            std::memcpy(reinterpret_cast<void*>(context+at<unsigned>(field,76)),&widget,8);
        }
        event(widget,ui.get(TitleUi::focus),nullptr);
    }
    void clear_controls() {
        for(auto& c:controls){
            if(const auto row=ui.get(c.row))event(row,ui.get(remove_parent),nullptr);
            c={};
        }
        control_count=0;
        friend_portraits={};
        address_field={};
        port_field={};
        seat_picker={};
        notice={};
    }
    void close_skin() {
        if(const auto root=ui.get(skin_root)){
            event(root,ui.get(remove_parent),nullptr);
            if(const auto frame=ui.get(frame_slot))event(frame,ui.get(canvas_position),frame_position.data());
        }
        skin_root=portrait_image=skin_name=skin_choose=skin_left=skin_right={};
    }
    void restore_layout() {
        if(!lan_layout)return;
        if(const auto slot=ui.get(frame_slot))event(slot,ui.get(canvas_set_layout),&stock_frame);
        if(const auto slot=ui.object(ui.get(rows),"Slot"))event(slot,ui.get(canvas_set_layout),&stock_rows);
        lan_layout=false;
    }
    void restore_panel(bool close) {
        const auto context=ui.get(panel);
        if(const auto field=ui.member(context,"LastSelectedWidget","ObjectProperty",8)){
            const auto previous=ui.get(previous_focus);
            std::memcpy(reinterpret_cast<void*>(context+at<unsigned>(field,76)),&previous,8);
        }
        previous_focus={};
        clear_controls();
        close_skin();
        restore_layout();
        if(const auto caption=ui.get(heading);caption&&!original_heading.empty())ui.label(caption,original_heading.c_str());
        original_heading.clear();
        heading=frame_slot={};
        for(auto& h:hidden){
            if(const auto widget=ui.get(h.widget))ui.visibility(widget,h.visibility);
            h={};
        }
        panel={};
        rows={};
        model={};
        screen=Screen::home;
        friends.clear();
        invited.clear();
        friend_choice=invite_room=0;
        feedback={};
        if(close&&context&&ui.shown(context))event(context,ui.get(options_close),nullptr);
    }
    void close_and_focus(bool close) {
        restore_panel(close);
        if(const auto widget=ui.get(entry))event(widget,ui.get(TitleUi::focus),nullptr);
    }
    bool track_control(Address widget,Action action,Address row=0) {
        if(!widget||control_count==controls.size())return false;
        auto& control=controls[control_count++];
        ui.remember(control.widget,widget);
        if(row)ui.remember(control.row,row);
        control.action=action;
        return true;
    }
    bool add_row(Address widget,Action action,float row_height=50.f,Address action_widget=0) {
        const auto context=ui.get(panel),box=ui.get(rows);
        const auto tree=ui.object(context,"WidgetTree");
        if(!widget||!tree||!box||control_count==controls.size())return false;
        const auto row=ui.spawn(tree,ui.get(size_type));
        if(!row||!ui.add(row,widget))return false;
        event(row,ui.get(height),&row_height);
        if(!ui.add(box,row,add_vertical))return false;
        return track_control(action_widget?action_widget:widget,action,row);
    }
    Address add_button(const wchar_t* caption,Action action,float row_height=50.f) {
        const auto context=ui.get(panel),source=ui.get(model);
        const auto button=context&&source?ui.button(context,source,caption):0;
        return add_row(button,action,row_height)?button:0;
    }
    Address stock_label(const wchar_t* caption) {
        const auto tree=ui.object(ui.get(panel),"WidgetTree");
        const auto sample=ui.first_text(ui.object(ui.get(model),"Subcontent"));
        if(!tree||!sample)return 0;
        const auto label=ui.spawn(tree,ui.get(text_type));
        if(!label)return 0;
        auto style=ui.object(sample,"Style","ClassProperty");
        if(style)event(label,ui.get(text_style),&style);
        ui.copy_struct(label,sample,"Font",104);
        ui.label(label,caption);
        return label;
    }
    void center_row(Address child,Margin padding) const {
        const auto slot=ui.object(child,"Slot");
        const auto row_slot=ui.object(ui.get(controls[control_count-1].row),"Slot");
        unsigned char center=2;
        if(slot){
            event(slot,ui.get(size_horizontal),&center);
            event(slot,ui.get(size_vertical),&center);
        }
        if(row_slot)event(row_slot,ui.get(slot_padding),padding.data());
    }
    Address centered_label(const wchar_t* text,float row_height,float wrap=0.f) {
        const auto label=stock_label(text);
        if(!label)return 0;
        if(wrap){
            if(const auto field=ui.member(label,"WrapTextAt","FloatProperty",4))
                std::memcpy(reinterpret_cast<void*>(label+at<unsigned>(field,76)),&wrap,sizeof(wrap));
        }
        if(!add_row(label,Action::none,row_height))return 0;
        center_row(label,{16.f,4.f,16.f,4.f});
        return label;
    }
    void notify(Phrase text) {
        feedback=text;
        if(const auto label=ui.get(notice))ui.label(label,message(text));
        else if(const auto added=centered_label(message(text),55.f,440.f))ui.remember(notice,added);
    }
    bool add_field(UE::WeakKey& key,const wchar_t* value) {
        const auto context=ui.get(panel);
        const auto tree=ui.object(context,"WidgetTree");
        const auto edit=tree?ui.spawn(tree,ui.get(edit_type)):0;
        if(!edit)return false;
        ui.edit_style(edit,ui.first_text(ui.object(ui.get(model),"Subcontent")));
        ui.label(edit,value,edit_set);
        if(!add_row(edit,Action::none,40.f))return false;
        center_row(edit,{35.f,2.f,35.f,8.f});
        ui.remember(key,edit);
        return true;
    }
    std::wstring seats_caption() const {
        return std::wstring(message(L"Players: ",L"玩家人数："))+std::to_wstring(screen==Screen::host_steam?steam_seats:lan_seats);
    }
    bool friend_screen() const {return screen==Screen::join_friends||screen==Screen::invite_friends;}
    bool room_has_space() const {return status.room.members<status.room.limit;}
    Address sized(Address tree,Address child,float width,float row_height) const {
        const auto box=ui.spawn(tree,ui.get(size_type));
        if(!box||(child&&!ui.add(box,child)))return 0;
        event(box,ui.get(size_width),&width);
        event(box,ui.get(height),&row_height);
        return box;
    }
    bool add_column(Address row,Address widget,Margin padding={},bool stretch=false) const {
        const auto slot=widget?ui.add(row,widget):0;
        if(!slot)return false;
        unsigned char centered=2;
        ChildSize size{1.f,static_cast<unsigned char>(stretch)};
        event(slot,ui.get(horizontal_slot_size),&size);
        event(slot,ui.get(horizontal_slot_padding),padding.data());
        event(slot,ui.get(horizontal_slot_vertical),&centered);
        return true;
    }
    bool add_seat_picker() {
        const auto context=ui.get(panel),source=ui.get(model),tree=ui.object(context,"WidgetTree");
        if(!tree||!source)return false;
        const auto row=ui.spawn(tree,ui.get(horizontal_box_type));
        const auto fewer=ui.button(context,source,L"-"),more=ui.button(context,source,L"+");
        const auto label=stock_label(seats_caption().c_str());
        const auto caption=label?sized(tree,label,210.f,arrow_height):0;
        if(!row||!fewer||!more||!caption)return false;
        unsigned char center=2;
        const auto slot=ui.object(label,"Slot");
        if(!slot)return false;
        event(slot,ui.get(size_horizontal),&center);
        event(slot,ui.get(size_vertical),&center);
        if(!add_column(row,sized(tree,fewer,arrow_width,arrow_height))||!add_column(row,caption)||
           !add_column(row,sized(tree,more,arrow_width,arrow_height))||
           !add_row(row,Action::fewer_seats,arrow_height,fewer))return false;
        center_row(row,{});
        if(!track_control(more,Action::more_seats))return false;
        ui.remember(seat_picker,label);
        return true;
    }
    bool add_friend(const Social::Friend& item,bool inviting,unsigned index,Address& first) {
        const auto context=ui.get(panel),tree=ui.object(context,"WidgetTree");
        if(!tree)return false;
        const auto row=ui.spawn(tree,ui.get(horizontal_box_type)),image=ui.spawn(tree,ui.get(image_type));
        const auto name=stock_label(item.name.empty()?message(L"Steam friend",L"Steam 好友"):item.name.c_str());
        const auto fit=ui.spawn(tree,ui.get(scale_type));
        if(!row||!image||!name||!fit)return false;
        const auto name_slot=ui.add(fit,name);
        if(!name_slot)return false;
        unsigned char left=1,down_only=1,scale_to_fit=2;
        event(name_slot,ui.get(scale_slot_horizontal),&left);
        event(fit,ui.get(set_stretch),&scale_to_fit);
        event(fit,ui.get(stretch_direction),&down_only);
        ui.visibility(image,4);
        const bool sent=std::ranges::find(invited,item.id)!=invited.end();
        const bool enabled=inviting?invite_has_space&&item.online&&!item.in_room&&!sent:bool(item.lobby);
        auto state=item.online?Phrase{L"Online",L"在线"}:Phrase{L"Offline",L"离线"};
        if(inviting&&item.in_room)state={L"In room",L"已在房间"};
        else if(inviting&&sent)state={L"Invited",L"已邀请"};
        else if(item.playing)state={L"Playing",L"游戏中"};
        const auto status_label=stock_label(message(state));
        const auto button=enabled?ui.button(context,ui.get(model),inviting?message(L"Invite",L"邀请"):message(L"Join",L"加入")):0;
        if(!status_label||(enabled&&!button))return false;
        const auto state_box=sized(tree,status_label,108.f,38.f);
        if(!state_box)return false;
        unsigned char right=3,centered=2;
        const auto state_slot=ui.object(status_label,"Slot");
        if(!state_slot)return false;
        event(state_slot,ui.get(size_horizontal),&right);
        event(state_slot,ui.get(size_vertical),&centered);
        if(!add_column(row,sized(tree,image,32.f,32.f),{0.f,0.f,10.f,0.f})||
           !add_column(row,fit,{0.f,0.f,8.f,0.f},true)||
           !add_column(row,state_box,{0.f,0.f,10.f,0.f})||
           !add_column(row,sized(tree,button,98.f,38.f))||
           !add_row(row,button?Action::friend_pick:Action::none,42.f,button))return false;
        auto margin=Margin{12.f,0.f,12.f,0.f};
        const auto slot=ui.object(ui.get(controls[control_count-1].row),"Slot");
        if(!slot)return false;
        event(slot,ui.get(slot_padding),margin.data());
        friend_portraits[index].id=item.id;
        ui.remember(friend_portraits[index].image,image);
        if(button){
            controls[control_count-1].friend_id=item.id;
            if(!first)first=button;
        }
        return true;
    }
    void refresh_avatars() {
        const auto account=social.account();
        if(!account)return;
        if(account!=avatar_account){avatar_cache={};avatar_account=account;}
        if(!ui.get(steam_friend_avatar)||!ui.get(steam_get_friends)||!ui.get(steam_friends_default))ui.step();
        for(auto& portrait:friend_portraits){
            const auto image=ui.get(portrait.image);
            if(!image)continue;
            const auto handle=social.avatar_handle(portrait.id);
            if(handle<0||handle==portrait.handle)continue;
            Address texture{};
            if(handle){
                for(const auto& cached:avatar_cache)if(cached.id==portrait.id&&cached.handle==handle){
                    texture=ui.get(cached.texture);
                    if(texture)break;
                }
                if(!texture){
                    texture=ui.friend_avatar(portrait.id,handle);
                    if(!texture)continue;
                    auto& cached=avatar_cache[avatar_cursor++%avatar_cache.size()];
                    cached={portrait.id,{},handle};
                    ui.remember(cached.texture,texture);
                }
            }
            struct {Address texture;bool match_size;} args{texture,false};
            event(image,ui.get(set_texture),&args);
            portrait.handle=handle;
        }
    }
    bool add_friend_footer(const wchar_t* text,unsigned max_page,Address first,Action page_focus) {
        const auto context=ui.get(panel),tree=ui.object(context,"WidgetTree"),source=ui.get(model);
        if(!tree||!source)return false;
        const auto label=centered_label(text,32.f,440.f);
        if(!label)return false;
        const auto text_slot=ui.object(label,"Slot");
        const auto status_slot=ui.object(ui.get(controls[control_count-1].row),"Slot");
        if(!text_slot||!status_slot)return false;
        // Use the remaining height above the footer, with the notice against its bottom.
        ChildSize remaining{1.f,1};
        unsigned char bottom=3;
        event(status_slot,ui.get(slot_size),&remaining);
        event(text_slot,ui.get(size_vertical),&bottom);
        ui.remember(notice,label);

        const auto bar=ui.spawn(tree,ui.get(horizontal_box_type));
        const auto back=ui.button(context,source,message(L"Back",L"返回"));
        const auto previous=friend_page?ui.button(context,source,left_arrow):0;
        const auto next=friend_page<max_page?ui.button(context,source,right_arrow):0;
        if(!bar||!back||(friend_page&&!previous)||(friend_page<max_page&&!next)||
           !add_column(bar,sized(tree,previous,arrow_width,arrow_height))||
           !add_column(bar,back,{},true)||
           !add_column(bar,sized(tree,next,arrow_width,arrow_height))||
           !add_row(bar,Action::back,arrow_height,back))return false;
        const auto bar_slot=ui.object(ui.get(controls[control_count-1].row),"Slot");
        if(!bar_slot)return false;
        Margin inset{12.f,0.f,12.f,6.f};
        event(bar_slot,ui.get(slot_padding),inset.data());
        if((previous&&!track_control(previous,Action::friend_previous))||
           (next&&!track_control(next,Action::friend_next)))return false;
        const auto page_button=page_focus==Action::friend_previous?previous:page_focus==Action::friend_next?next:0;
        const auto fallback=first?first:next?next:previous?previous:back;
        focus(context,page_button?page_button:fallback);
        return true;
    }
    bool render_friends(Action page_focus=Action::none){
        clear_controls();
        const bool inviting=screen==Screen::invite_friends;
        const auto context=ui.get(panel);
        if(!context)return false;
        if(const auto caption=ui.get(heading))ui.label(caption,inviting?message(L"Invite players",L"邀请玩家"):
                                                                     message(L"Join Steam friends",L"加入好友"));
        const auto max_page=friends.empty()?0u:static_cast<unsigned>((friends.size()-1)/friends_per_page);
        friend_page=std::min(friend_page,max_page);
        Address first{};
        const auto begin=friend_page*friends_per_page;
        for(unsigned i=begin;i<friends.size()&&i<begin+friends_per_page;++i)
            if(!add_friend(friends[i],inviting,i-begin,first))return false;
        if(begin<friends.size())refresh_avatars();
        std::wstring_view text=feedback.english?message(feedback):L"";
        if(text.empty()&&inviting&&!invite_has_space)
            text=message(L"The room is full.",L"房间已满。");
        if(text.empty())switch(friend_access){
        case FriendAccess::launch_context:
            text=message(L"Launch QueensDomain from your Steam library.",L"请从 Steam 库启动游戏。");break;
        case FriendAccess::sign_in:
            text=message(L"Connecting to Steam friends…",L"正在连接 Steam 好友……");break;
        case FriendAccess::unavailable:
            text=message(L"Steam friends are unavailable.",L"暂时无法读取好友。");break;
        case FriendAccess::ready:
            if(friends.empty())text=message(L"No Steam friends to show.",L"好友列表为空。");
            break;
        }
        return add_friend_footer(text.data(),max_page,first,page_focus);
    }
    bool refresh_friends(ULONGLONG now){
        if(!friend_screen()||joining||now<next_friends)return true;
        next_friends=now+1500;
        std::vector<Social::Friend> current;
        const auto access=social.list_friends(invite_room,current);
        const bool space=screen==Screen::invite_friends&&room_has_space();
        if(access==friend_access&&space==invite_has_space&&current==friends){refresh_avatars();return true;}
        friend_access=access;
        invite_has_space=space;
        friends=std::move(current);
        return render_friends();
    }
    bool show_friends(Screen mode,Steam::SteamId room=0){
        close_skin();
        screen=mode;
        invite_room=room;
        invite_has_space=mode==Screen::invite_friends&&room_has_space();
        friend_page=0;
        friend_choice=0;
        invited.clear();
        friends.clear();
        feedback={};
        friend_access=social.list_friends(room,friends);
        next_friends=GetTickCount64()+1500;
        return render_friends();
    }
    bool use_friend_action(Action action){
        if(action==Action::friend_previous||action==Action::friend_next){
            if(action==Action::friend_next)++friend_page;
            else if(friend_page)--friend_page;
            return render_friends(action);
        }
        const auto id=std::exchange(friend_choice,0);
        if(!id||joining)return true;
        const auto selected=std::ranges::find(friends,id,&Social::Friend::id);
        if(selected==friends.end())return true;
        if(screen==Screen::invite_friends){
            if(std::ranges::find(invited,id)!=invited.end())return true;
            const auto sent=social.invite_friend(invite_room,id);
            notify(sent?Phrase{L"Invitation sent.",L"Steam 邀请已发送。"}:
                        Phrase{L"Could not send the invitation.",L"无法发送 Steam 邀请。"});
            if(sent){invited.push_back(id);return render_friends();}
        }else{
            const auto lobby=social.friend_lobby(id);
            if(!lobby||lobby!=selected->lobby||!submit({Room::Operation::join,0,0,lobby})){
                notify({L"This room is unavailable.",L"此房间暂时无法加入。"});
                next_friends=0;
                return true;
            }
            steam_target=lobby;
            joining=true;
            notify({L"Joining Steam game...",L"正在加入 Steam 游戏……"});
        }
        return true;
    }
    bool open_skin() {
        const auto context=ui.get(panel),source=ui.get(model),frame=ui.get(frame_slot);
        const auto tree=ui.object(context,"WidgetTree");
        const auto selection=ui.object(context,"OptionsSelectMenu");
        if(!tree||!source||!frame||!ui.is(selection,ui.get(canvas_type))||
           !ui.method(add_canvas,16,{{"Content","ObjectProperty",0,8},{"ReturnValue","ObjectProperty",8,8}})||
           !ui.method(canvas_get_position,8,{{"ReturnValue","StructProperty",0,8}})||
           !ui.method(canvas_position,8,{{"InPosition","StructProperty",0,8}})||
           !ui.method(canvas_alignment,8,{{"InAlignment","StructProperty",0,8}})||
           !ui.method(canvas_size,8,{{"InSize","StructProperty",0,8}})||
           !ui.method(canvas_anchors,16,{{"InAnchors","StructProperty",0,16}})||
           !ui.method(size_width,4,{{"InWidthOverride","FloatProperty",0,4}})||
           !ui.method(size_horizontal,1,{{"InHorizontalAlignment","ByteProperty",0,1}})||
           !ui.method(size_vertical,1,{{"InVerticalAlignment","ByteProperty",0,1}}))return false;
        const auto root=ui.spawn(tree,ui.get(vertical_box_type)),bar=ui.spawn(tree,ui.get(horizontal_box_type));
        const auto image=ui.spawn(tree,ui.get(image_type)),name=stock_label(L"");
        const auto left=ui.button(context,source,left_arrow),right=ui.button(context,source,right_arrow);
        const auto choose=ui.button(context,source,message(L"Select",L"选择"));
        if(!root||!bar||!image||!name||!left||!right||!choose)return false;
        unsigned char centered=2;
        const auto size=[&](Address parent,Address content,float width,float height_value,bool center){
            const auto box=ui.spawn(tree,ui.get(size_type));
            if(!box)return false;
            const auto slot=ui.add(box,content);
            if(!slot)return false;
            if(center){
                event(slot,ui.get(size_horizontal),&centered);
                event(slot,ui.get(size_vertical),&centered);
            }
            event(box,ui.get(size_width),&width);
            event(box,ui.get(height),&height_value);
            const bool vertical_parent=parent==root;
            const auto placed=ui.add(parent,box,vertical_parent?add_vertical:add_child);
            if(placed&&vertical_parent)event(placed,ui.get(slot_horizontal),&centered);
            return placed!=0;
        };
        const auto bar_slot=ui.add(root,bar,add_vertical);
        if(!bar_slot||!size(bar,left,arrow_width,arrow_height,false)||!size(bar,name,260.f,45.f,true)||
           !size(bar,right,arrow_width,arrow_height,false)||!size(root,image,390.f,520.f,false)||
           !size(root,choose,260.f,45.f,false))return false;
        event(bar_slot,ui.get(slot_horizontal),&centered);
        ui.visibility(image,4);
        const auto slot=ui.add(selection,root,add_canvas);
        if(!slot)return false;
        float anchors[]{.5f,.5f,.5f,.5f},alignment[]{.5f,.5f},position[]{280.f,0.f},dimensions[]{400.f,640.f};
        event(slot,ui.get(canvas_anchors),anchors);
        event(slot,ui.get(canvas_alignment),alignment);
        event(slot,ui.get(canvas_position),position);
        event(slot,ui.get(canvas_size),dimensions);
        event(frame,ui.get(canvas_get_position),frame_position.data());
        float panel_position[]{-265.f,frame_position[1]};
        event(frame,ui.get(canvas_position),panel_position);
        ui.remember(skin_root,root);
        ui.remember(portrait_image,image);
        ui.remember(skin_name,name);
        ui.remember(skin_choose,choose);
        ui.remember(skin_left,left);
        ui.remember(skin_right,right);
        preview_skin=current_skin();
        refresh_skin();
        return true;
    }
    void refresh_skin() {
        constexpr std::array paths{
            "/Game/QDCoopSkins/Portraits/Prince.Prince","/Game/QDCoopSkins/Portraits/Arjaot.Arjaot",
            "/Game/QDCoopSkins/Portraits/Ippibi.Ippibi","/Game/QDCoopSkins/Portraits/Ujji.Ujji",
            "/Game/QDCoopSkins/Portraits/Imba.Imba"
        };
        constexpr const wchar_t* names[]{L"Prince",L"Arjaot",L"Ippibi",L"Ujji",L"Imba"};
        const auto image=ui.get(portrait_image),name=ui.get(skin_name),choose=ui.get(skin_choose);
        if(!image||!name||!choose||!Player::valid_skin(preview_skin))return;
        const auto index=preview_skin-1;
        ui.label(name,!index&&zh?L"王子":names[index]);
        struct {Address texture;bool match_size;} args{ui.portrait(paths[index]),false};
        event(image,ui.get(set_texture),&args);
        ui.visibility(choose,current_skin()==preview_skin?2:0);
    }
    void load_preferences() {
        if(loaded_preferences||!preferences.bind(ui.project_saved_directory()))return;
        loaded_preferences=true;
        backup.start(preferences.directory);
        preferences.lan(lan_address,join_port);
        if(const auto skin=preferences.skin())select_native_skin(skin,true);
    }
    bool set_lan_layout(float frame_height) {
        const auto frame=ui.get(frame_slot),box=ui.object(ui.get(rows),"Slot");
        if(!frame||!box)return false;
        event(frame,ui.get(canvas_layout),&stock_frame);
        event(box,ui.get(canvas_layout),&stock_rows);
        auto frame_layout=stock_frame;
        frame_layout.offsets[2]=516.f;
        frame_layout.offsets[3]=frame_height;
        CanvasLayout box_layout{{0.f,68.f,0.f,10.f},{0.f,0.f,1.f,1.f},{0.f,0.f}};
        event(frame,ui.get(canvas_set_layout),&frame_layout);
        event(box,ui.get(canvas_set_layout),&box_layout);
        lan_layout=true;
        return true;
    }
    bool show(Screen desired) {
        clear_controls();
        close_skin();
        restore_layout();
        feedback={};
        screen=desired;
        const auto context=ui.get(panel);
        if(!context)return false;
        Address first{};
        const bool hosting_lan=screen==Screen::host_lan,joining_lan=screen==Screen::join_lan;
        if(const auto caption=ui.get(heading)){
            const auto text=hosting_lan?message(L"Host game",L"主持游戏"):joining_lan?
                message(L"Join game",L"加入游戏"):message(L"Multiplayer",L"多人游戏");
            ui.label(caption,text);
        }
        if(screen==Screen::home){
            first=add_button(message(L"Host Steam game",L"主持STEAM游戏"),Action::host);
            if(!first||!add_button(message(L"Join Steam game",L"加入STEAM游戏"),Action::friends)||
               !add_button(message(L"Host LAN game",L"主持局域网游戏"),Action::host_lan)||
               !add_button(message(L"Join LAN game",L"加入局域网游戏"),Action::join_lan))return false;
            if(!open_skin())return false;
        }else if(screen==Screen::host_steam){
            if(!add_seat_picker())return false;
            first=add_button(message(L"Choose save",L"选择存档"),Action::confirm_host);
            if(!first)return false;
        }else if(hosting_lan){
            if(!set_lan_layout(440.f)||!centered_label(message(L"Room port",L"房间端口"),30.f)||
               !add_field(port_field,std::to_wstring(host_port).c_str()))return false;
            if(!add_seat_picker())return false;
            first=add_button(message(L"Host game",L"主持游戏"),Action::confirm_host,45.f);
            if(!first)return false;
        }else if(joining_lan){
            if(!set_lan_layout(680.f)||!centered_label(message(L"Host IP address",L"房主 IP 地址"),30.f)||
               !add_field(address_field,lan_address.data())||!centered_label(message(L"Port",L"端口"),30.f)||
               !add_field(port_field,std::to_wstring(join_port).c_str()))return false;
            first=add_button(message(L"Connect",L"连接"),Action::connect,45.f);
            if(!first||!centered_label(message(L"Use 127.0.0.1 on the same PC, or the host’s LAN or Radmin IP on another PC.",
                                               L"同机测试用 127.0.0.1；其他电脑填写房主的局域网或 Radmin IP。"),110.f,440.f))return false;
        }
        if(!add_button(message(L"Back",L"返回"),Action::back,lan_layout?45.f:50.f))return false;
        if(lan_layout){
            const auto status_label=centered_label(L"",hosting_lan?58.f:110.f,440.f);
            if(!status_label||!open_skin())return false;
            ui.remember(notice,status_label);
        }
        focus(context,first);
        return true;
    }
    void button_caption(Address button,const wchar_t* text) const {
        if(const auto label=ui.first_text(ui.object(button,"Subcontent")))ui.label(label,text);
    }
    bool refresh_language(ULONGLONG now) {
        if(now<next_language)return true;
        next_language=now+250;
        const auto language=ui.chinese();
        if(language==zh)return true;
        zh=language;
        if(const auto button=ui.get(entry))button_caption(button,message(L"Multiplayer",L"多人游戏"));
        const auto context=ui.get(panel);
        if(!context||!ui.shown(context))return true;
        if(friend_screen())return render_friends();
        const auto address=ui.get(address_field),port=ui.get(port_field);
        const auto typed_address=address?ui.read_field(address,256):std::wstring{};
        const auto typed_port=port?ui.read_field(port,256):std::wstring{};
        const auto skin=preview_skin;
        const auto text=feedback;
        if(!show(screen))return false;
        if(address)ui.label(ui.get(address_field),typed_address.c_str(),edit_set);
        if(port)ui.label(ui.get(port_field),typed_port.c_str(),edit_set);
        if(ui.get(skin_root)){preview_skin=skin;refresh_skin();}
        if(text.english)notify(text);
        return true;
    }
    bool open_panel(Address title,Address game_instance) {
        if(ui.get(panel))return true;
        auto option=ui.object(title,"Options");
        const auto previous=ui.object(game_instance,"CurrentOptionsMenu");
        if(!ui.is(option,ui.get(button_type))||(previous&&ui.shown(previous)))return false;
        event(title,ui.get(title_options),&option);
        const auto fresh=ui.object(game_instance,"CurrentOptionsMenu");
        return adopt_panel(fresh)&&show(Screen::home);
    }
    bool adopt_panel(Address fresh){
        if(!ui.is(fresh,ui.get(options_type))||!ui.shown(fresh))return false;
        ui.remember(panel,fresh);
        if(const auto selected=ui.object(fresh,"LastSelectedWidget"))ui.remember(previous_focus,selected);
        const auto selection=ui.object(fresh,"OptionsSelectMenu");
        const auto source=ui.object(fresh,"TopLevel_VideoButton");
        if(!ui.is(selection,ui.get(panel_type))||!ui.is(source,ui.get(button_type)))return false;
        const auto frame=ui.child(selection,0);
        if(!ui.is(frame,ui.get(panel_type)))return false;
        const auto canvas=ui.child(frame,0);
        if(!ui.is(canvas,ui.get(panel_type)))return false;
        const auto source_row=ui.parent(source);
        if(!ui.is(source_row,ui.get(size_type)))return false;
        const auto box=ui.parent(source_row);
        if(!ui.is(box,ui.get(vertical_box_type))||ui.parent(box)!=canvas)return false;
        const auto count=ui.count(box);
        if(count<1||static_cast<unsigned>(count)>hidden.size()||!ui.is(ui.child(box,0),ui.get(image_type)))return false;
        ui.remember(frame_slot,ui.object(frame,"Slot"));
        for(int i=0;i<ui.count(canvas);++i){
            const auto widget=ui.child(canvas,i);
            if(ui.is(widget,ui.get(text_type))){
                ui.remember(heading,widget);
                original_heading=ui.caption(widget);
                break;
            }
        }

        ui.remember(rows,box);
        ui.remember(model,source);
        for(int i=1;i<count;++i){
            const auto widget=ui.child(box,i);
            if(!widget)return false;
            ui.remember(hidden[i].widget,widget);
            hidden[i].visibility=ui.visibility(widget);
            ui.visibility(widget,1); // ESlateVisibility::Collapsed.
        }
        return true;
    }
    bool select_save_page(Address title,SaveMode mode) {
        auto load=ui.object(title,"LoadGameButton");
        if(!ui.is(load,ui.get(button_type)))return false;
        restore_panel(true);
        event(title,ui.get(title_load),&load);
        const auto save=ui.member(title,"InSaveMenu","BoolProperty",1);
        if(!save||!UE::get_bool(reinterpret_cast<void*>(save),reinterpret_cast<void*>(title),0))return false;
        save_mode=mode;
        save_selected=false;
        return true;
    }
    void refresh_saves(Address title) {
        // The title can cache slots before the Steam user's save directory is available.
        const auto account=social.account();
        if(!account||account==save_account||!ui.method(title_load_saves,0,{})||!ui.method(title_refresh_slots,0,{}))return;
        save_account=account;
        event(title,ui.get(title_load_saves),nullptr);
        event(title,ui.get(title_refresh_slots),nullptr);
    }
    void stop_join() {
        if(joining||save_mode==SaveMode::guest)submit({Room::Operation::leave});
        joining=false;
        save_mode=SaveMode::none;
        save_selected=false;
        host_request={};
        steam_target=0;
    }
    bool read_port(unsigned& port) {
        const auto field=ui.get(port_field);
        if(!field)return false;
        if(Room::parse_lan_port(ui.read_field(field,5),port))return true;
        notify({L"Port must be an integer from 1024 to 65535.",L"端口须为 1024 至 65535 的整数。"});
        return false;
    }
    void connect_lan() {
        const auto field=ui.get(address_field);
        if(!field||joining)return;
        Room::Request request{Room::Operation::join_lan};
        if(!Room::set_ipv4(ui.read_field(field,15),request.ipv4)){
            notify({L"Enter a valid IPv4 address.",L"请输入有效的 IPv4 地址。"});
            return;
        }
        if(!read_port(request.port))return;
        if(!submit(request)){
            notify({L"Could not start the connection.",L"无法启动连接。"});
            return;
        }
        lan_address=request.ipv4;
        join_port=request.port;
        preferences.remember_lan(request);
        steam_target=0;
        joining=true;
        notify({L"Connecting…",L"正在连接……"});
    }
    bool in_save_menu(Address title) const {
        const auto field=ui.member(title,"InSaveMenu","BoolProperty",1);
        return field&&UE::get_bool(reinterpret_cast<void*>(field),reinterpret_cast<const void*>(title),0);
    }
    void accept_steam(Address title,Address instance) {
        if(joining||save_mode!=SaveMode::none||in_save_menu(title))return;
        const auto options=ui.object(instance,"CurrentOptionsMenu");
        if(options&&ui.shown(options)&&options!=ui.get(panel))return;
        const auto request=read_invitation();
        if(!request.ticket)return;
        const bool allowed=invitation_gate.offered!=request.ticket||invitation_gate.confirmed==request.ticket;
        if(!respond_to_invitation(request.ticket,allowed))return;
        invitation_gate={};
        if(!allowed)return;
        steam_target=request.lobby;
        joining=true;
        if(open_panel(title,instance))notify({L"Joining Steam game...",L"正在加入 Steam 游戏……"});
    }
    void service_join(Address title) {
        if(!joining)return;
        if(status.result==Room::Result::closed||status.result==Room::Result::rejected){
            joining=false;
            if(steam_target){
                steam_target=0;
                notify({L"Could not join the Steam room.",L"无法加入 Steam 房间。"});
            }else if(ui.get(address_field)){
                notify({L"Connection failed. Check the host and port.",L"连接失败。请检查房主地址和端口。"});
            }
            return;
        }
        if(status.result!=Room::Result::running)return;
        const auto phase=static_cast<Steam::Phase>(status.room.phase);
        const bool ready=steam_target?status.room.lobby==steam_target&&status.room.nonce&&status.room.epoch&&
            status.room.enter_response==1&&(phase==Steam::Phase::waiting||phase==Steam::Phase::handshake||phase==Steam::Phase::active):
            lan_status.room&&(lan_status.phase==Lan::Phase::waiting||lan_status.phase==Lan::Phase::active);
        if(!ready)return;
        joining=false;
        if(!select_save_page(title,SaveMode::guest))submit({Room::Operation::leave});
    }
    void dispatch(Address title,Address instance) {
        const auto action=pending;
        pending=Action::none;
        switch(action) {
        case Action::open:
            if(!open_panel(title,instance))close_and_focus(true);
            break;
        case Action::host:
            if(!show(Screen::host_steam))close_and_focus(true);
            break;
        case Action::friends:
            if(!show_friends(Screen::join_friends))close_and_focus(true);
            break;
        case Action::host_lan:
            if(!show(Screen::host_lan))close_and_focus(true);
            break;
        case Action::join_lan:
            if(!show(Screen::join_lan))close_and_focus(true);
            break;
        case Action::fewer_seats:
        case Action::more_seats: {
            auto& seats=screen==Screen::host_steam?steam_seats:lan_seats;
            if(action==Action::fewer_seats&&seats>2)--seats;
            if(action==Action::more_seats&&seats<4)++seats;
            if(const auto label=ui.get(seat_picker))ui.label(label,seats_caption().c_str());
            break;
        }
        case Action::confirm_host:
            if(screen==Screen::host_steam){
                if(!social.account()){
                    notify({L"Start the game from Steam and sign in to use your Steam saves.",
                            L"请登录 Steam 并从库启动，以读取该账号的存档。"});
                    break;
                }
                host_request={Room::Operation::host,steam_seats,1};
                select_save_page(title,SaveMode::host);
            }else if(read_port(host_port)){
                host_request={Room::Operation::host_lan,lan_seats};
                host_request.port=host_port;
                select_save_page(title,SaveMode::host);
            }
            break;
        case Action::friend_pick:
        case Action::friend_previous:
        case Action::friend_next:
            if(!use_friend_action(action))close_and_focus(true);
            break;
        case Action::connect:
            connect_lan();
            break;
        case Action::previous_skin:
            preview_skin=preview_skin==1?5:preview_skin-1;
            refresh_skin();
            break;
        case Action::next_skin:
            preview_skin=preview_skin==5?1:preview_skin+1;
            refresh_skin();
            break;
        case Action::select_skin:
            if(preview_skin!=current_skin()){
                if(preferences.skin(preview_skin)&&select_native_skin(preview_skin))refresh_skin();
                else notify({L"Could not save the skin selection.",L"无法保存皮肤选择。"});
            }
            break;
        case Action::back:
            stop_join();
            if(screen==Screen::home||!show(Screen::home))close_and_focus(true);
            break;
        case Action::closed:
            stop_join();
            close_and_focus(false);
            break;
        case Action::canceled:
            stop_join();
            if(!open_panel(title,instance))close_and_focus(true);
            break;
        case Action::none:
            break;
        }
    }
    const unsigned char* wrapper(Symbol id,Symbol graph,unsigned entry_point) const {
        const auto fn=ui.get(id);
        const auto script=at<const unsigned char*>(fn,96);
        if(!script||at<int>(fn,104)!=18||script[0]!=0x46||script[9]!=0x1d||
           at<Address>(reinterpret_cast<Address>(script),1)!=ui.get(graph)||
           at<unsigned>(reinterpret_cast<Address>(script),10)!=entry_point||
           script[14]!=0x16||script[15]!=0x04||script[16]!=0x0b||script[17]!=0x53)return nullptr;
        return script+1;
    }
    void receive(void* context,void* frame) {
        if(!context||!frame||GetCurrentThreadId()!=thread)return;
        const auto stack=reinterpret_cast<Address>(frame);
        const auto object=reinterpret_cast<Address>(context);
        if(at<Address>(stack,24)!=object)return;
        const auto code=at<const unsigned char*>(stack,32);
        const auto node=at<Address>(stack,16);
        if(code==click_code&&node==ui.get(click)){
            if(ui.get(entry)==object)pending=Action::open;
            receive_control(object);
        }else if(code==close_code&&node==ui.get(options_close)&&ui.get(panel)==object){
            pending=Action::closed;
        }else if(code==back_code&&node==ui.get(title_back)&&ui.get(ui.active_title)==object&&save_mode!=SaveMode::none){
            pending=Action::canceled;
        }
    }
    void receive_control(Address object){
        if(object&&ui.get(skin_root)){
            if(object==ui.get(skin_left)){pending=Action::previous_skin;return;}
            if(object==ui.get(skin_right)){pending=Action::next_skin;return;}
            if(object==ui.get(skin_choose)&&preview_skin!=current_skin()){pending=Action::select_skin;return;}
        }
        for(const auto& c:controls)if(c.action!=Action::none&&ui.get(c.widget)==object){
            pending=c.action;
            friend_choice=c.friend_id;
            return;
        }
    }
    void select_save(void* frame) {
        if(save_mode==SaveMode::none||!frame||GetCurrentThreadId()!=thread)return;
        const auto stack=reinterpret_cast<Address>(frame);
        if(at<const unsigned char*>(stack,32)==load_code&&at<Address>(stack,16)==ui.get(load_slot)&&
           at<Address>(stack,24)==ui.get(ui.active_title))save_selected=true;
    }
    bool attach_open(Exec observer) {
        if(open_slot)return true;
        const auto fn=ui.get(load_slot),open=ui.get(open_level);
        const auto script=at<const unsigned char*>(fn,96);
        if(!script||at<int>(fn,104)!=504||script[367]!=0x68||
           at<Address>(reinterpret_cast<Address>(script),368)!=open)return false;
        const auto entry=reinterpret_cast<void*>(ui.image+0x2dc1510);
        constexpr unsigned char prefix[]{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x18,0x55,0x57,0x41,0x56};
        auto cell=reinterpret_cast<void* volatile*>(open+216);
        MEMORY_BASIC_INFORMATION memory{};
        if(*cell!=entry||std::memcmp(entry,prefix,sizeof(prefix))||
           !VirtualQuery(reinterpret_cast<void*>(open+216),&memory,sizeof(memory))||memory.State!=MEM_COMMIT||
           !(memory.Protect&PAGE_READWRITE)||(memory.Protect&PAGE_GUARD))return false;
        load_code=script+376;
        original_open.store(reinterpret_cast<Exec>(entry),std::memory_order_release);
        if(InterlockedCompareExchangePointer(cell,reinterpret_cast<void*>(observer),entry)!=entry)return false;
        open_slot=cell;
        return true;
    }
    bool attach(Exec observer) {
        if(slot)return true;
        click_code=wrapper(click,button_graph,1337);
        close_code=wrapper(options_close,options_graph,1505);
        back_code=wrapper(title_back,title_graph,6854);
        if(!click_code||!close_code||!back_code)return false;
        const auto address=ui.image+0x4a62e10+0x46*sizeof(void*);
        auto cell=reinterpret_cast<void**>(address);
        const auto expected=reinterpret_cast<void*>(ui.image+0x11b6ec0);
        constexpr unsigned char prefix[]{0x48,0x83,0xec,0x38,0x48,0x8b,0x42,0x20,0x4c,0x8b,0xd1,0x4d};
        if(*cell!=expected||std::memcmp(expected,prefix,sizeof(prefix)))return false;
        DWORD old{},ignored{};
        if(!VirtualProtect(cell,sizeof(void*),PAGE_READWRITE,&old))return false;
        original.store(reinterpret_cast<Exec>(expected),std::memory_order_release);
        const auto hook=reinterpret_cast<void*>(observer);
        const auto prior=InterlockedCompareExchangePointer(cell,hook,expected);
        if(!VirtualProtect(cell,sizeof(void*),old,&ignored)){
            InterlockedCompareExchangePointer(cell,expected,hook);
            VirtualProtect(cell,sizeof(void*),old,&ignored);
            return false;
        }
        if(prior!=expected)return false;
        slot=cell;
        return true;
    }
    void detach(Exec observer,Exec open_observer) {
        if(open_slot){
            const auto expected=reinterpret_cast<void*>(original_open.load(std::memory_order_acquire));
            InterlockedCompareExchangePointer(open_slot,expected,reinterpret_cast<void*>(open_observer));
            open_slot=nullptr;
        }
        if(!slot)return;
        auto cell=const_cast<void**>(slot);
        DWORD old{},ignored{};
        if(!VirtualProtect(cell,sizeof(void*),PAGE_READWRITE,&old))return;
        const auto expected=reinterpret_cast<void*>(original.load(std::memory_order_acquire));
        InterlockedCompareExchangePointer(cell,expected,reinterpret_cast<void*>(observer));
        VirtualProtect(cell,sizeof(void*),old,&ignored);
        slot=nullptr;
    }
    void tick(Address engine,ULONGLONG now,Exec observer,Exec open_observer) {
        if(now<next_step&&pending==Action::none)return;
        next_step=now+100;
        if(!ui.ready){
            ui.step();
            if(!ui.ready){next_step=now+(ui.cursor?8:100);return;}
        }
        const auto instance=ui.object(engine,"GameInstance");
        if(!instance)return;
        load_preferences();
        auto title=ui.get(ui.active_title);
        on_title=ui.title_level(instance,nullptr,&level_known);
        if(!level_known)return;
        if(!on_title) {
            if(slot){
                restore_panel(false);
                if(const auto widget=ui.get(entry))event(widget,ui.get(remove_parent),nullptr);
                entry={};
                ui.active_title={};
                ui.title_cursor=0;
                save_account=0;
                if(save_mode==SaveMode::host&&save_selected&&pending!=Action::canceled)submit(host_request);
                if(joining||(save_mode==SaveMode::guest&&(!save_selected||pending==Action::canceled)))
                    submit({Room::Operation::leave});
                joining=false;
                save_mode=SaveMode::none;
                save_selected=false;
                host_request={};
                steam_target=0;
                pending=Action::none;
                detach(observer,open_observer);
            }
            return;
        }
        if(!title||!ui.shown(title)){
            restore_panel(false);
            if(const auto widget=ui.get(entry))event(widget,ui.get(remove_parent),nullptr);
            entry={};
            ui.active_title={};
            save_account=0;
            if(!ui.get(title_type)||!ui.get(click)||!ui.get(load_slot))ui.ready=false;
            if(!ui.ready)return;
            title=ui.find_title();
            if(!title){next_step=now+(ui.title_cursor?8:50);return;}
        }
        const auto source=ui.object(title,"LoadGameButton");
        const auto box=source?ui.parent(source):0;
        const auto current_entry=ui.get(entry);
        if(!current_entry||!box||ui.parent(current_entry)!=box){
            if(current_entry)event(current_entry,ui.get(remove_parent),nullptr);
            entry={};
            thread=GetCurrentThreadId();
            if(!attach(observer)||!attach_open(open_observer))return;
            if(!ui.is(box,ui.get(vertical_box_type)))return;
            zh=ui.chinese();
            const auto button=ui.button(title,source,message(L"Multiplayer",L"多人游戏"));
            if(!button||!ui.insert_after(box,source,button))return;
            ui.remember(entry,button);
        }
        refresh_saves(title);
        if(const auto options=ui.get(panel);options&&!ui.shown(options)&&pending==Action::none)pending=Action::closed;
        if(pending==Action::none){
            if(!refresh_language(now))close_and_focus(true);
            service_join(title);
            accept_steam(title,instance);
            if(screen==Screen::join_friends&&!refresh_friends(now))close_and_focus(true);
        }
        if(save_mode==SaveMode::guest&&!save_selected&&
           (status.result==Room::Result::closed||status.result==Room::Result::rejected)){
            const auto from_steam=steam_target!=0;
            stop_join();
            event(title,ui.get(title_back),nullptr);
            if(!open_panel(title,instance)||(!from_steam&&!show(Screen::join_lan)))close_and_focus(true);
            else if(from_steam)notify({L"The Steam room disconnected.",L"Steam 房间已断开。"});
        }
        dispatch(title,instance);
    }
};

Menu menu;
void observe(void* context,void* frame,void* result){
    menu.receive(context,frame);
    menu.original.load(std::memory_order_acquire)(context,frame,result);
}
void observe_open(void* context,void* frame,void* result){
    menu.select_save(frame);
    menu.original_open.load(std::memory_order_acquire)(context,frame,result);
}
}
