// The game thread owns Steam social state and uses the game's existing pipe.
namespace Social=qdcoop::steam_social;
void receive_invitation(Social::Invitation invitation);

struct SteamCallback;
struct SteamCallbackVtable {
    void (*run)(SteamCallback*,void*);
    void (*run_call)(SteamCallback*,void*,bool,std::uint64_t);
    int (*size)(SteamCallback*);
};
struct SteamCallback {
    const SteamCallbackVtable* vtable;
    std::uint8_t flags{};
    int id{};
    bool registered()const { return (flags&1u)!=0; }
};
static_assert(sizeof(SteamCallback)==16&&offsetof(SteamCallback,id)==12);

void on_steam_invite(SteamCallback* callback,void* payload) {
    const auto invitation=Social::decode_invitation(callback->id,payload);
    if(invitation.lobby)receive_invitation(invitation);
}
const SteamCallbackVtable invite_vtable{
    &on_steam_invite,
    [](SteamCallback* callback,void* payload,bool failed,std::uint64_t) {
        if(!failed)on_steam_invite(callback,payload);
    },
    [](SteamCallback* callback) {
        return static_cast<int>(callback->id==Social::lobby_callback?sizeof(Social::LobbyJoin):sizeof(Social::PresenceJoin));
    }
};
SteamCallback lobby_invite{&invite_vtable,0,Social::lobby_callback};
SteamCallback presence_invite{&invite_vtable,0,Social::presence_callback};

enum class Overlay {none,friends,invite};
enum class FriendAccess {ready,launch_context,sign_in,unavailable};
#ifdef QDCOOP_STEAM_SOCIAL_LAB
#include "steam_dispatch_probe.inc"
constexpr char presence_key[]="qdcoop_lab_connect";
#else
constexpr char presence_key[]="connect";
#endif
class SteamSocial {
    struct FriendGame {
        std::uint64_t game{};
        std::uint32_t ip{};
        std::uint16_t game_port{},query_port{};
        Steam::SteamId lobby{};
    };
    static_assert(sizeof(FriendGame)==24&&offsetof(FriendGame,lobby)==16);
    static constexpr int immediate=4;
    HMODULE module_{};
    int (*pipe_)(){};
    int (*initialize_)(char*){};
    void *(*get_friends_)(){},*(*get_utils_)(){},*(*get_user_)(){};
    unsigned (*app_)(void*){};
    Steam::SteamId (*self_)(void*){};
    bool (*logged_on_)(void*){},(*overlay_enabled_)(void*){};
    void (*register_)(SteamCallback*,int){};
    void (*open_)(void*,const char*){},(*invite_)(void*,Steam::SteamId){};
    bool (*set_presence_)(void*,const char*,const char*){};
    const char* (*get_presence_)(void*,Steam::SteamId,const char*){};
    int (*friend_count_)(void*,int){},(*friend_state_)(void*,Steam::SteamId){};
    int (*friend_avatar_)(void*,Steam::SteamId){};
    Steam::SteamId (*friend_at_)(void*,int,int){};
    const char* (*friend_name_)(void*,Steam::SteamId){};
    bool (*friend_game_)(void*,Steam::SteamId,FriendGame*){},(*has_friend_)(void*,Steam::SteamId,int){};
    void* (*get_matchmaking_)(){};
    int (*lobby_count_)(void*,Steam::SteamId){},(*lobby_limit_)(void*,Steam::SteamId){};
    Steam::SteamId (*lobby_member_)(void*,Steam::SteamId,int){},(*lobby_owner_)(void*,Steam::SteamId){};
    bool (*send_invite_)(void*,Steam::SteamId,Steam::SteamId){};
    void* friends_{};
    Steam::SteamId owner_{},published_{};
    ULONGLONG next_init_{};
    FriendAccess access_{FriendAccess::unavailable};
    bool ready_{},overlay_{},friends_available_{},matchmaking_available_{};

    static bool steam_launch() {
        wchar_t value[16]{};
        return GetEnvironmentVariableW(L"SteamAppId",value,std::size(value))==7&&
            std::wstring_view(value)==L"1799840";
    }

    bool bind() {
        if(module_)return true;
        HMODULE module{};
        if(!GetModuleHandleExW(0,L"steam_api64.dll",&module))return false;
#define QD_SOCIAL(field,name) load_export(module,"SteamAPI_" name,field)
        const bool loaded=QD_SOCIAL(pipe_,"GetHSteamPipe")&&
            QD_SOCIAL(get_friends_,"SteamFriends_v018")&&QD_SOCIAL(get_utils_,"SteamUtils_v010")&&
            QD_SOCIAL(get_user_,"SteamUser_v023")&&QD_SOCIAL(app_,"ISteamUtils_GetAppID")&&
            QD_SOCIAL(self_,"ISteamUser_GetSteamID")&&QD_SOCIAL(logged_on_,"ISteamUser_BLoggedOn");
        QD_SOCIAL(initialize_,"InitFlat");
        QD_SOCIAL(overlay_enabled_,"ISteamUtils_IsOverlayEnabled");
        QD_SOCIAL(register_,"RegisterCallback");
        QD_SOCIAL(open_,"ISteamFriends_ActivateGameOverlay");
        QD_SOCIAL(invite_,"ISteamFriends_ActivateGameOverlayInviteDialog");
        QD_SOCIAL(set_presence_,"ISteamFriends_SetRichPresence");
        QD_SOCIAL(get_presence_,"ISteamFriends_GetFriendRichPresence");
        QD_SOCIAL(friend_avatar_,"ISteamFriends_GetSmallFriendAvatar");
        friends_available_=loaded&&QD_SOCIAL(friend_count_,"ISteamFriends_GetFriendCount")&&
            QD_SOCIAL(friend_at_,"ISteamFriends_GetFriendByIndex")&&QD_SOCIAL(friend_state_,"ISteamFriends_GetFriendPersonaState")&&
            QD_SOCIAL(friend_name_,"ISteamFriends_GetFriendPersonaName")&&QD_SOCIAL(friend_game_,"ISteamFriends_GetFriendGamePlayed")&&
            QD_SOCIAL(has_friend_,"ISteamFriends_HasFriend");
        matchmaking_available_=QD_SOCIAL(get_matchmaking_,"SteamMatchmaking_v009")&&
            QD_SOCIAL(lobby_count_,"ISteamMatchmaking_GetNumLobbyMembers")&&QD_SOCIAL(lobby_limit_,"ISteamMatchmaking_GetLobbyMemberLimit")&&
            QD_SOCIAL(lobby_member_,"ISteamMatchmaking_GetLobbyMemberByIndex")&&
            QD_SOCIAL(lobby_owner_,"ISteamMatchmaking_GetLobbyOwner")&&QD_SOCIAL(send_invite_,"ISteamMatchmaking_InviteUserToLobby");
#undef QD_SOCIAL
        if(!loaded){FreeLibrary(module);return false;}
        module_=module;
        return true;
    }

    void refresh() {
        ready_=overlay_=false;
        friends_=nullptr;
        access_=FriendAccess::unavailable;
        if(!bind()||!pipe_()){
            const bool launched=steam_launch();
            access_=launched?FriendAccess::sign_in:FriendAccess::launch_context;
            const auto now=GetTickCount64();
            if(!launched||!initialize_||now<next_init_)return;
            next_init_=now+5000;
            char error[1024]{};
            if(initialize_(error)!=0||!pipe_())return;
        }
        auto utils=get_utils_();
        auto user=get_user_();
        friends_=get_friends_();
        if(!utils||!user||!friends_||app_(utils)!=Steam::app_id){friends_=nullptr;return;}
        if(register_&&!lobby_invite.registered())register_(&lobby_invite,lobby_invite.id);
        if(register_&&!presence_invite.registered())register_(&presence_invite,presence_invite.id);
#ifdef QDCOOP_STEAM_SOCIAL_LAB
        bind_dispatch_probe(module_);
#endif
        const auto self=self_(user);
        if(!Steam::user_id(self)){friends_=nullptr;access_=FriendAccess::sign_in;return;}
        if(owner_!=self)published_=0;
        owner_=self;
        ready_=logged_on_(user);
        overlay_=overlay_enabled_&&overlay_enabled_(utils);
        access_=friends_available_?FriendAccess::ready:FriendAccess::unavailable;
    }

    static std::array<char,40> connect_text(Steam::SteamId lobby) {
        std::array<char,40> text{};
        std::memcpy(text.data(),Social::connect_prefix.data(),Social::connect_prefix.size());
        std::to_chars(text.data()+Social::connect_prefix.size(),text.data()+text.size()-1,lobby);
        return text;
    }

    std::string_view connect_value() const {
        const auto value=get_presence_(friends_,owner_,presence_key);
        return value?std::string_view(value):std::string_view{};
    }

    void update_presence(Steam::SteamId desired) {
        if(!ready_||!get_presence_||!set_presence_)return;
        if(published_&&published_!=desired) {
            const auto own=connect_text(published_);
            if(connect_value()==own.data()&&!set_presence_(friends_,presence_key,nullptr))return;
            published_=0;
        }
        if(!desired)return;
        const auto value=connect_value();
        const auto desired_text=connect_text(desired);
        if(value==desired_text.data())published_=desired;
        else if(value.empty()&&set_presence_(friends_,presence_key,desired_text.data()))published_=desired;
        else published_=0;
    }
    bool playing(Steam::SteamId id) const {
        FriendGame game;
        return friend_game_(friends_,id,&game)&&(game.game&0xffffffu)==Steam::app_id;
    }
    Steam::SteamId advertised_lobby(Steam::SteamId id) const {
        const auto value=get_presence_?get_presence_(friends_,id,presence_key):nullptr;
        const auto text=value?std::string_view(value):std::string_view{};
        return text.starts_with(Social::connect_prefix)?Social::parse_connect(text):0;
    }
    static std::wstring display_name(const char* name){
        if(!name||!*name)return {};
        std::array<wchar_t,128> wide{};
        const auto count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,name,-1,wide.data(),static_cast<int>(wide.size()));
        if(count<=1)return {};
        std::wstring text(wide.data(),static_cast<unsigned>(count-1));
        for(auto& ch:text)if(ch<L' '||ch==0x7f)ch=L' ';
        constexpr unsigned limit=23;
        if(text.size()>limit){
            const auto last=text[limit-1];
            text.resize(last>=0xd800&&last<=0xdbff?limit-1:limit);
            text+=L'\u2026';
        }
        return text;
    }
public:
    Steam::SteamId account() const {return friends_?owner_:0;}
    bool room_members(Steam::SteamId room,Steam::Roster& roster) const {
        if(!ready_||!matchmaking_available_||!Steam::lobby_id(room))return false;
        const auto matchmaking=get_matchmaking_();
        if(!matchmaking)return false;
        const int count=lobby_count_(matchmaking,room);
        if(count<1||count>static_cast<int>(qdcoop::max_players))return false;
        std::array<Steam::SteamId,qdcoop::max_players> members{};
        for(int i=0;i<count;++i)members[i]=lobby_member_(matchmaking,room,i);
        if(std::ranges::find(members,owner_)==members.end())return false;
        return Steam::make_roster(lobby_owner_(matchmaking,room),members,static_cast<unsigned>(count),roster);
    }
    std::wstring room_name(Steam::SteamId id) const {
        auto name=friends_&&friend_name_?display_name(friend_name_(friends_,id)):std::wstring{};
        return name.empty()||name==L"[unknown]"?std::to_wstring(id):name;
    }
    int avatar_handle(Steam::SteamId id) const {return friends_&&friend_avatar_?friend_avatar_(friends_,id):0;}
    FriendAccess list_friends(Steam::SteamId room,std::vector<Social::Friend>& result){
        refresh();
        if(access_!=FriendAccess::ready)return access_;
        const auto count=friend_count_(friends_,immediate);
        if(count<0)return FriendAccess::sign_in;
        std::array<Steam::SteamId,qdcoop::max_players> members{};
        if(room&&matchmaking_available_)if(const auto matchmaking=get_matchmaking_()){
            const auto member_count=std::clamp(lobby_count_(matchmaking,room),0,static_cast<int>(members.size()));
            for(int i=0;i<member_count;++i)members[i]=lobby_member_(matchmaking,room,i);
        }
        result.clear();
        for(int i=0;i<count&&i<2048;++i){
            const auto id=friend_at_(friends_,i,immediate);
            if(!Steam::user_id(id)||id==owner_)continue;
            const auto in_game=playing(id);
            const auto online=in_game||friend_state_(friends_,id)>0;
            result.push_back({id,in_game?advertised_lobby(id):0,display_name(friend_name_(friends_,id)),in_game,online,
                std::ranges::find(members,id)!=members.end()});
        }
        Social::sort_friends(result);
        return FriendAccess::ready;
    }
    Steam::SteamId friend_lobby(Steam::SteamId id){
        refresh();
        return ready_&&friends_available_&&has_friend_(friends_,id,immediate)&&playing(id)?advertised_lobby(id):0;
    }
    bool invite_friend(Steam::SteamId room,Steam::SteamId id){
        refresh();
        if(!ready_||!friends_available_||!matchmaking_available_||!Steam::lobby_id(room)||!Steam::user_id(id)||id==owner_||
           !has_friend_(friends_,id,immediate)||(!playing(id)&&friend_state_(friends_,id)<=0))return false;
        const auto matchmaking=get_matchmaking_();
        if(!matchmaking||lobby_owner_(matchmaking,room)!=owner_)return false;
        const auto count=lobby_count_(matchmaking,room);
        const auto limit=lobby_limit_(matchmaking,room);
        if(count<1||limit>static_cast<int>(qdcoop::max_players)||count>=limit)return false;
        for(int i=0;i<count;++i)if(lobby_member_(matchmaking,room,i)==id)return false;
        return send_invite_(matchmaking,room,id);
    }
    Social::Status service(Steam::SteamId desired,Overlay action,Steam::SteamId invite_lobby) {
        refresh();
        update_presence(desired);
        auto result=Social::Action::none;
        if(action!=Overlay::none) {
            result=Social::Action::unavailable;
            if(ready_&&overlay_) {
                if(action==Overlay::friends&&open_) {
                    open_(friends_,"Friends");
                    result=Social::Action::opened;
                } else if(invite_lobby&&invite_) {
                    invite_(friends_,invite_lobby);
                    result=Social::Action::opened;
                }
            }
        }
        return {ready_,unsigned(lobby_invite.registered())+unsigned(presence_invite.registered()),
                overlay_,result,published_};
    }
};
