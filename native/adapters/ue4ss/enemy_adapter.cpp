// Temporary M1 adapter. All Unreal access occurs on the registered EngineTick
// thread. The worker owns packet bytes only, and never touches Unreal objects.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include <tlhelp32.h>
#include "qdcoop/enemy_stream.hpp"
#include "qdcoop/enemy_discovery.hpp"
#include "qdcoop/enemy_enrollment.hpp"
#include "qdcoop/steam_room.hpp"
#include "qdcoop/lan_room.hpp"
#include "qdcoop/native_world.hpp"
#include "qdcoop/player_pose.hpp"
#include "qdcoop/player_gear_pose.hpp"
#include "qdcoop/player_motion.hpp"
#include "qdcoop/object_lookup.hpp"
#include "qdcoop/reflection_view.hpp"
#include "qdcoop/weak_object_view.hpp"
#include "qdcoop/animation_state.hpp"
#include "qdcoop/animation_catalog.hpp"
#include "qdcoop/death_state.hpp"
#include "qdcoop/reward_stream.hpp"
#include "qdcoop/reward_worker_windows.hpp"
#include <deque>
#include "qdcoop/rescue_store_windows.hpp"
#include "qdcoop/rescue_wire.hpp"
#include "qdcoop/checkpoint_store_windows.hpp"
#include "qdcoop/world_progress_map.hpp"
#include "qdcoop/world_progress_personal.hpp"
#include "qdcoop/world_interaction_intent.hpp"
#include "qdcoop/door_trigger_intent.hpp"
#include "qdcoop/enemy_rest_intent.hpp"
#include "qdcoop/story_reward_intent.hpp"
#include "qdcoop/native_save_clone.hpp"
#include "qdcoop/persistent_travel.hpp"
#include "qdcoop/elevator_intent.hpp"
#include "qdcoop/cutscene_wire.hpp"
#include "qdcoop/boss_phase.hpp"
#include "qdcoop/boss_interact.hpp"
#include "qdcoop/projectile_stream.hpp"
#include "qdcoop/placed_item_record.hpp"
#include "qdcoop/placed_item_claim.hpp"
#include "qdcoop/placed_item_deposit.hpp"
#include "qdcoop/body_material.hpp"
#include "qdcoop/visual_mount.hpp"
#include "qdcoop/boss_hud.hpp"
#include "qdcoop/enemy_visual_catalog.hpp"
#include "qdcoop/sound_stream.hpp"
#include "qdcoop/sound_loop.hpp"
#include "qdcoop/enemy_damage.hpp"
#include "qdcoop/threat_policy.hpp"
#include "qdcoop/transactions.hpp"
#include <memory>
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>
#include <span>
#include <optional>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace E=qdcoop::enemy;
struct View{const char* data;std::size_t size;};
struct alignas(16) Weak{unsigned char bytes[256]{};};
struct Api {
    void*(*construct)(void*,void*);void(*destruct)(void*);
    View*(*getstring)(void*,View*,int);void(*setstring)(void*,const char*,std::size_t);
    bool(*isstring)(void*,int),(*isnumber)(void*,int);std::int64_t(*getinteger)(void*,int);
    double(*getnumber)(void*,int);void(*setinteger)(void*,std::int64_t);void(*setnumber)(void*,double);int(*stacksize)(void*);
    void(*event)(void*,void*,void*);int*(*offset)(void*);int*(*size)(void*);int*(*propertiesSize)(void*);std::uint16_t*(*parms)(void*);
    bool(*isa)(void*,void*),(*getbool)(void*,const void*,int);void(*setbool)(void*,void*,bool,int);
    void*(*weakctor)(void*,const void*);void*(*weakget)(const void*);
}api{};
static bool bound;
template<class F>static bool proc(HMODULE h,F& fn,const char* name){auto p=GetProcAddress(h,name);static_assert(sizeof(p)==sizeof(fn));std::memcpy(&fn,&p,sizeof(p));return p!=nullptr;}
static bool bind(){
    if(bound)return true;
    auto h=GetModuleHandleW(L"UE4SS.dll");if(!h)return false;
#define API(v,n) if(!proc(h,api.v,n))return false
    API(construct,"??0Lua@LuaMadeSimple@RC@@QEAA@PEAUlua_State@@@Z");API(destruct,"??1Lua@LuaMadeSimple@RC@@QEAA@XZ");
    API(getstring,"?get_string@Lua@LuaMadeSimple@RC@@QEBA?AV?$basic_string_view@DU?$char_traits@D@std@@@std@@H@Z");
    API(setstring,"?set_string@Lua@LuaMadeSimple@RC@@QEBAXPEBD_K@Z");API(isstring,"?is_string@Lua@LuaMadeSimple@RC@@QEBA_NH@Z");
    API(isnumber,"?is_number@Lua@LuaMadeSimple@RC@@QEBA_NH@Z");API(getinteger,"?get_integer@Lua@LuaMadeSimple@RC@@QEBA_JH@Z");
    API(getnumber,"?get_number@Lua@LuaMadeSimple@RC@@QEBANH@Z");API(setinteger,"?set_integer@Lua@LuaMadeSimple@RC@@QEBAX_J@Z");
    API(setnumber,"?set_number@Lua@LuaMadeSimple@RC@@QEBAXN@Z");API(stacksize,"?get_stack_size@Lua@LuaMadeSimple@RC@@QEBAHXZ");
    API(event,"?ProcessEvent@UObject@Unreal@RC@@QEAAXPEAVUFunction@23@PEAX@Z");
    API(offset,"?GetOffset_Internal@FProperty@Unreal@RC@@QEAAAEAHXZ");API(size,"?GetElementSize@FProperty@Unreal@RC@@QEAAAEAHXZ");
    API(propertiesSize,"?GetPropertiesSize@UStruct@Unreal@RC@@QEAAAEAHXZ");API(parms,"?GetParmsSize@UFunction@Unreal@RC@@QEAAAEAGXZ");
    API(isa,"?IsA@UObjectBase@Unreal@RC@@QEBA_NPEAVUClass@23@@Z");
    API(getbool,"?GetPropertyValueInContainer@FBoolProperty@Unreal@RC@@QEAA_NPEBXH@Z");
    API(setbool,"?SetPropertyValueInContainer@FBoolProperty@Unreal@RC@@QEAA@PEAX_NH@Z");
    API(weakctor,"??0FWeakObjectPtr@Unreal@RC@@QEAA@PEBVUObject@12@@Z");API(weakget,"?Get@FWeakObjectPtr@Unreal@RC@@QEBAPEAVUObject@23@XZ");
#undef API
    bound=true;return true;
}
static double now(){LARGE_INTEGER q{},f{};QueryPerformanceCounter(&q);QueryPerformanceFrequency(&f);return double(q.QuadPart)/double(f.QuadPart);}
static std::atomic<DWORD> game_thread{};
static bool metadata_ready,callback_installed,native_bootstrap;
static unsigned mode;
static E::Envelope envelope{0,0,1,1};
struct Getter{void* fn{};void* ret{};int at{};};
static void* base_class;static void* mesh_class;static void* actor_class;static int hp_at,mesh_at;
static std::array<void*,4> bool_props{};
static std::array<Getter,6> getters{};
static void* actor_fn;static void* mesh_fn;static void* hide_fn;static void* visible_fn;
static std::array<Weak,13> metadata_weak{};
static unsigned metadata_count;
static std::uint64_t calls{},captured{},applied{},expired{},replaced{},rejected{},queue_drops{},stale{},no_sample{};
static std::atomic<std::uint64_t> wrong_thread{};
struct Entity {
    Weak visual{},visual_mesh{},owner_fn{};bool split{};
    Weak actor{},mesh{};bool used{};std::uint32_t generation{};std::uint64_t sequence{};
    E::State last{};E::Timeline timeline{};double received{},next_capture{};
    bool placed{};unsigned hidden{2},visible{2};
};
static std::array<Entity,E::capacity> entities{};
static E::Clock clock_state{};
static bool makeweak(Weak& w,void* object){
    if(!object)return false;
    std::memset(w.bytes,0xcc,sizeof(w.bytes));api.weakctor(&w,object);
    for(unsigned i=64;i<sizeof(w.bytes);++i)if(w.bytes[i]!=0xcc)return false;
    return api.weakget(&w)==object;
}
static bool property(void* p,int bytes,int owner){return p&&*api.size(p)==bytes&&*api.offset(p)>=0&&*api.offset(p)+bytes<=owner;}
static bool initialize(std::array<std::uint64_t,42> a){
    metadata_ready=false;metadata_count=0;
    auto ptr=[&](unsigned i){return reinterpret_cast<void*>(static_cast<std::uintptr_t>(a[i]));};
    base_class=ptr(0);mesh_class=ptr(1);if(!base_class||!mesh_class)return false;
    const int size=*api.propertiesSize(base_class);if(size<1||size>1048576)return false;
    if(!property(ptr(2),4,size)||!property(ptr(3),8,size))return false;
    hp_at=*api.offset(ptr(2));mesh_at=*api.offset(ptr(3));
    for(unsigned i=0;i<4;++i){if(!property(ptr(4+i),1,size))return false;bool_props[i]=ptr(4+i);}
    const int expected[]={12,12,12,1,48,1};
    for(unsigned i=0;i<6;++i){auto fn=ptr(8+i*2),ret=ptr(9+i*2);if(!fn||!ret)return false;
        const int bytes=*api.parms(fn);if(bytes>256||bytes<expected[i]||!property(ret,expected[i],bytes))return false;
        getters[i]={fn,ret,*api.offset(ret)};}
    const int offsets[]={0,12,24,28,164,165,0,48,52,188,0,16,32};
    const int sizes[]={12,12,1,136,1,1,48,1,136,1,16,12,12};
    const unsigned indices[]={25,26,27,28,29,30,33,34,35,36,37,38,39};
    for(unsigned i=0;i<13;++i)if(!ptr(indices[i])||*api.offset(ptr(indices[i]))!=offsets[i]||*api.size(ptr(indices[i]))!=sizes[i])return false;
    actor_fn=ptr(23);mesh_fn=ptr(31);hide_fn=ptr(40);visible_fn=ptr(41);
    if(!actor_fn||!mesh_fn||!hide_fn||!visible_fn||*api.parms(actor_fn)<166||*api.parms(actor_fn)>256||*api.parms(mesh_fn)<189||*api.parms(mesh_fn)>256||*api.parms(hide_fn)!=1||*api.parms(visible_fn)!=2)return false;
    actor_class=ptr(24);
    for(auto object:{base_class,mesh_class,actor_class,actor_fn,mesh_fn,hide_fn,visible_fn})if(!makeweak(metadata_weak[metadata_count++],object))return false;
    for(auto& g:getters)if(!makeweak(metadata_weak[metadata_count++],g.fn))return false;
    game_thread=GetCurrentThreadId();metadata_ready=true;return true;
}
static void* resolve(Entity& entity,void*& mesh){
    auto actor=api.weakget(&entity.actor);mesh=api.weakget(&entity.mesh);
    if(!actor){entity.used=false;++expired;return nullptr;}
    void* actual{};std::memcpy(&actual,static_cast<char*>(actor)+mesh_at,8);
    if(!mesh||actual!=mesh){entity.used=false;++replaced;return nullptr;}
    alignas(16) unsigned char args[256]{};api.event(actor,getters[3].fn,args);
    if(api.getbool(getters[3].ret,args,0)){entity.used=false;++expired;return nullptr;}
    return actor;
}
static bool capture(Entity& entity,unsigned id,double time,E::State& s){
    void* mesh{};auto actor=resolve(entity,mesh);if(!actor)return false;
    s={};s.id=id;s.generation=entity.generation;s.sequence=++entity.sequence;s.time=time;
    std::memcpy(&s.v[0],static_cast<char*>(actor)+hp_at,4);s.v[0]=std::max(0.f,s.v[0]);
    for(unsigned i=0;i<4;++i)s.v[i+1]=api.getbool(bool_props[i],actor,0)?1.f:0.f;
    alignas(16) unsigned char args[256]{};api.event(mesh,getters[5].fn,args);s.v[5]=api.getbool(getters[5].ret,args,0)?1.f:0.f;
    for(unsigned i=0;i<3;++i){std::memset(args,0,sizeof(args));api.event(actor,getters[i].fn,args);std::memcpy(&s.v[6+i*3],args+getters[i].at,12);}
    std::memset(args,0,sizeof(args));api.event(mesh,getters[4].fn,args);auto* t=args+getters[4].at;
    std::memcpy(&s.v[15],t+16,12);std::memcpy(&s.v[18],t,16);std::memcpy(&s.v[22],t+32,12);
    if(!E::finite(s)){++rejected;return false;}++captured;return true;
}
static void apply(Entity& entity,const E::State& state){
    void* mesh{};auto actor=resolve(entity,mesh);if(!actor)return;
    void* visual=actor;
    if(entity.split){
        visual=api.weakget(&entity.visual);mesh=api.weakget(&entity.visual_mesh);auto owner_fn=api.weakget(&entity.owner_fn);
        if(!visual||!mesh||!owner_fn){entity.used=false;++expired;return;}
        void* owner{};api.event(mesh,owner_fn,&owner);
        if(owner!=visual){entity.used=false;++replaced;return;}
        alignas(16) unsigned char alive[256]{};api.event(visual,getters[3].fn,alive);
        if(api.getbool(getters[3].ret,alive,0)){entity.used=false;++expired;return;}
    }
    const auto& v=state.v;auto& old=entity.last.v;bool move=!entity.placed,component=!entity.placed;
    for(unsigned i=6;i<12;++i)move=move||std::abs(v[i]-old[i])>.001f;
    for(unsigned i=15;i<25;++i)component=component||std::abs(v[i]-old[i])>.0001f;
    alignas(16) unsigned char args[256]{};
    if(move){std::memcpy(args,&v[6],24);args[164]=1;api.event(actor,actor_fn,args);if(entity.split){std::memset(args,0,sizeof(args));std::memcpy(args,&v[6],24);args[164]=1;api.event(visual,actor_fn,args);}}
    if(component){std::memset(args,0,sizeof(args));std::memcpy(args,&v[18],16);std::memcpy(args+16,&v[15],12);std::memcpy(args+32,&v[22],12);args[188]=1;api.event(mesh,mesh_fn,args);}
    if(!entity.placed||entity.last.sequence!=state.sequence){
        std::memcpy(static_cast<char*>(actor)+hp_at,&v[0],4);
        for(unsigned i=0;i<3;++i)api.setbool(bool_props[i],actor,v[i+1]!=0,0);
    }
    const unsigned hidden=v[4]!=0,visible=v[5]!=0;
    if(entity.hidden!=hidden){std::memset(args,0,sizeof(args));args[0]=static_cast<unsigned char>(hidden);api.event(visual,hide_fn,args);entity.hidden=hidden;}
    if(entity.visible!=visible){std::memset(args,0,sizeof(args));args[0]=static_cast<unsigned char>(visible);api.event(mesh,visible_fn,args);entity.visible=visible;}
    entity.last=state;entity.placed=true;++applied;
}

struct ReceivedPacket { E::Packet packet{}; unsigned peer{}; };
struct TargetedPacket { E::Packet packet{}; unsigned peers{7}; };
static E::Spsc<TargetedPacket,1024> outgoing; static E::Spsc<ReceivedPacket,1024> incoming;
static HANDLE worker{},stop_event{},wake_event{};
static std::atomic<int> build_state{}; // 0 hashing, 1 verified, -1 failed
// Cold installation validates the pinned engine array against live metadata.
// Construction still uses the engine/UE4SS serial-allocation path.
static int native_weak_state;
static std::uintptr_t native_object_array;
static void* native_weak_get(const void* value){
    if(GetCurrentThreadId()!=game_thread.load())return nullptr;
    qdcoop::ue427::WeakKey key{};std::memcpy(&key,value,sizeof(key));
    const auto read=[](std::uintptr_t p,void* out,std::size_t n){std::memcpy(out,reinterpret_cast<const void*>(p),n);return true;};
    return reinterpret_cast<void*>(qdcoop::ue427::resolve_weak(native_object_array,key,read));
}
static bool install_native_weak(){
    if(native_weak_state)return native_weak_state==1;
    if(build_state.load()!=1||GetCurrentThreadId()!=game_thread.load()||!metadata_ready||metadata_count!=13)return false;
    const auto address=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr))+0x4A64260;
    const auto read=[](std::uintptr_t p,void* out,std::size_t n){SIZE_T got{};return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(p),out,n,&got)&&got==n;};
    qdcoop::ue427::ObjectArray array{};
    if(!read(address,&array,sizeof(array))||!qdcoop::ue427::object_array_shape(array)){native_weak_state=-1;return false;}
    for(const auto& weak:metadata_weak){
        qdcoop::ue427::WeakKey key{};std::memcpy(&key,weak.bytes,sizeof(key));
        const auto expected=api.weakget(&weak);
        if(!expected||qdcoop::ue427::resolve_weak(address,key,read)!=reinterpret_cast<std::uintptr_t>(expected)){native_weak_state=-1;return false;}
    }
    native_object_array=address;api.weakget=&native_weak_get;native_weak_state=1;return true;
}

// ProcessEvent remains virtual: Actor and UObject have different implementations.
static int native_event_state;
static void native_process_event(void* object,void* function,void* parameters){
    if(GetCurrentThreadId()!=game_thread.load()){++wrong_thread;return;}
    std::uintptr_t table{};std::memcpy(&table,object,sizeof(table));
    decltype(api.event) invoke{};std::memcpy(&invoke,reinterpret_cast<const void*>(table+0x220),sizeof(invoke));
    invoke(object,function,parameters);
}
static bool install_native_event(){
    if(native_event_state)return native_event_state==1;
    if(build_state.load()!=1||GetCurrentThreadId()!=game_thread.load()||!metadata_ready||metadata_count!=13)return false;
    const auto expected=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr))+0x11A7AA0;
    for(const auto& weak:metadata_weak){
        auto object=api.weakget(&weak);std::uintptr_t table{},entry{};SIZE_T got{};
        if(!object||!ReadProcessMemory(GetCurrentProcess(),object,&table,sizeof(table),&got)||got!=sizeof(table)||
           !ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(table+0x220),&entry,sizeof(entry),&got)||got!=sizeof(entry)||entry!=expected){native_event_state=-1;return false;}
    }
    api.event=&native_process_event;native_event_state=1;return true;
}

static int native_reflection_state;
static bool install_native_reflection(){
    if(native_reflection_state)return native_reflection_state==1;
    if(build_state.load()!=1||GetCurrentThreadId()!=game_thread.load()||!metadata_ready)return false;
    namespace R=qdcoop::ue427;
    auto fail=[](){native_reflection_state=-1;return false;};
    for(auto type:{base_class,mesh_class,actor_class})if(api.propertiesSize(type)!=R::properties_size(type))return fail();
    for(const auto& g:getters)if(api.parms(g.fn)!=R::parameter_size(g.fn)||api.offset(g.ret)!=R::property_offset(g.ret)||api.size(g.ret)!=R::element_size(g.ret))return fail();
    // Compare writes only in private buffers: no live actor is changed here.
    for(auto p:bool_props){
        if(api.offset(p)!=R::property_offset(p)||api.size(p)!=R::element_size(p))return fail();
        const auto at=*api.offset(p);const auto size=*api.size(p);if(at<0||at>1048576||size!=1)return fail();
        const auto byte=R::field<unsigned char>(p,121);if(byte>=size)return fail();
        const auto count=static_cast<unsigned>(at+size+16);std::vector<unsigned char> old(count),next(count);
        for(unsigned value=0;value<256;++value)for(bool set:{false,true}){
            std::fill(old.begin(),old.end(),static_cast<unsigned char>(value));next=old;
            if(api.getbool(p,old.data(),0)!=R::get_bool(p,next.data(),0))return fail();
            api.setbool(p,old.data(),set,0);R::set_bool(p,next.data(),set,0);if(old!=next)return fail();
        }
    }
    void**(*legacy_class)(void*){};void**(*legacy_super)(void*){};auto module=GetModuleHandleW(L"UE4SS.dll");
    if(!proc(module,legacy_class,"?GetClassPrivate@UObjectBase@Unreal@RC@@QEAAAEAPEAVUClass@23@XZ")||!proc(module,legacy_super,"?GetSuperStruct@UStruct@Unreal@RC@@QEAAAEAPEAV123@XZ"))return fail();
    for(const auto& weak:metadata_weak){auto object=api.weakget(&weak);if(!object||reinterpret_cast<unsigned char*>(legacy_class(object))!=static_cast<unsigned char*>(object)+16)return fail();
        auto type=*legacy_class(object);unsigned depth=0;for(;type&&depth<256;++depth){if(reinterpret_cast<unsigned char*>(legacy_super(type))!=static_cast<unsigned char*>(type)+64||!R::is_a(object,type)||!api.isa(object,type))return fail();type=*legacy_super(type);}if(type)return fail();
        for(auto other:{base_class,mesh_class,actor_class})if(api.isa(object,other)!=R::is_a(object,other))return fail();
    }
    api.offset=&R::property_offset;api.size=&R::element_size;api.propertiesSize=&R::properties_size;api.parms=&R::parameter_size;api.getbool=&R::get_bool;api.setbool=&R::set_bool;api.isa=&R::is_a;native_reflection_state=1;return true;
}

// Use the original engine assignment and its atomic serial allocator. This is
// not a private serial counter and does not create temporary soft references.
static int native_construct_state;
static void(*engine_weak_assign)(void*,const void*);
static void* native_weak_construct(void* out,const void* object){
    if(GetCurrentThreadId()!=game_thread.load()){++wrong_thread;return nullptr;}
    engine_weak_assign(out,object);return out;
}
static bool install_native_construct(){
    if(native_construct_state)return native_construct_state==1;
    if(build_state.load()!=1||GetCurrentThreadId()!=game_thread.load()||!metadata_ready)return false;
    const auto address=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr))+0x11BB360;
    constexpr unsigned char expected[]{0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x85,0xd2,0x74,0x1a,0x8b,0x52};
    unsigned char bytes[sizeof(expected)]{};SIZE_T got{};
    auto fail=[](){native_construct_state=-1;return false;};
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),bytes,sizeof(bytes),&got)||got!=sizeof(bytes)||std::memcmp(bytes,expected,sizeof(bytes)))return fail();
    decltype(engine_weak_assign) assign{};std::memcpy(&assign,&address,sizeof(assign));
    for(const auto& weak:metadata_weak){auto object=api.weakget(&weak);if(!object)return fail();Weak candidate{};std::memset(candidate.bytes,0xcc,sizeof(candidate.bytes));assign(&candidate,object);if(std::memcmp(candidate.bytes,weak.bytes,8)||api.weakget(&candidate)!=object)return fail();for(unsigned i=8;i<sizeof(candidate.bytes);++i)if(candidate.bytes[i]!=0xcc)return fail();}
    Weak nil{};assign(&nil,nullptr);qdcoop::ue427::WeakKey key{};std::memcpy(&key,nil.bytes,8);if(key.index!=-1||key.serial)return fail();
    engine_weak_assign=assign;api.weakctor=&native_weak_construct;native_construct_state=1;return true;
}

static std::atomic<bool> stopped{true};
static std::atomic<std::uint64_t> tx{},rx{},worker_drops{},foreign{},socket_errors{},udp_unreachable{};
static bool host_role;static unsigned short local_port;
static std::array<unsigned short,3> peer_ports{};
static unsigned active_peer_mask;
static bool verified_game(){
    const unsigned char expected[32]={0x79,0xcc,0xdc,0x6c,0xf9,0xe1,0xe8,0x55,0x12,0xd5,0x2d,0x2c,0xb1,0xc3,0xff,0xa9,0x63,0xd4,0x00,0xbe,0x15,0x55,0x05,0x1a,0x35,0x23,0xc9,0x82,0xd1,0x81,0xd1,0xe6};
    wchar_t name[32768]{};if(!GetModuleFileNameW(nullptr,name,32768))return false;
    HANDLE file=CreateFileW(name,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    BCRYPT_ALG_HANDLE alg{};BCRYPT_HASH_HANDLE hash{};bool ok=BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)==0;
    if(ok)ok=BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0)==0;
    std::array<unsigned char,65536> buffer{};DWORD bytes{};
    while(ok){if(!ReadFile(file,buffer.data(),static_cast<DWORD>(buffer.size()),&bytes,nullptr)){ok=false;break;}if(!bytes)break;ok=BCryptHashData(hash,buffer.data(),bytes,0)==0;}
    unsigned char result[32]{};if(ok)ok=BCryptFinishHash(hash,result,32,0)==0&&std::memcmp(result,expected,32)==0;
    if(hash)BCryptDestroyHash(hash);
    if(alg)BCryptCloseAlgorithmProvider(alg,0);
    CloseHandle(file);return ok;
}
#include "steam_transport.inc"
#include "lan_transport.inc"
static bool native_transport_ready(){return transport_ready()&&lan_transport_ready();}
#ifdef QDCOOP_STEAM_RECOVERY_LAB
#include "../../tests/steam_recovery_fixture.inc"
static RecoveryFixture::Fixture recovery_fixture;
static DWORD WINAPI probe_steam_network(void*){
    SR::Status status{};
    status.lobby=RecoveryFixture::lobby;
    const auto failure=steam_session(recovery_fixture.api(),status,RecoveryFixture::host);
    status.failure=static_cast<unsigned>(failure);
    steam_phase(status,steam_stopping()?SR::Phase::closed:SR::Phase::failed);
    stopped=true;
    return 0;
}
#endif
static DWORD WINAPI network(void*){
    const auto high=envelope.room_high,low=envelope.room_low;
    if(!verified_game()){build_state=-1;stopped=true;return 0;}
    WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data)!=0){build_state=-1;stopped=true;return 0;}
    SOCKET socket=::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);WSAEVENT event=WSACreateEvent();
    sockaddr_in local{};local.sin_family=AF_INET;local.sin_addr.s_addr=htonl(INADDR_LOOPBACK);local.sin_port=htons(local_port);
    if(socket==INVALID_SOCKET||event==WSA_INVALID_EVENT||::bind(socket,reinterpret_cast<sockaddr*>(&local),sizeof(local))!=0||WSAEventSelect(socket,event,FD_READ)!=0){
        build_state=-1;++socket_errors;
    }else{
        build_state=1;HANDLE events[]={stop_event,wake_event,event};
        while(WaitForSingleObject(stop_event,0)!=WAIT_OBJECT_0){
            WaitForMultipleObjects(3,events,FALSE,10);WSANETWORKEVENTS occurred{};WSAEnumNetworkEvents(socket,event,&occurred);
            E::Packet packet{};
            TargetedPacket target{};for(unsigned budget=0;budget<256&&outgoing.pop(target);++budget)for(unsigned peer=0;peer<3;++peer)if(peer_ports[peer]&&(target.peers&(1u<<peer))){const auto port=peer_ports[peer];const auto& packet=target.packet;
                sockaddr_in to=local;to.sin_port=htons(port);const auto sent=sendto(socket,reinterpret_cast<const char*>(packet.data.data()),static_cast<int>(packet.size),0,reinterpret_cast<sockaddr*>(&to),sizeof(to));
                if(sent==static_cast<int>(packet.size))++tx;
                else if(sent==SOCKET_ERROR&&WSAGetLastError()==WSAECONNRESET)++udp_unreachable;
                else ++socket_errors;
            }
            for(unsigned budget=0;budget<256;++budget){sockaddr_in from{};int length=sizeof(from);
                const int bytes=recvfrom(socket,reinterpret_cast<char*>(packet.data.data()),E::packet_capacity,0,reinterpret_cast<sockaddr*>(&from),&length);
                if(bytes<0){
                    const auto error=WSAGetLastError();
                    // A peer may close first. An ICMP port-unreachable report
                    // on this datagram socket must not mask other peers' data.
                    if(error==WSAECONNRESET){++udp_unreachable;continue;}
                    if(error!=WSAEWOULDBLOCK)++socket_errors;
                    break;
                }
                const auto port=ntohs(from.sin_port);bool known=false;unsigned peer=0;for(unsigned j=0;j<3;++j)if(peer_ports[j]&&port==peer_ports[j]){known=true;peer=j;break;}
                if(from.sin_addr.s_addr!=htonl(INADDR_LOOPBACK)||!known){++foreign;continue;}
                packet.size=static_cast<unsigned>(bytes);
                if(!SR::match_datagram(packet,high,low)){++foreign;continue;}
                if(!incoming.push({packet,peer}))++worker_drops;else ++rx;
            }
        }
    }
    if(socket!=INVALID_SOCKET)closesocket(socket);
    if(event!=WSA_INVALID_EVENT)WSACloseEvent(event);
    WSACleanup();stopped=true;return 0;
}
static bool launch_transport(LPTHREAD_START_ROUTINE entry){
    stop_event=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    wake_event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(stop_event&&wake_event){
        stopped=false;
        worker=CreateThread(nullptr,0,entry,nullptr,0,nullptr);
        if(worker)return true;
    }
    if(stop_event)CloseHandle(stop_event);
    if(wake_event)CloseHandle(wake_event);
    stop_event=wake_event=nullptr;
    stopped=true;
    return false;
}

static bool start_transport(bool host,std::uint64_t port,std::uint64_t room_high,
                            std::uint64_t room_low,const std::array<std::uint64_t,3>& peers){
    host_role=host;
    local_port=static_cast<unsigned short>(port);
    envelope.room_high=room_high;
    envelope.room_low=room_low;
    active_peer_mask=0;
    for(unsigned i=0;i<peers.size();++i){
        peer_ports[i]=static_cast<unsigned short>(peers[i]);
        if(peers[i])active_peer_mask|=1u<<i;
    }
    return launch_transport(network);
}

static bool send(const E::Packet& packet,unsigned peers=7){if(!outgoing.push({packet,peers})){++queue_drops;return false;}SetEvent(wake_event);return true;}
#include "animation_engine.inc"
#include "native_animation_catalog.inc"
#include "native_enemy_discovery.inc"
static bool retirement_requested(unsigned index);
static bool native_boss_managed(unsigned index);
static void native_boss_capture(unsigned index,const E::State& scalar,unsigned peers);
static void native_boss_apply(unsigned index,const E::State& scalar,double time);
#include "death_engine.inc"
#include "death_observer.inc"
#include "ownership_engine.inc"
#include "enemy_enrollment.inc"
#include "reward_engine.inc"
static unsigned metric_count;static bool measuring;static double previous_frame,metric_start;
static std::array<double,60000> frame_ms{},work_ms{};
static std::uint64_t thread_start,process_start,network_start;
static std::uint64_t filetime(FILETIME t){return (std::uint64_t(t.dwHighDateTime)<<32)|t.dwLowDateTime;}
static std::uint64_t cpu(HANDLE h,bool process){FILETIME a{},b{},k{},u{};const auto ok=process?GetProcessTimes(h,&a,&b,&k,&u):GetThreadTimes(h,&a,&b,&k,&u);return ok?filetime(k)+filetime(u):0;}
struct ThreadMetric{HANDLE handle{};std::uint64_t start{};bool rhi{};};
static std::array<ThreadMetric,8> render_threads{};
static void collect_render_threads(){
    for(auto& item:render_threads){if(item.handle)CloseHandle(item.handle);item={};}
    HRESULT(WINAPI* description)(HANDLE,PWSTR*){};
    if(!proc(GetModuleHandleW(L"Kernel32.dll"),description,"GetThreadDescription"))return;
    const auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);if(snapshot==INVALID_HANDLE_VALUE)return;
    THREADENTRY32 entry{};entry.dwSize=sizeof(entry);unsigned count=0;
    if(Thread32First(snapshot,&entry))do{if(entry.th32OwnerProcessID!=GetCurrentProcessId())continue;
        const auto handle=OpenThread(THREAD_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ThreadID);if(!handle)continue;
        PWSTR name{};bool render=false,rhi=false;
        if(SUCCEEDED(description(handle,&name))&&name){rhi=std::wcsstr(name,L"RHIThread")!=nullptr;render=std::wcsstr(name,L"RenderThread")||std::wcsstr(name,L"RenderingThread");LocalFree(name);}
        if((render||rhi)&&count<render_threads.size())render_threads[count++]={handle,cpu(handle,false),rhi};else CloseHandle(handle);
    }while(Thread32Next(snapshot,&entry));CloseHandle(snapshot);
}
static std::uint64_t automatic_enrolled,automatic_profile_unready,factory_created,factory_failed,factory_unloaded;
static unsigned world_lookup_stage,world_configure_stage;
static std::uint64_t world_missing_inputs;
static bool native_epoch_open,native_stopping;
static std::uint64_t native_rooms_started,native_rooms_stopped,native_recoveries;
static std::array<std::uint64_t,2> previous_loopback{};
static void native_world_step(double time);
static bool receive_player(const ReceivedPacket&,double);
static bool receive_persistent_travel(const ReceivedPacket&,unsigned);
static bool receive_elevator_intent(const ReceivedPacket&,unsigned,double);
static bool receive_story_reward(const ReceivedPacket&,unsigned,double);
static bool receive_world_interaction(const ReceivedPacket&,unsigned,double);
static bool receive_door_trigger(const ReceivedPacket&,unsigned,double);
static bool native_door_triggers_cleanup();
static bool receive_enemy_rest(const ReceivedPacket&,unsigned,double);
static bool receive_world_scene(const ReceivedPacket&,unsigned,double);
static bool receive_native_boss(const ReceivedPacket&,unsigned);
static bool receive_boss_interact(const ReceivedPacket&,unsigned,double);
static bool receive_projectile(const ReceivedPacket&,unsigned,double);
static bool receive_sound(const ReceivedPacket&,unsigned,double);
static bool receive_placed_claim(const ReceivedPacket&,unsigned,double);
static bool persistent_travel_close();
static bool cleanup_players();
static bool cleanup_native_life();
static bool world_stream_cleanup();
static bool world_scene_cleanup();
static bool native_boss_interact_cleanup();
static bool native_projectile_cleanup();
static bool native_placed_items_cleanup();
static bool native_placed_item_save_pending();
static bool native_placed_item_custody_busy();
static bool receive_placed_deposit(const ReceivedPacket& received,unsigned kind,double time);
static bool receive_body_material(const ReceivedPacket& received,unsigned kind,double time);
static void native_body_material_cleanup();
static bool receive_visual_mount(const ReceivedPacket& received,unsigned kind,double time);
static bool receive_boss_hud(const ReceivedPacket& received,unsigned kind,double time);
static void native_healthbar_cleanup();
static bool native_visual_mount_cleanup();
static void native_placed_item_save_resume(void* save,double time);
static bool native_sound_cleanup();
static bool world_music_cleanup();
static bool world_library_cleanup();
static void world_elevator_cleanup();
static void reset_native_life();
static bool checkpoint_cleanup();
static void checkpoint_reset();
static void checkpoint_rollback_reset();
static bool checkpoint_can_wipe(double time);
static bool world_progress_ready();
static bool native_pause_cleanup();
static bool native_damage_cleanup();
static bool native_travel_guard_cleanup();
static bool native_stock_enemies_cleanup();
static void reset_native_gameplay_scope();
static std::string native_reward_stats();
static std::uint64_t checkpoint_world_nonce;
static void reset_players();
static std::uint64_t player_sent,player_received,player_relayed,player_rejected,player_created,player_destroyed;
static std::uint64_t player_graph_bound,player_body_frames,player_body_missing,player_action_starts,player_action_stops;
static std::uint64_t player_equipped,player_parts_created,player_parts_removed,player_skin_changed;
static unsigned player_equipment_stage;
static std::uint64_t gear_pose_sent,gear_pose_received,gear_pose_rejected,gear_pose_applied,gear_pose_writes;
static unsigned gear_pose_stage;
static unsigned player_asset_stage;
static unsigned life_stage;
static std::uint64_t life_intercepted,life_downed,life_revived,life_requests,life_grants,life_restored;
static std::uint64_t medic_debits,medic_revives,medic_refunds,medic_errors;
static std::uint64_t presentation_camera_updates,presentation_look_events,presentation_highlights,
                     presentation_power_updates,presentation_restored,presentation_camera_ticks;
static std::uint64_t checkpoint_archives,checkpoint_commits,checkpoint_ready_acks,checkpoint_stock_writes,checkpoint_errors;
static std::uint64_t world_progress_sent,world_progress_applied,world_progress_writes,world_progress_errors;
static std::uint64_t world_actor_restores,world_actor_chests,world_actor_sounds,world_actor_breakables;
static std::uint64_t world_interactions_queued,world_interactions_sent,world_interactions_received,world_interactions_applied;
static std::uint64_t door_triggers_observed,door_triggers_replayed,door_triggers_sent,door_triggers_received;
static unsigned door_trigger_stage,door_trigger_index_size,door_trigger_binding,door_trigger_actor_count;
static unsigned world_interaction_stage,world_breakable_index_size;
static std::uint64_t stock_enemies_suppressed;
static std::uint64_t enemy_rest_requests,enemy_rest_sent,enemy_rest_refreshes,enemy_rest_spawners;
static unsigned enemy_rest_stage;
static std::uint64_t world_mechanism_doors,world_mechanism_levers;
static std::uint64_t world_barrier_captured,world_barrier_applied;
static std::uint64_t world_arena_captured,world_arena_applied,world_arena_necro_material;
static std::uint64_t world_state_notifies,world_quest_notifies;
static std::uint64_t native_unpaused,native_menu_dilation,native_enemy_dilation,native_pause_restored;
static std::uint64_t native_modal_dilation,native_modal_restored;
static std::uint64_t damage_sent,damage_acked,damage_applied,damage_rejected,damage_duplicates,damage_suppressed,damage_proxies;
static unsigned hit_queries_stage;
static unsigned enemy_damage_replay_stage;
static std::uint64_t combat_proxies_armed,combat_proxies_registered,combat_proxies_unregistered;
static unsigned combat_proxy_stage;
static unsigned threat_stage;
static std::uint64_t threat_switches,threat_restores,threat_traces;
static std::uint64_t threat_host_hits;
static std::uint64_t threat_memory_created,threat_memory_used,threat_memory_retired;
static unsigned threat_memory_stage;
static std::uint64_t travel_guards_installed,travel_guards_restored;
static unsigned world_stream_stage;
static std::uint64_t world_stream_published,world_stream_restores,world_stream_verified;
static std::uint64_t world_arrival_published,world_arrival_applied,world_arrival_cancelled;
static std::uint64_t persistent_host_changes,persistent_guest_changes;
static unsigned persistent_stage;
static std::uint64_t world_music_changes,world_music_restored;
static std::uint64_t guest_safe_save_writes;
static std::uint64_t world_library_locks,world_library_restores,world_library_gate_locks;
static std::uint64_t world_elevator_moves,world_elevator_leases,world_elevator_releases;
static std::uint64_t elevator_motion_sent,elevator_motion_received;
static std::uint64_t elevator_intents,elevator_host_accepts,elevator_host_refusals,elevator_guest_accepts,elevator_guest_refusals;
static std::uint64_t cine_starts,cine_plays,cine_skips,cine_broadcasts,cine_commits,cine_callbacks,cine_errors;
static std::uint64_t boss_phase_sent,boss_phase_received,boss_phase_applied,boss_phase_interactable;
static std::uint64_t boss_interact_leases_taken,boss_interact_intercepted,boss_interact_requests;
static std::uint64_t boss_interact_host_accepts,boss_interact_host_refusals,boss_interact_guest_accepts,boss_interact_guest_refusals;
static std::uint64_t projectile_observed,projectile_sent,projectile_received,projectile_spawned;
static std::uint64_t projectile_moves,projectile_removed,projectile_retired,projectile_rejected;
static std::uint64_t projectile_relayed;
static std::uint64_t projectile_trail_sent,projectile_trail_received,projectile_trail_spawned,projectile_trail_removed;
static std::uint64_t sound_observed,sound_sent,sound_received,sound_played,sound_rejected,sound_stopped;
static std::uint64_t sound_direct_blueprint,sound_direct_reflected;
static std::uint64_t sound_control_observed,sound_control_sent,sound_control_received,sound_control_applied;
static std::uint64_t sound_replica_muted;
static std::uint64_t projectile_replica_suppressed;
static std::uint64_t projectile_fallback_births;
static std::uint64_t placed_items_spawned,placed_items_removed;
static std::uint64_t placed_pickup_enables,placed_pickup_requests,placed_pickup_credits,placed_pickup_confirmed;
static std::uint64_t placed_pickup_save_attempts;
static std::uint64_t placed_claim_sent,placed_claim_received,placed_claim_rejected,placed_claim_reserved,placed_claim_settled;
static std::uint64_t placed_source_debits,placed_source_confirmed;
static std::uint64_t placed_deposit_sent,placed_deposit_registered,placed_deposit_settled;
static unsigned placed_source_stage;
static std::uint64_t body_material_sent,body_material_applied;
static std::uint64_t visual_mount_sent,visual_mount_spawned,visual_mount_removed;
static std::uint64_t body_material_received,visual_mount_received;
static unsigned body_material_stage,visual_mount_stage;
static std::uint64_t enemy_hud_updates,boss_hud_sent,boss_hud_received,boss_hud_updates;
static unsigned placed_items_stage,placed_pickup_stage;
static std::uint64_t sound_loop_observed,sound_loop_sent,sound_loop_received,sound_loop_played;
static std::uint64_t sound_loop_moved,sound_loop_retired,sound_loop_stopped;
static std::uint64_t sound_loop_fade_observed,sound_loop_fade_received,sound_loop_fade_applied;
static std::uint64_t sound_loop_fade_in_observed,sound_loop_fade_in_received,sound_loop_fade_in_applied;
static std::uint64_t sound_loop_ambient_discovered;
static std::uint64_t story_duck_grants,story_duck_pickups_retired;
static void reset_native_world_epoch();
static bool receive_native_reward(const ReceivedPacket& received);
static void reset_native_epoch();
extern "C" __declspec(dllexport) int QDEnemyStopNative();
extern "C" __declspec(dllexport) int QDEnemyPollNativeRecovery();

extern "C" __declspec(dllexport) void QDEnemyEngineTick(float){
    if(GetCurrentThreadId()!=game_thread.load()){++wrong_thread;return;}
    if((steam_transport||lan_transport)&&mode){
        const auto build=build_state.load();
        if(build<0){QDEnemyStopNative();return;}
        if(!build){QDEnemyPollNativeRecovery();return;}
    }
    if(!metadata_ready||mode==0||!native_transport_ready())return;
    if(!install_native_weak()||!install_native_event()||!install_native_reflection()||!install_native_construct())return;
    const auto started=now();++calls;
    for(unsigned i=0;i<metadata_count;++i)if(!api.weakget(&metadata_weak[i])){metadata_ready=false;++expired;return;}
    if(mode==1){
        if(host_role){
            ReceivedPacket received{};
            for(unsigned n=0;n<256&&incoming.pop(received);++n){
                if(receive_player(received,started)||receive_enrollment(received)||receive_native_reward(received)||reward_receive(received))continue;
                D::Event event{};
                if(!D::decode(envelope,received.packet,true,event)){++death_rejected;continue;}
                if(retired_generation(event.id-1,event.generation)){++roster_stale;continue;}
                if(deaths[event.id-1].acknowledge(event,received.peer))++death_acks;
                else ++death_rejected;
            }
            native_world_step(started);
            pump_enrollment(started);
            for(auto& d:deaths)if(d.due(started)){
                const auto peers=enrollment_peer_mask(d.event.id-1)&d.waiting;
                E::Packet packet{};
                if(peers&&D::encode(envelope,d.event,false,packet)&&send(packet,peers))++death_retries;
            }
            for(unsigned i=0;i<E::capacity;++i){auto& e=entities[i];if(!e.used||deaths[i].terminal||retirement_requested(i)||started<e.next_capture)continue;
            const auto peers=enrollment_peer_mask(i);
            if(!peers)continue;
            e.next_capture=started+.05;E::State state{};E::Packet packet{};if(capture(e,i+1,started,state)&&E::encode(envelope,state,packet)){send(packet,peers);A::Frame animation{};void* mesh=api.weakget(&e.mesh);if(anim_capture(i,mesh,state,animation)&&A::encode(envelope,animation,packet))send(packet,peers);native_boss_capture(i,state,peers);}}}
        else{
            ReceivedPacket received{};for(unsigned i=0;i<256&&incoming.pop(received);++i){if(receive_player(received,started)||receive_enrollment(received)||receive_native_reward(received)||reward_receive(received))continue;auto& packet=received.packet;E::State state{};
                if(packet.size>=40&&A::wire::get<std::uint32_t>(packet,32)==4){
                    D::Event event{};
                    if(!D::decode(envelope,packet,false,event)){++death_rejected;continue;}
                    if(stale_replica_generation(event.id-1,event.generation)){++roster_stale;continue;}
                    if(death_receive(event)){E::Packet ack{};if(D::encode(envelope,event,true,ack))send(ack);}
                    else ++death_rejected;
                    continue;
                }
                if(packet.size>=40&&A::wire::get<std::uint32_t>(packet,32)==3){
                    A::Frame frame{};
                    if(!A::decode(envelope,packet,frame)){++anim_rejected;continue;}
                    if(stale_replica_generation(frame.id-1,frame.generation)){++roster_stale;continue;}
                    if(deaths[frame.id-1].terminal||!anim_push(frame))++anim_rejected;
                    continue;
                }
                if(!E::decode(envelope,packet,state)){++rejected;continue;}auto& e=entities[state.id-1];
                if(stale_replica_generation(state.id-1,state.generation)){++roster_stale;continue;}
                if(!e.used||deaths[state.id-1].terminal||e.generation!=state.generation){++stale;continue;}
                if(e.timeline.push(state,started)){clock_state.push(state.time,started);e.received=started;}else ++stale;
            }
            pump_client_retirements(started);
            native_world_step(started);
            for(auto& e:entities)if(e.used&&!deaths[static_cast<unsigned>(&e-entities.data())].terminal&&
                                     !retirement_requested(static_cast<unsigned>(&e-entities.data()))){E::Sample s{};if(e.received==0||started-e.received>.5){
                    if(e.placed&&e.hidden!=1){auto hidden=e.last;hidden.v[4]=1;hidden.v[5]=0;apply(e,hidden);}
                    ++no_sample;continue;}
                if(e.timeline.sample(clock_state,started,s)){
                    const auto index=static_cast<unsigned>(&e-entities.data());
                    apply(e,s.state);
                    native_boss_apply(index,s.state,started);
                    anim_apply(index,s.time);
                }}
        }
    }
    if(mode==1)reward_pump(started);
    if(measuring&&metric_count<frame_ms.size()&&previous_frame!=0){frame_ms[metric_count]=(started-previous_frame)*1000;work_ms[metric_count]=(now()-started)*1000;++metric_count;}
    previous_frame=started;
}
static std::string stats(){
    unsigned active=0;for(auto& e:entities)active+=e.used;
    char buffer[1600];std::snprintf(buffer,sizeof(buffer),"{\"build\":%d,\"mode\":%u,\"active\":%u,\"thread\":%lu,\"calls\":%llu,\"captured\":%llu,\"applied\":%llu,\"expired\":%llu,\"replaced\":%llu,\"wrong_thread\":%llu,\"rejected\":%llu,\"stale\":%llu,\"queue_drops\":%llu,\"tx\":%llu,\"rx\":%llu,\"worker_drops\":%llu,\"foreign\":%llu,\"socket_errors\":%llu,\"no_sample\":%llu}",build_state.load(),mode,active,static_cast<unsigned long>(game_thread.load()),calls,captured,applied,expired,replaced,wrong_thread.load(),rejected,stale,queue_drops,tx.load(),rx.load(),worker_drops.load(),foreign.load(),socket_errors.load(),no_sample);std::string result=buffer;result.pop_back();result+=",\"native_weak\":"+std::to_string(native_weak_state)+",\"native_event\":"+std::to_string(native_event_state)+",\"native_reflection\":"+std::to_string(native_reflection_state)+",\"native_construct\":"+std::to_string(native_construct_state);unsigned waiting=0;for(const auto& d:deaths)waiting+=d.waiting!=0;
    char extra[640];
    std::snprintf(extra,sizeof(extra),",\"kill_hook\":%u,\"kill_observed\":%llu,\"kill_queued\":%llu,\"kill_ignored\":%llu,\"kill_forwarded\":%llu,\"kill_restored\":%u,\"kill_restore_conflicts\":%u}",
                  unsigned(kill_observer_active),kill_observed,kill_queued,kill_ignored,kill_forwarded,kill_restored,kill_restore_conflicts);
    result+=extra;result.pop_back();
    result+=",\"anim_catalog_slots\":"+std::to_string(catalog_slots)+
        ",\"anim_catalog_passes\":"+std::to_string(catalog_passes)+
        ",\"anim_catalog_montages\":"+std::to_string(catalog_asset_ids[0].size())+
        ",\"anim_catalog_sequences\":"+std::to_string(catalog_asset_ids[1].size())+
        ",\"anim_catalog_binds\":"+std::to_string(catalog_binds);
    result+=",\"enum_calls\":"+std::to_string(enum_calls)+",\"enum_seen\":"+std::to_string(enum_seen)+
        ",\"enum_in_range\":"+std::to_string(enum_in_range)+",\"enum_peer_range\":"+std::to_string(enum_peer_range)+
        ",\"enum_frees\":"+std::to_string(enum_frees)+
        ",\"enroll_sent\":"+std::to_string(enrollment_sent)+",\"enroll_received\":"+std::to_string(enrollment_received)+
        ",\"enroll_acked\":"+std::to_string(enrollment_acked)+",\"enroll_confirmed\":"+std::to_string(enrollment_confirmed)+
        ",\"enroll_rejected\":"+std::to_string(enrollment_rejected);
    result+=",\"retire_sent\":"+std::to_string(retire_sent)+",\"retire_acked\":"+std::to_string(retire_acked)+
        ",\"retire_applied\":"+std::to_string(retire_applied)+",\"retire_duplicates\":"+std::to_string(retire_duplicates)+
        ",\"retire_rejected\":"+std::to_string(retire_rejected)+",\"retire_deferred\":"+std::to_string(retire_deferred)+
        ",\"retire_automatic\":"+std::to_string(retire_automatic)+",\"retire_destroyed\":"+std::to_string(retire_destroyed)+
        ",\"roster_stale\":"+std::to_string(roster_stale);
    result+=",\"auto_enrolled\":"+std::to_string(automatic_enrolled)+",\"factory_created\":"+std::to_string(factory_created)+
        ",\"factory_failed\":"+std::to_string(factory_failed)+",\"factory_unloaded\":"+std::to_string(factory_unloaded)+
        ",\"auto_profile_unready\":"+std::to_string(automatic_profile_unready)+
        ",\"world_lookup_stage\":"+std::to_string(world_lookup_stage)+
        ",\"world_configure_stage\":"+std::to_string(world_configure_stage)+
        ",\"world_missing_inputs\":"+std::to_string(world_missing_inputs)+
        ",\"world_epoch\":\""+std::to_string(envelope.world)+"\""+
        native_reward_stats()+
        ",\"rooms_started\":"+std::to_string(native_rooms_started)+
        ",\"rooms_stopped\":"+std::to_string(native_rooms_stopped)+
        ",\"recoveries\":"+std::to_string(native_recoveries)+
        ",\"udp_unreachable\":"+std::to_string(udp_unreachable.load())+
        ",\"player_sent\":"+std::to_string(player_sent)+",\"player_received\":"+std::to_string(player_received)+
        ",\"player_relayed\":"+std::to_string(player_relayed)+",\"player_rejected\":"+std::to_string(player_rejected)+
        ",\"player_created\":"+std::to_string(player_created)+",\"player_destroyed\":"+std::to_string(player_destroyed)+
        ",\"player_asset_stage\":"+std::to_string(player_asset_stage)+
        ",\"player_graph_bound\":"+std::to_string(player_graph_bound)+",\"player_body_frames\":"+std::to_string(player_body_frames)+
        ",\"player_body_missing\":"+std::to_string(player_body_missing)+",\"player_action_starts\":"+std::to_string(player_action_starts)+
        ",\"player_action_stops\":"+std::to_string(player_action_stops)+
        ",\"life_stage\":"+std::to_string(life_stage)+
        ",\"life_intercepted\":"+std::to_string(life_intercepted)+
        ",\"life_downed\":"+std::to_string(life_downed)+",\"life_revived\":"+std::to_string(life_revived)+
        ",\"life_requests\":"+std::to_string(life_requests)+",\"life_grants\":"+std::to_string(life_grants)+
        ",\"life_restored\":"+std::to_string(life_restored)+
        ",\"medic_debits\":"+std::to_string(medic_debits)+",\"medic_revives\":"+std::to_string(medic_revives)+
        ",\"medic_refunds\":"+std::to_string(medic_refunds)+",\"medic_errors\":"+std::to_string(medic_errors)+
        ",\"view_camera\":"+std::to_string(presentation_camera_updates)+",\"view_look\":"+std::to_string(presentation_look_events)+
        ",\"view_highlight\":"+std::to_string(presentation_highlights)+",\"view_power\":"+std::to_string(presentation_power_updates)+
        ",\"view_restored\":"+std::to_string(presentation_restored)+
        ",\"view_camera_ticks\":"+std::to_string(presentation_camera_ticks)+
        ",\"cp_archives\":"+std::to_string(checkpoint_archives)+",\"cp_commits\":"+std::to_string(checkpoint_commits)+
        ",\"cp_acks\":"+std::to_string(checkpoint_ready_acks)+",\"cp_stock\":"+std::to_string(checkpoint_stock_writes)+
        ",\"cp_errors\":"+std::to_string(checkpoint_errors)+
        ",\"world_progress_sent\":"+std::to_string(world_progress_sent)+
        ",\"world_progress_applied\":"+std::to_string(world_progress_applied)+
        ",\"world_progress_writes\":"+std::to_string(world_progress_writes)+
        ",\"world_progress_errors\":"+std::to_string(world_progress_errors)+
        ",\"world_actor_restores\":"+std::to_string(world_actor_restores)+
        ",\"world_actor_chests\":"+std::to_string(world_actor_chests)+
        ",\"world_actor_breakables\":"+std::to_string(world_actor_breakables)+
        ",\"world_interactions_queued\":"+std::to_string(world_interactions_queued)+
        ",\"world_interactions_sent\":"+std::to_string(world_interactions_sent)+
        ",\"world_interactions_received\":"+std::to_string(world_interactions_received)+
        ",\"world_interactions_applied\":"+std::to_string(world_interactions_applied)+
        ",\"door_triggers_observed\":"+std::to_string(door_triggers_observed)+
        ",\"door_triggers_replayed\":"+std::to_string(door_triggers_replayed)+
        ",\"door_triggers_sent\":"+std::to_string(door_triggers_sent)+
        ",\"door_triggers_received\":"+std::to_string(door_triggers_received)+
        ",\"door_trigger_stage\":"+std::to_string(door_trigger_stage)+
        ",\"door_trigger_index_size\":"+std::to_string(door_trigger_index_size)+
        ",\"door_trigger_binding\":"+std::to_string(door_trigger_binding)+
        ",\"door_trigger_actor_count\":"+std::to_string(door_trigger_actor_count)+
        ",\"world_interaction_stage\":"+std::to_string(world_interaction_stage)+
        ",\"world_breakable_index\":"+std::to_string(world_breakable_index_size)+
        ",\"stock_enemies_suppressed\":"+std::to_string(stock_enemies_suppressed)+
        ",\"enemy_rest_requests\":"+std::to_string(enemy_rest_requests)+
        ",\"enemy_rest_sent\":"+std::to_string(enemy_rest_sent)+
        ",\"enemy_rest_stage\":"+std::to_string(enemy_rest_stage)+
        ",\"enemy_rest_refreshes\":"+std::to_string(enemy_rest_refreshes)+
        ",\"enemy_rest_spawners\":"+std::to_string(enemy_rest_spawners)+
        ",\"world_actor_sounds\":"+std::to_string(world_actor_sounds)+
        ",\"world_mechanism_doors\":"+std::to_string(world_mechanism_doors)+
        ",\"world_mechanism_levers\":"+std::to_string(world_mechanism_levers)+
        ",\"world_barrier_captured\":"+std::to_string(world_barrier_captured)+
        ",\"world_barrier_applied\":"+std::to_string(world_barrier_applied)+
        ",\"world_arena_captured\":"+std::to_string(world_arena_captured)+
        ",\"world_arena_applied\":"+std::to_string(world_arena_applied)+
        ",\"world_arena_necro_material\":"+std::to_string(world_arena_necro_material)+
        ",\"world_state_notifies\":"+std::to_string(world_state_notifies)+
        ",\"world_quest_notifies\":"+std::to_string(world_quest_notifies)+
        ",\"pause_unpaused\":"+std::to_string(native_unpaused)+
        ",\"pause_menu_dilation\":"+std::to_string(native_menu_dilation)+
        ",\"pause_enemy_dilation\":"+std::to_string(native_enemy_dilation)+
        ",\"pause_restored\":"+std::to_string(native_pause_restored)+
        ",\"pause_modal_dilation\":"+std::to_string(native_modal_dilation)+
        ",\"pause_modal_restored\":"+std::to_string(native_modal_restored)+
        ",\"damage_sent\":"+std::to_string(damage_sent)+
        ",\"damage_acked\":"+std::to_string(damage_acked)+
        ",\"damage_applied\":"+std::to_string(damage_applied)+
        ",\"damage_rejected\":"+std::to_string(damage_rejected)+
        ",\"damage_duplicates\":"+std::to_string(damage_duplicates)+
        ",\"damage_suppressed\":"+std::to_string(damage_suppressed)+
        ",\"damage_proxies\":"+std::to_string(damage_proxies)+
        ",\"hit_queries_stage\":"+std::to_string(hit_queries_stage)+
        ",\"enemy_damage_replay_stage\":"+std::to_string(enemy_damage_replay_stage)+
        ",\"combat_proxies_registered\":"+std::to_string(combat_proxies_registered)+
        ",\"combat_proxies_armed\":"+std::to_string(combat_proxies_armed)+
        ",\"combat_proxies_unregistered\":"+std::to_string(combat_proxies_unregistered)+
        ",\"combat_proxy_stage\":"+std::to_string(combat_proxy_stage)+
        ",\"threat_stage\":"+std::to_string(threat_stage)+
        ",\"threat_switches\":"+std::to_string(threat_switches)+
        ",\"threat_restores\":"+std::to_string(threat_restores)+
        ",\"threat_traces\":"+std::to_string(threat_traces)+
        ",\"threat_host_hits\":"+std::to_string(threat_host_hits)+
        ",\"threat_memory_created\":"+std::to_string(threat_memory_created)+
        ",\"threat_memory_used\":"+std::to_string(threat_memory_used)+
        ",\"threat_memory_retired\":"+std::to_string(threat_memory_retired)+
        ",\"threat_memory_stage\":"+std::to_string(threat_memory_stage)+
        ",\"travel_guards_installed\":"+std::to_string(travel_guards_installed)+
        ",\"travel_guards_restored\":"+std::to_string(travel_guards_restored)+
        ",\"world_stream_stage\":"+std::to_string(world_stream_stage)+
        ",\"world_stream_published\":"+std::to_string(world_stream_published)+
        ",\"world_stream_restores\":"+std::to_string(world_stream_restores)+
        ",\"world_stream_verified\":"+std::to_string(world_stream_verified)+
        ",\"world_arrival_published\":"+std::to_string(world_arrival_published)+
        ",\"world_arrival_applied\":"+std::to_string(world_arrival_applied)+
        ",\"world_arrival_cancelled\":"+std::to_string(world_arrival_cancelled)+
        ",\"persistent_host_changes\":"+std::to_string(persistent_host_changes)+
        ",\"persistent_guest_changes\":"+std::to_string(persistent_guest_changes)+
        ",\"persistent_stage\":"+std::to_string(persistent_stage)+
        ",\"world_music_changes\":"+std::to_string(world_music_changes)+
        ",\"world_music_restored\":"+std::to_string(world_music_restored)+
        ",\"guest_safe_save_writes\":"+std::to_string(guest_safe_save_writes)+
        ",\"world_library_locks\":"+std::to_string(world_library_locks)+
        ",\"world_library_restores\":"+std::to_string(world_library_restores)+
        ",\"world_library_gate_locks\":"+std::to_string(world_library_gate_locks)+
        ",\"world_elevator_moves\":"+std::to_string(world_elevator_moves)+
        ",\"elevator_motion_sent\":"+std::to_string(elevator_motion_sent)+
        ",\"elevator_motion_received\":"+std::to_string(elevator_motion_received)+
        ",\"world_elevator_leases\":"+std::to_string(world_elevator_leases)+
        ",\"world_elevator_releases\":"+std::to_string(world_elevator_releases)+
        ",\"elevator_intents\":"+std::to_string(elevator_intents)+
        ",\"elevator_host_accepts\":"+std::to_string(elevator_host_accepts)+
        ",\"elevator_host_refusals\":"+std::to_string(elevator_host_refusals)+
        ",\"elevator_guest_accepts\":"+std::to_string(elevator_guest_accepts)+
        ",\"elevator_guest_refusals\":"+std::to_string(elevator_guest_refusals)+
        ",\"cine_starts\":"+std::to_string(cine_starts)+",\"cine_plays\":"+std::to_string(cine_plays)+
        ",\"cine_skips\":"+std::to_string(cine_skips)+",\"cine_broadcasts\":"+std::to_string(cine_broadcasts)+
        ",\"cine_commits\":"+std::to_string(cine_commits)+",\"cine_callbacks\":"+std::to_string(cine_callbacks)+
        ",\"cine_errors\":"+std::to_string(cine_errors)+
        ",\"boss_phase_sent\":"+std::to_string(boss_phase_sent)+",\"boss_phase_received\":"+std::to_string(boss_phase_received)+
        ",\"boss_phase_applied\":"+std::to_string(boss_phase_applied)+
        ",\"boss_phase_interactable\":"+std::to_string(boss_phase_interactable)+
        ",\"boss_interact_leases\":"+std::to_string(boss_interact_leases_taken)+
        ",\"boss_interact_intercepted\":"+std::to_string(boss_interact_intercepted)+
        ",\"boss_interact_requests\":"+std::to_string(boss_interact_requests)+
        ",\"boss_interact_host_accepts\":"+std::to_string(boss_interact_host_accepts)+
        ",\"boss_interact_host_refusals\":"+std::to_string(boss_interact_host_refusals)+
        ",\"boss_interact_guest_accepts\":"+std::to_string(boss_interact_guest_accepts)+
        ",\"boss_interact_guest_refusals\":"+std::to_string(boss_interact_guest_refusals)+
        ",\"projectile_observed\":"+std::to_string(projectile_observed)+
        ",\"projectile_sent\":"+std::to_string(projectile_sent)+
        ",\"projectile_received\":"+std::to_string(projectile_received)+
        ",\"projectile_spawned\":"+std::to_string(projectile_spawned)+
        ",\"projectile_moves\":"+std::to_string(projectile_moves)+
        ",\"projectile_removed\":"+std::to_string(projectile_removed)+
        ",\"projectile_retired\":"+std::to_string(projectile_retired)+
        ",\"projectile_rejected\":"+std::to_string(projectile_rejected)+
        ",\"projectile_relayed\":"+std::to_string(projectile_relayed)+
        ",\"projectile_trail_sent\":"+std::to_string(projectile_trail_sent)+
        ",\"projectile_trail_received\":"+std::to_string(projectile_trail_received)+
        ",\"projectile_trail_spawned\":"+std::to_string(projectile_trail_spawned)+
        ",\"projectile_trail_removed\":"+std::to_string(projectile_trail_removed)+
        ",\"sound_observed\":"+std::to_string(sound_observed)+
        ",\"sound_sent\":"+std::to_string(sound_sent)+
        ",\"sound_received\":"+std::to_string(sound_received)+
        ",\"sound_played\":"+std::to_string(sound_played)+
        ",\"sound_rejected\":"+std::to_string(sound_rejected)+
        ",\"sound_stopped\":"+std::to_string(sound_stopped)+
        ",\"sound_control_observed\":"+std::to_string(sound_control_observed)+
        ",\"sound_control_sent\":"+std::to_string(sound_control_sent)+
        ",\"sound_control_received\":"+std::to_string(sound_control_received)+
        ",\"sound_control_applied\":"+std::to_string(sound_control_applied)+
        ",\"sound_replica_muted\":"+std::to_string(sound_replica_muted)+
        ",\"projectile_replica_suppressed\":"+std::to_string(projectile_replica_suppressed)+
        ",\"projectile_fallback_births\":"+std::to_string(projectile_fallback_births)+
        ",\"placed_items_stage\":"+std::to_string(placed_items_stage)+
        ",\"placed_items_spawned\":"+std::to_string(placed_items_spawned)+
        ",\"placed_items_removed\":"+std::to_string(placed_items_removed)+
        ",\"placed_pickup_enables\":"+std::to_string(placed_pickup_enables)+
        ",\"placed_pickup_stage\":"+std::to_string(placed_pickup_stage)+
        ",\"placed_pickup_requests\":"+std::to_string(placed_pickup_requests)+
        ",\"placed_pickup_credits\":"+std::to_string(placed_pickup_credits)+
        ",\"placed_pickup_confirmed\":"+std::to_string(placed_pickup_confirmed)+
        ",\"placed_pickup_save_attempts\":"+std::to_string(placed_pickup_save_attempts)+
        ",\"placed_claim_sent\":"+std::to_string(placed_claim_sent)+
        ",\"placed_claim_received\":"+std::to_string(placed_claim_received)+
        ",\"placed_claim_rejected\":"+std::to_string(placed_claim_rejected)+
        ",\"placed_claim_reserved\":"+std::to_string(placed_claim_reserved)+
        ",\"placed_claim_settled\":"+std::to_string(placed_claim_settled)+
        ",\"placed_source_debits\":"+std::to_string(placed_source_debits)+
        ",\"placed_source_stage\":"+std::to_string(placed_source_stage)+
        ",\"body_material_sent\":"+std::to_string(body_material_sent)+
        ",\"body_material_applied\":"+std::to_string(body_material_applied)+
        ",\"body_material_received\":"+std::to_string(body_material_received)+
        ",\"body_material_stage\":"+std::to_string(body_material_stage)+
        ",\"visual_mount_sent\":"+std::to_string(visual_mount_sent)+
        ",\"visual_mount_spawned\":"+std::to_string(visual_mount_spawned)+
        ",\"visual_mount_removed\":"+std::to_string(visual_mount_removed)+
        ",\"visual_mount_received\":"+std::to_string(visual_mount_received)+
        ",\"visual_mount_stage\":"+std::to_string(visual_mount_stage)+
        ",\"enemy_hud_updates\":"+std::to_string(enemy_hud_updates)+
        ",\"boss_hud_sent\":"+std::to_string(boss_hud_sent)+
        ",\"boss_hud_received\":"+std::to_string(boss_hud_received)+
        ",\"boss_hud_updates\":"+std::to_string(boss_hud_updates)+
        ",\"placed_deposit_sent\":"+std::to_string(placed_deposit_sent)+
        ",\"placed_deposit_registered\":"+std::to_string(placed_deposit_registered)+
        ",\"placed_deposit_settled\":"+std::to_string(placed_deposit_settled)+
        ",\"placed_source_confirmed\":"+std::to_string(placed_source_confirmed)+
        ",\"sound_direct_blueprint\":"+std::to_string(sound_direct_blueprint)+
        ",\"sound_direct_reflected\":"+std::to_string(sound_direct_reflected)+
        ",\"sound_loop_observed\":"+std::to_string(sound_loop_observed)+
        ",\"sound_loop_sent\":"+std::to_string(sound_loop_sent)+
        ",\"sound_loop_received\":"+std::to_string(sound_loop_received)+
        ",\"sound_loop_played\":"+std::to_string(sound_loop_played)+
        ",\"sound_loop_moved\":"+std::to_string(sound_loop_moved)+
        ",\"sound_loop_retired\":"+std::to_string(sound_loop_retired)+
        ",\"sound_loop_stopped\":"+std::to_string(sound_loop_stopped)+
        ",\"sound_loop_fade_observed\":"+std::to_string(sound_loop_fade_observed)+
        ",\"sound_loop_fade_received\":"+std::to_string(sound_loop_fade_received)+
        ",\"sound_loop_fade_applied\":"+std::to_string(sound_loop_fade_applied)+
        ",\"sound_loop_fade_in_observed\":"+std::to_string(sound_loop_fade_in_observed)+
        ",\"sound_loop_fade_in_received\":"+std::to_string(sound_loop_fade_in_received)+
        ",\"sound_loop_fade_in_applied\":"+std::to_string(sound_loop_fade_in_applied)+
        ",\"sound_loop_ambient_discovered\":"+std::to_string(sound_loop_ambient_discovered)+
        ",\"story_duck_grants\":"+std::to_string(story_duck_grants)+
        ",\"story_duck_pickups_retired\":"+std::to_string(story_duck_pickups_retired)+
        ",\"player_equipped\":"+std::to_string(player_equipped)+",\"player_parts_created\":"+std::to_string(player_parts_created)+
        ",\"player_parts_removed\":"+std::to_string(player_parts_removed)+",\"player_skin_changed\":"+std::to_string(player_skin_changed)+
        ",\"player_equipment_stage\":"+std::to_string(player_equipment_stage)+
        ",\"gear_pose_sent\":"+std::to_string(gear_pose_sent)+",\"gear_pose_received\":"+std::to_string(gear_pose_received)+
        ",\"gear_pose_rejected\":"+std::to_string(gear_pose_rejected)+",\"gear_pose_applied\":"+std::to_string(gear_pose_applied)+
        ",\"gear_pose_writes\":"+std::to_string(gear_pose_writes)+",\"gear_pose_stage\":"+std::to_string(gear_pose_stage);
    std::snprintf(extra,sizeof(extra),",\"anim_captured\":%llu,\"anim_applied\":%llu,\"anim_rejected\":%llu,\"anim_missing\":%llu,\"anim_starts\":%llu,\"anim_stops\":%llu,\"anim_corrections\":%llu}",
                  anim_captured,anim_applied,anim_rejected,anim_missing,anim_starts,anim_stops,anim_corrections);
    result+=extra;result.pop_back();
    std::snprintf(extra,sizeof(extra),",\"death_applied\":%llu,\"death_duplicates\":%llu,\"death_rejected\":%llu,\"death_sends\":%llu,\"death_acks\":%llu,\"death_pending\":%u}",death_applied,death_duplicates,death_rejected,death_retries,death_acks,waiting);result+=extra;result.pop_back();std::snprintf(extra,sizeof(extra),",\"ownership_taken\":%llu,\"ownership_released\":%llu,\"ownership_failed\":%llu}",ownership_taken,ownership_released,ownership_failed);result+=extra;result.pop_back();std::snprintf(extra,sizeof(extra),",\"reward_applied\":%llu,\"reward_xp\":%llu,\"reward_rejected\":%llu,\"reward_sent\":%llu,\"reward_outbox\":%u,\"reward_inbox\":%u,\"reward_phase\":%u}",reward_applied,reward_xp_total,reward_rejected,reward_sent,reward_outbox.pending(),reward_inbox.pending(),static_cast<unsigned>(reward_inbox.phase()));result+=extra;result.pop_back();std::snprintf(extra,sizeof(extra),",\"bestiary_applied\":%llu,\"drop_applied\":%llu,\"drop_items\":%llu}",bestiary_applied,drop_applied,drop_items);result+=extra;result.pop_back();std::snprintf(extra,sizeof(extra),",\"receipt_enabled\":%u,\"receipt_ready\":%u,\"receipt_error\":%u,\"receipt_saved\":%llu,\"receipt_worker_thread\":%u,\"receipt_io_us\":%llu,\"receipt_io_max_us\":%llu,\"receipt_stage\":%u}",unsigned(receipt_enabled),unsigned(receipt_ready),receipt_error,static_cast<unsigned long long>(receipt_saved_cursor),receipt_worker_thread,static_cast<unsigned long long>(receipt_io_us),static_cast<unsigned long long>(receipt_io_max_us),static_cast<unsigned>(receipt_stage));return result+extra;
}
static std::string metrics(){
    measuring=false;const double elapsed=now()-metric_start;
    double render_cpu=0,rhi_cpu=0;unsigned render_count=0,rhi_count=0;
    for(auto& item:render_threads)if(item.handle){const auto time=(cpu(item.handle,false)-item.start)/10000.;if(item.rhi){rhi_cpu+=time;++rhi_count;}else{render_cpu+=time;++render_count;}CloseHandle(item.handle);item={};}
    auto summary=[](const auto& array,unsigned count){std::vector<double> values(array.begin(),array.begin()+count);std::sort(values.begin(),values.end());char s[256];
        auto p=[&](double q){return count?values[static_cast<unsigned>(q*(count-1))]:0.;};
        std::snprintf(s,sizeof(s),"{\"p50\":%.6f,\"p95\":%.6f,\"p99\":%.6f}",p(.50),p(.95),p(.99));return std::string(s);};
    char head[768];std::snprintf(head,sizeof(head),"{\"samples\":%u,\"seconds\":%.4f,\"game_thread_cpu_ms\":%.4f,\"process_cpu_ms\":%.4f,\"network_cpu_ms\":%.4f,\"render_thread_cpu_ms\":%.4f,\"rhi_thread_cpu_ms\":%.4f,\"render_threads_found\":%u,\"rhi_threads_found\":%u,\"frame_ms\":",metric_count,elapsed,(cpu(GetCurrentThread(),false)-thread_start)/10000.,(cpu(GetCurrentProcess(),true)-process_start)/10000.,worker?(cpu(worker,false)-network_start)/10000.:0.,render_cpu,rhi_cpu,render_count,rhi_count);
    return std::string(head)+summary(frame_ms,metric_count)+",\"native_work_ms\":"+summary(work_ms,metric_count)+",\"state\":"+stats()+"}";
}
// Control calls are setup/inspection only. The native mode never traverses Lua
// per frame or per enemy after registration. Lua is used for the legacy A/B arm.
// These in-process controls run on the game thread. The bootstrap owner
// schedules QDEnemyEngineTick and keeps the core loaded for the session.
extern "C" __declspec(dllexport) int QDEnemyInitializeNative(const std::uint64_t* values,unsigned count){
    const auto thread=GetCurrentThreadId();
    const auto previous_thread=game_thread.load();
    if(!values||count!=42||worker||bound||(previous_thread&&previous_thread!=thread))return 0;
    if(!native_bootstrap&&!verified_game())return 0;

    native_bootstrap=true;
    game_thread=thread;
    namespace R=qdcoop::ue427;
    const auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    native_object_array=base+0x4A64260;
    catalog_name_pool=base+0x4A27F00;
    const auto assign=base+0x11BB360;
    std::memcpy(&engine_weak_assign,&assign,sizeof(assign));
    api.weakctor=&native_weak_construct;
    api.weakget=&native_weak_get;
    api.event=&native_process_event;
    api.offset=&R::property_offset;
    api.size=&R::element_size;
    api.propertiesSize=&R::properties_size;
    api.parms=&R::parameter_size;
    api.isa=&R::is_a;
    api.getbool=&R::get_bool;
    api.setbool=&R::set_bool;

    std::array<std::uint64_t,42> args{};
    std::memcpy(args.data(),values,sizeof(args));
    if(!initialize(args))return 0;
    native_weak_state=native_event_state=native_reflection_state=native_construct_state=1;
    return 1;
}

static bool can_start_native_epoch(){
    return GetCurrentThreadId()==game_thread.load()&&native_bootstrap&&metadata_ready&&
        !native_epoch_open&&!worker&&!mode&&!reward_busy()&&!receipt_enabled;
}

static void opened_native_epoch(){
    native_epoch_open=true;
    ++native_rooms_started;
    mode=1;
}

#ifdef QDCOOP_STEAM_RECOVERY_LAB
extern "C" __declspec(dllexport) int QDEnemyProbeStartRecovery(){
    if(!can_start_native_epoch())return 0;
    steam_transport=true;
    host_role=true;
    build_state=0;
    steam_request={0,4,false};
    if(!launch_transport(probe_steam_network))return 0;
    opened_native_epoch();
    return 1;
}
extern "C" __declspec(dllexport) int QDEnemyProbeChangeRecoveryRoster(unsigned replace){
    if(GetCurrentThreadId()!=game_thread.load()||!native_epoch_open||!steam_transport||!host_role||replace>1)
        return 0;
    using namespace RecoveryFixture;
    recovery_fixture.set_roster({host,a,b,replace?replacement:0},replace?4:3);
    return 1;
}
#endif

extern "C" __declspec(dllexport) int QDEnemyStartNative(unsigned role,unsigned port,
    std::uint64_t high,std::uint64_t low,unsigned p0,unsigned p1,unsigned p2){
    if(!can_start_native_epoch()||previous_loopback==std::array{high,low}||
       role>1||port<1024||port>65535||!(high||low))return 0;
    const std::array<std::uint64_t,3> peers{p0,p1,p2};
    for(unsigned i=0;i<peers.size();++i){
        const auto peer=peers[i];
        if(!peer)continue;
        if(peer<1024||peer>65535||peer==port)return 0;
        for(unsigned j=0;j<i;++j)if(peer==peers[j])return 0;
    }
    build_state=0;
    envelope={high,low,1,1};
    if(!start_transport(role!=0,port,high,low,peers))return 0;
    previous_loopback={high,low};
    opened_native_epoch();
    return 1;
}

extern "C" __declspec(dllexport) int QDEnemyStartNativeSteam(unsigned role,std::uint64_t lobby,
    unsigned expected,unsigned friends){
    if(!can_start_native_epoch()||role>1||friends>1)return 0;
    if(role==1?(lobby||expected<2||expected>qdcoop::max_players):(!SR::lobby_id(lobby)||expected||friends))
        return 0;
    build_state=0;
    envelope={0,0,1,1};
    host_role=role==1;
    steam_request={lobby,expected,friends!=0};
    steam_transport=true;
    SR::Status status{};
    status.expected=expected;
    steam_phase(status,SR::Phase::connecting);
    if(!launch_transport(steam_network)){
        steam_transport=false;
        status.failure=static_cast<unsigned>(SR::Failure::unavailable);
        steam_phase(status,SR::Phase::failed);
        return 0;
    }
    opened_native_epoch();
    return 1;
}

extern "C" __declspec(dllexport) int QDEnemyReadNativeSteamRoom(SR::Status* out,unsigned size){
    if(!native_bootstrap||GetCurrentThreadId()!=game_thread.load()||!out||size!=sizeof(*out))return 0;
    *out=read_steam_status();
    return 1;
}

extern "C" __declspec(dllexport) int QDEnemyStartNativeLan(unsigned role,const wchar_t* ipv4,unsigned port,unsigned seats){
    if(!can_start_native_epoch()||role>1||port<1024||port>65535)return 0;
    IN_ADDR address{};
    if(role){
        if(ipv4||seats<2||seats>qdcoop::max_players)return 0;
    }else{
        if(!ipv4||seats||InetPtonW(AF_INET,ipv4,&address)!=1)return 0;
        const auto host_order=ntohl(address.s_addr);
        if(!host_order||host_order>=0xe0000000u)return 0;
    }
    build_state=0;
    host_role=role==1;
    active_peer_mask=0;
    envelope={0,0,1,1};
    lan_request={address.s_addr,port,seats};
    lan_transport=true;
    publish_lan_roster({});
    Lan::Status status;
    status.phase=Lan::Phase::waiting;
    status.seats=seats;
    publish_lan_status(status);
    if(!launch_transport(lan_network)){
        lan_transport=false;
        status.phase=Lan::Phase::failed;
        status.failure=Lan::Failure::socket;
        publish_lan_status(status);
        return 0;
    }
    opened_native_epoch();
    return 1;
}

extern "C" __declspec(dllexport) int QDEnemyReadNativeLanRoom(Lan::Status* out,unsigned size){
    if(!native_bootstrap||GetCurrentThreadId()!=game_thread.load()||!out||size!=sizeof(*out))return 0;
    *out=read_lan_status();
    return 1;
}
extern "C" __declspec(dllexport) int QDEnemyReadNativeLanRoster(Lan::LocalRoster* out,unsigned size){
    if(!native_bootstrap||GetCurrentThreadId()!=game_thread.load()||!out||size!=sizeof(*out))return 0;
    AcquireSRWLockShared(&lan_status_lock);
    *out=lan_local_roster;
    ReleaseSRWLockShared(&lan_status_lock);
    return 1;
}

extern "C" __declspec(dllexport) unsigned QDEnemyNativeStatus(char* out,unsigned capacity){
    if(!native_bootstrap||GetCurrentThreadId()!=game_thread.load())return 0;
    const auto text=stats();
    const auto required=static_cast<unsigned>(text.size()+1);
    if(out&&capacity>=required)std::memcpy(out,text.c_str(),required);
    return required;
}

static bool cleanup_native_actors(){
    remove_kill_observer();
    native_healthbar_cleanup();
    native_body_material_cleanup();
    bool cleaned=checkpoint_cleanup();
    if(!native_visual_mount_cleanup())cleaned=false;
    if(!native_pause_cleanup())cleaned=false;
    if(!native_travel_guard_cleanup())cleaned=false;
    if(!native_stock_enemies_cleanup())cleaned=false;
    if(!native_door_triggers_cleanup())cleaned=false;
    if(!native_boss_interact_cleanup())return false;
    if(!native_projectile_cleanup())cleaned=false;
    if(!native_placed_items_cleanup())cleaned=false;
    if(!native_sound_cleanup())cleaned=false;
    if(!native_damage_cleanup())cleaned=false;
    if(!world_music_cleanup())cleaned=false;
    if(!world_library_cleanup())cleaned=false;
    world_elevator_cleanup();
    if(!world_stream_cleanup())cleaned=false;
    if(!world_scene_cleanup())return false;
    if(!cleanup_native_life())cleaned=false;
    if(!cleanup_players())cleaned=false;
    for(unsigned i=0;i<E::capacity;++i){
        auto& entry=enrollments[i];
        if(!host_role&&entry.offer.id){
            if(destroy_owned_replica(entry)){
                entry.retiring=entry.offer.generation;
                complete_retirement(i);
            }else{
                cleaned=false;
                ++ownership_failed;
            }
        }else if(ownership[i].active&&!deaths[i].terminal&&
                 api.weakget(&ownership[i].objects[0])&&!ownership_release(i+1)){
            cleaned=false;
            ++ownership_failed;
        }
    }
    return cleaned;
}

extern "C" __declspec(dllexport) int QDEnemyPollNativeRecovery(){
    if(GetCurrentThreadId()!=game_thread.load()||!native_bootstrap||!native_epoch_open||
       (!steam_transport&&!lan_transport)||mode!=1||native_stopping)return 0;
    unsigned epoch{};
    if(lan_transport){
        const auto status=read_lan_status();
        if(status.phase!=Lan::Phase::recovering)return 0;
        epoch=status.epoch;
    }else{
        const auto status=read_steam_status();
        if(status.phase!=static_cast<unsigned>(SR::Phase::recovering))return 0;
        epoch=status.epoch;
    }
    auto& gate=lan_transport?lan_recovery:steam_recovery;
    if(gate.state(epoch)!=SR::Recovery::waiting)return 0;
    if(reward_busy()||receipt_enabled){
        gate.finish(epoch,SR::Recovery::blocked);
        return -1;
    }
    native_stopping=true;
    const bool clean=persistent_travel_close()&&cleanup_native_actors();
    if(clean){
        reset_native_world_epoch();
        ++native_recoveries;
        gate.finish(epoch,SR::Recovery::clean);
    }
    native_stopping=false;
    return clean?1:0;
}

extern "C" __declspec(dllexport) int QDEnemyStopNative(){
    if(GetCurrentThreadId()!=game_thread.load()||!native_bootstrap||native_stopping)return 0;
    if(!native_epoch_open)return 1;
    native_stopping=true;
    mode=0;
    if(stop_event)SetEvent(stop_event);
    const bool closed=persistent_travel_close()&&cleanup_native_actors()&&stopped.load()&&WaitForSingleObject(worker,0)==WAIT_OBJECT_0;
    if(closed){
        CloseHandle(worker);
        CloseHandle(stop_event);
        CloseHandle(wake_event);
        worker=stop_event=wake_event=nullptr;
        reset_native_epoch();
        native_epoch_open=false;
        ++native_rooms_stopped;
    }
    native_stopping=false;
    return closed?1:0;
}

static bool register_enemy(std::uint32_t id,std::uint32_t generation,void* actor,void* mesh){
    if(!metadata_ready||id<1||id>E::capacity||!generation||!actor||!mesh||
       !api.isa(actor,base_class)||!api.isa(mesh,mesh_class))return false;
    void* actual{};
    std::memcpy(&actual,static_cast<char*>(actor)+mesh_at,sizeof(actual));
    const auto index=id-1;
    const auto& previous=entities[index];
    const bool same=generation==previous.generation&&api.weakget(&previous.actor)==actor;
    if(actual!=mesh||(!same&&generation<=previous.generation)||
       !ownership_replaceable(index,generation)||
       (deaths[index].terminal&&deaths[index].event.generation>=generation))return false;

    Entity candidate{};
    if(!makeweak(candidate.actor,actor)||!makeweak(candidate.mesh,mesh))return false;
    candidate.used=true;
    candidate.generation=generation;
    entities[index]=candidate;
    anim_bindings[index]={};
    deaths[index]={};
    ownership[index]={};
    return true;
}

extern "C" __declspec(dllexport) int QDEnemyRegisterNative(
    std::uint32_t id,std::uint32_t generation,void* actor,void* mesh){
    if(!native_bootstrap||GetCurrentThreadId()!=game_thread.load())return 0;
    if(id&&id<=E::capacity){
        const auto& entry=enrollments[id-1];
        if(entry.offer.id||entry.retiring||entry.tombstone)return 0;
    }
    return register_enemy(id,generation,actor,mesh)?1:0;
}

static bool bind_display(std::uint32_t id,std::uint32_t generation,void* visual,void* mesh,void* owner_fn){
    if(!metadata_ready||host_role||id<1||id>E::capacity||!visual||!mesh||!owner_fn)return false;
    auto& e=entities[id-1];
    if(!e.used||generation!=e.generation||deaths[id-1].terminal||
       !api.isa(visual,actor_class)||!api.isa(mesh,mesh_class)||*api.parms(owner_fn)!=8)return false;
    void* owner{};
    api.event(mesh,owner_fn,&owner);
    Weak visual_key{},mesh_key{},owner_key{};
    if(owner!=visual||!makeweak(visual_key,visual)||!makeweak(mesh_key,mesh)||!makeweak(owner_key,owner_fn))return false;
    e.visual=visual_key;
    e.visual_mesh=mesh_key;
    e.owner_fn=owner_key;
    e.split=true;
    e.placed=false;
    e.hidden=e.visible=2;
    return true;
}

extern "C" __declspec(dllexport) int QDEnemyBindNativeDisplay(
    std::uint32_t id,std::uint32_t generation,void* visual,void* mesh,void* owner_fn){
    if(!native_bootstrap||GetCurrentThreadId()!=game_thread.load()||!id||id>E::capacity)return 0;
    auto& entry=enrollments[id-1];
    if(entry.retiring)return 0;
    if(entry.offer.id){
        const auto owned=api.weakget(&entry.replica),previous=api.weakget(&entry.display);
        if(entry.offer.generation!=generation||!owned||!visual||owned==visual||
           (previous&&previous!=visual)||owns_actor_elsewhere(id-1,visual))return 0;
    }
    if(!bind_display(id,generation,visual,mesh,owner_fn))return 0;
    if(entry.offer.id)entry.display=entities[id-1].visual;
    return 1;
}

extern "C" __declspec(dllexport) int QDEnemyConfigureNativeDeath(const std::uint64_t* values,unsigned count){
    if(!native_bootstrap||!metadata_ready||GetCurrentThreadId()!=game_thread.load()||kill_observer_active||!values||count!=6)return 0;
    std::array<std::uint64_t,6> args{};
    std::memcpy(args.data(),values,sizeof(args));
    return configure_death(args)?1:0;
}

extern "C" __declspec(dllexport) int QDEnemyObserveNativeKill(){
    if(!native_bootstrap||!metadata_ready||GetCurrentThreadId()!=game_thread.load()||!native_transport_ready())return 0;
    return install_kill_observer()?1:0;
}
extern "C" __declspec(dllexport) int QDEnemyQueueNativeDeath(const void* actor){
    if(!native_bootstrap||!metadata_ready||GetCurrentThreadId()!=game_thread.load()||mode!=1||!native_transport_ready())return 0;
    return queue_death(reinterpret_cast<std::uintptr_t>(actor))?1:0;
}

extern "C" __declspec(dllexport) int QDEnemyConfigureNativeOwnership(const std::uint64_t* values,unsigned count){
    if(!native_bootstrap||!metadata_ready||host_role||GetCurrentThreadId()!=game_thread.load()||
       !values||count!=24)return 0;
    for(const auto& saved:ownership)if(saved.active)return 0;
    std::array<std::uint64_t,24> args{};
    std::memcpy(args.data(),values,sizeof(args));
    return configure_ownership(args)?1:0;
}

extern "C" __declspec(dllexport) int QDEnemyTakeNativeOwnership(
    std::uint32_t id,std::uint32_t generation,const std::uintptr_t* components,unsigned count){
    if(!native_bootstrap||!metadata_ready||!worker||mode!=1||GetCurrentThreadId()!=game_thread.load()||!native_transport_ready()||
       id<1||id>E::capacity||!generation||count>16||(count&&!components)||retirement_requested(id-1))return 0;
    std::array<void*,16> audio{};
    for(unsigned i=0;i<count;++i)audio[i]=reinterpret_cast<void*>(components[i]);
    if(ownership_take(id,generation,audio,count))return 1;
    ++ownership_failed;
    return 0;
}

extern "C" __declspec(dllexport) int QDEnemyReleaseNativeOwnership(std::uint32_t id,std::uint32_t generation){
    if(!native_bootstrap||!metadata_ready||GetCurrentThreadId()!=game_thread.load()||
       id<1||id>E::capacity||!generation||entities[id-1].generation!=generation||retirement_requested(id-1))return 0;
    if(ownership_release(id))return 1;
    ++ownership_failed;
    return 0;
}

static bool native_animation_ready(){
    return native_bootstrap&&metadata_ready&&GetCurrentThreadId()==game_thread.load();
}

extern "C" __declspec(dllexport) int QDEnemyConfigureNativeEnumeration(void* function){
    return native_animation_ready()&&configure_native_enumerator(function);
}

extern "C" __declspec(dllexport) int QDEnemyEnumerateNativeEnemies(
    void* context,E::NativeCandidate* out,unsigned count){
    if(!native_animation_ready())return -1;
    return enumerate_native_enemies(context,out,count);
}

static bool native_enrollment_ready(){
    return native_animation_ready()&&worker&&mode==1&&native_transport_ready();
}

static bool configure_native_replica_destruction(void* function){
    if(!native_animation_ready())return false;
    std::string path;
    return catalog_path(function,path)&&path=="/Script/Engine.Actor.K2_DestroyActor"&&
        *api.parms(function)==0&&makeweak(replica_destroy_method,function);
}

extern "C" __declspec(dllexport) int QDEnemyConfigureNativeReplicaDestruction(void* function){
    return !host_role&&configure_native_replica_destruction(function);
}

extern "C" __declspec(dllexport) int QDEnemyRetireNativeEnemy(std::uint32_t id,std::uint32_t generation){
    if(!native_enrollment_ready()||!host_role||!Roster::valid(Roster::Retirement{id,generation}))return 0;
    return begin_host_retirement(id-1,generation);
}

extern "C" __declspec(dllexport) int QDEnemyEnrollNativeEnemy(
    std::uint32_t id,std::uint32_t generation,const AnimU::WeakKey* identity){
    if(!native_enrollment_ready()||!host_role||!id||id>E::capacity||!generation||!identity)return 0;
    const auto index=id-1;
    auto& entry=enrollments[index];
    if(entities[index].used||entry.offer.id||entry.retiring||generation<=entry.tombstone||deaths[index].waiting||
       !native_enumerated_identity(*identity))return 0;
    auto actor=reinterpret_cast<void*>(AnimU::resolve_weak(native_object_array,*identity,catalog_read));
    void* mesh{};
    Roster::Offer offer{id,generation,0,0,{}};
    if(!identify_native_enemy(actor,mesh,offer.class_id,offer.mesh_id)||
       !native_enemy_discoverable(actor,offer.class_id))return 0;
    for(const auto& entity:entities)if(entity.used&&api.weakget(&entity.actor)==actor)return 0;
    offer.location=native_actor_location(actor);
    const auto peers=active_peer_mask;
    if(!peers||!Roster::valid(offer)||!register_enemy(id,generation,actor,mesh))return 0;
    entry.offer=offer;
    entry.peers=entry.waiting=peers;
    managed_enrollment=true;
    return 1;
}

extern "C" __declspec(dllexport) int QDEnemyReadNativeEnrollment(std::uint32_t id,Roster::Offer* out){
    if(!native_enrollment_ready()||host_role||!id||id>E::capacity||!out)return 0;
    const auto& entry=enrollments[id-1];
    if(!entry.offer.id||entry.confirmed||entry.retiring)return 0;
    *out=entry.offer;
    return 1;
}

extern "C" __declspec(dllexport) int QDEnemyRegisterNativeReplica(
    const Roster::Offer* offer,void* actor,void* mesh){
    if(!native_enrollment_ready()||host_role||!offer||!Roster::valid(*offer))return 0;
    auto& entry=enrollments[offer->id-1];
    if(entry.confirmed||entry.retiring||!(entry.offer==*offer)||!api.weakget(&replica_destroy_method))return 0;
    const auto previous=api.weakget(&entry.replica);
    if((previous&&previous!=actor)||owns_actor_elsewhere(offer->id-1,actor))return 0;
    void* actual{};
    std::uint16_t class_id{},mesh_id{};
    if(!identify_native_enemy(actor,actual,class_id,mesh_id)||actual!=mesh||
       class_id!=offer->class_id||mesh_id!=offer->mesh_id)return 0;
    if(!register_enemy(offer->id,offer->generation,actor,mesh))return 0;
    entry.replica=entities[offer->id-1].actor;
    return 1;
}

extern "C" __declspec(dllexport) int QDEnemyConfirmNativeReplica(std::uint32_t id,std::uint32_t generation){
    if(!native_enrollment_ready()||host_role||!id||id>E::capacity||!generation)return 0;
    const auto index=id-1;
    auto& entry=enrollments[index];
    auto& entity=entities[index];
    if(entry.retiring||entry.offer.generation!=generation||!entity.used||entity.generation!=generation||
       !entity.split||!ownership[index].active||ownership[index].generation!=generation)return 0;
    void* mesh{};
    if(!resolve(entity,mesh))return 0;
    const auto visual=api.weakget(&entity.visual),pose=api.weakget(&entity.visual_mesh);
    const auto owner_fn=api.weakget(&entity.owner_fn);
    if(!visual||!pose||!owner_fn)return 0;
    void* owner{};
    api.event(pose,owner_fn,&owner);
    if(owner!=visual)return 0;
    E::Packet ack{};
    if(!Roster::encode(envelope,entry.offer,true,ack)||!send(ack))return 0;
    if(!entry.confirmed){
        ++enrollment_confirmed;
        --unconfirmed_offers;
    }
    entry.confirmed=true;
    return 1;
}

extern "C" __declspec(dllexport) int QDEnemyConfigureNativeAnimation(
    const std::uint64_t* functions,unsigned count){
    return native_animation_ready()&&configure_anim_functions(functions,count);
}

extern "C" __declspec(dllexport) int QDEnemyConfigureNativeAnimProfile(
    unsigned profile,void* type,const std::uint64_t* fields,unsigned count){
    if(!native_animation_ready()||!configure_anim_profile(profile,type,fields,count))return 0;
    catalog_profile_types[profile]={};
    return 1;
}

extern "C" __declspec(dllexport) int QDEnemyRegisterNativeAnimAsset(
    unsigned kind,unsigned id,void* asset,float length){
    return native_animation_ready()&&register_anim_asset(kind,id,asset,length);
}

extern "C" __declspec(dllexport) int QDEnemyBindNativeAnimation(
    std::uint32_t id,std::uint32_t generation,unsigned profile){
    if(!native_animation_ready()||!can_bind_anim(id,profile))return 0;
    const auto index=id-1;
    const auto& e=entities[index];
    if(!generation||generation!=e.generation||deaths[index].terminal||!api.weakget(&e.actor))return 0;
    auto& binding=anim_bindings[index];
    const auto mesh=api.weakget(e.split?&e.visual_mesh:&e.mesh);
    if(binding.profile==profile)return anim_instance(binding,mesh)!=nullptr;
    AnimBinding candidate{};
    candidate.profile=profile;
    if(!anim_instance(candidate,mesh))return 0;
    binding=candidate;
    return 1;
}

// Call in bounded slices during setup. A later call after completion starts a refresh.
extern "C" __declspec(dllexport) int QDEnemyScanNativeAnimation(unsigned budget){
    if(!native_animation_ready()||!budget||budget>4096)return -1;
    return scan_native_animation(budget);
}

// Returns the preview3 profile ID after validating the live source and display.
extern "C" __declspec(dllexport) unsigned QDEnemyBindCatalogNativeAnimation(
    std::uint32_t id,std::uint32_t generation){
    if(!native_animation_ready()||!catalog_ready||!id||id>E::capacity||!generation)return 0;
    const auto index=id-1;
    auto& entity=entities[index];
    if(!entity.used||entity.generation!=generation||deaths[index].terminal)return 0;
    const auto actor=api.weakget(&entity.actor);
    if(!actor)return 0;
    const auto profile=prepare_catalog_animation(entity,actor);
    if(!profile||!QDEnemyBindNativeAnimation(id,generation,profile))return 0;
    anim_bindings[index].catalog=true;
    ++catalog_binds;
    return profile;
}

#include "native_world_runtime.inc"
#include "native_world_attachment.inc"
#include "native_player_life.inc"
#include "native_checkpoint.inc"
#include "native_world_progress.inc"
#include "native_world_actor.inc"
#include "native_world_library.inc"
#include "native_world_mechanisms.inc"
#include "native_world_barriers.inc"
#include "native_pause.inc"
namespace Hit=qdcoop::enemy::damage;
static void receive_native_damage(const ReceivedPacket& received,unsigned kind,double time);
#include "native_player.inc"
#include "native_enemy_rest.inc"
#include "native_damage.inc"
#include "native_travel_guard.inc"
#include "native_world_streaming.inc"
#include "native_world_elevator.inc"
#include "native_world_music.inc"
#include "native_world_cutscene.inc"
#include "native_boss_phase.inc"
#include "native_boss_interact.inc"
#include "native_vm.inc"
#include "native_cosmetic_scope.inc"
#include "native_projectile.inc"
#include "native_door_triggers.inc"
#include "native_placed_items.inc"
#include "native_body_material.inc"
#include "native_visual_mount.inc"
#include "native_healthbar.inc"
#include "native_sound.inc"
#include "native_world_special.inc"
#include "native_story_reward.inc"
#include "native_combat_reward.inc"
static void reset_native_gameplay_scope();
#include "native_persistent_travel.inc"
#include "native_guest_safe_save.inc"

static void reset_native_gameplay_scope(){
    native_reward_unbind();
    world_scene_reset();
    native_healthbar_reset();
    native_boss_interact_reset();
    native_boss_reset();
    native_projectile_reset();
    native_placed_items_reset();
    native_sound_reset();
    reset_players();
    reset_native_life();
    checkpoint_reset();
    world_progress_reset();
    world_actor_reset();
    world_mechanisms_reset();
    world_special_reset();
    enemy_rest={};
    native_damage_reset();
    for(auto& entity:entities)entity={};
    for(auto& binding:anim_bindings)binding={};
    enrollments={};
    deaths={};
    ownership={};
    managed_enrollment=false;
    unconfirmed_offers=0;
    client_retire_pending=false;
    client_retire_cursor=0;
    clock_state={};
    native_enum_context={};
    native_enum_count=0;
    native_enum_world=0;
    automatic_context={};
    automatic_limit=automatic_guest_cursor=0;
    automatic_next_scan=0;
    factory_next_cold_load=0;
    factory_configured=false;
    ownership_ready=false;
    death_ready=false;
    attached_world={};
    world_configured=world_rescan=false;
    world_cursor=0;
    world_next_prepare=world_missing_since=world_next_rescan=0;
    world_rescan_unloaded=factory_unloaded;
    catalog_ready=false;
    catalog_scan_cursor=0;
    if(!reward_busy()&&!receipt_enabled){
        reward_inbox={};
        reward_outbox={};
        reward_ready=bestiary_ready=drop_ready=false;
    }
}

static void reset_native_world_epoch(){
    incoming.discard_quiescent();
    outgoing.discard_quiescent();
    reset_native_gameplay_scope();
    persistent_travel={};
    checkpoint_rollback_reset();
    checkpoint_world_nonce=0;
    steam_committed=false;
    lan_committed=false;
    active_peer_mask=0;
}

static void reset_native_epoch(){
    reset_native_world_epoch();
    steam_transport=false;
    lan_transport=false;
    lan_request={};
    lan_recovery.reset();
    steam_request={};
    steam_recovery.reset();
    peer_ports={};
}

extern "C" __declspec(dllexport) int qdenemy(void* L){
    if(native_bootstrap)return 0;
    if(!bind())return 0;
    alignas(16) unsigned char lua[512]{};api.construct(lua,L);View view{};std::int64_t result=-1;std::string answer;int returns=1;
    if(api.isstring(lua,1)){api.getstring(lua,&view,1);if(view.data&&view.size&&view.size<16384){
        std::string command(view.data,view.size);char op=command[0];char* p=command.data()+1;
        if(game_thread.load()&&GetCurrentThreadId()!=game_thread.load()){
            ++wrong_thread;api.setinteger(lua,-2);api.destruct(lua);return 1;
        }
        auto number=[&](std::uint64_t& value){while(*p==' ')++p;if(*p<'0'||*p>'9')return false;char* end;value=std::strtoull(p,&end,10);if(end==p)return false;p=end;return true;};
        if(op=='I'){std::array<std::uint64_t,42> args{};bool ok=!game_thread.load()||GetCurrentThreadId()==game_thread.load();for(auto& value:args)ok=ok&&number(value);result=ok&&!*p&&initialize(args);}
        else if(op=='H'){
            auto h=LoadLibraryW(L"QDEngineTickBridge.dll");int(*install)(void(*)(float)){};
            if(h&&proc(h,install,"QDInstallEngineTick")){callback_installed=install(&QDEnemyEngineTick)!=0;result=callback_installed;}
        }else if(op=='O'&&!worker&&metadata_ready&&callback_installed&&GetCurrentThreadId()==game_thread){
            std::uint64_t role{},port{},high{},low{};std::array<std::uint64_t,3> peers{};
            bool ok=number(role)&&number(port)&&number(high)&&number(low);for(auto& peer:peers)ok=ok&&number(peer);
            ok=ok&&!*p&&role<=1&&port>=1024&&port<=65535&&(high||low);
            for(auto peer:peers)ok=ok&&(peer==0||(peer>=1024&&peer<=65535&&peer!=port));
            if(ok)result=start_transport(role!=0,port,high,low,peers);
        }else if(op=='S'){answer=stats();}
        else if(op=='N'){api.setnumber(lua,now());returns=1;api.destruct(lua);return returns;}
        else if(op=='X'){for(unsigned i=0;i<E::capacity;++i)if(ownership[i].active&&!deaths[i].terminal&&!ownership_release(i+1))++ownership_failed;mode=0;if(stop_event)SetEvent(stop_event);result=stopped.load()?1:0;}
        else if(GetCurrentThreadId()!=game_thread){++wrong_thread;result=-2;}
        else if(op=='r'){answer=reward_needed();}
        else if((op=='w'||op=='q'||op=='b'||op=='a'||op=='k'||op=='l'||op=='d'||op=='j'||op=='s')&&metadata_ready){result=reward_control(op,p);}
        else if((op=='C'||op=='F'||op=='U')&&metadata_ready){result=ownership_control(op,p);if(!result)++ownership_failed;}
        else if((op=='D'||op=='K')&&metadata_ready){result=death_control(op,p);}
        else if((op=='G'||op=='A'||op=='T'||op=='Y')&&metadata_ready){result=anim_control(op,p);}
        else if(op=='R'&&metadata_ready){
            if(api.stacksize(lua)==4){
                const auto id=api.getinteger(lua,1),generation=api.getinteger(lua,1);
                auto actor=reinterpret_cast<void*>(api.getinteger(lua,1));
                auto mesh=reinterpret_cast<void*>(api.getinteger(lua,1));
                if(id>=1&&id<=E::capacity&&generation>=1&&generation<=UINT32_MAX&&
                   register_enemy(static_cast<std::uint32_t>(id),static_cast<std::uint32_t>(generation),actor,mesh))result=1;
            }
        }else if(op=='J'&&metadata_ready&&!host_role){
            if(api.stacksize(lua)==5){
                const auto id=api.getinteger(lua,1),generation=api.getinteger(lua,1);
                auto visual=reinterpret_cast<void*>(api.getinteger(lua,1));
                auto mesh=reinterpret_cast<void*>(api.getinteger(lua,1));
                auto owner_fn=reinterpret_cast<void*>(api.getinteger(lua,1));
                if(id>=1&&id<=E::capacity&&generation>=1&&generation<=UINT32_MAX&&
                   bind_display(static_cast<std::uint32_t>(id),static_cast<std::uint32_t>(generation),visual,mesh,owner_fn))result=1;
            }
        }else if(op=='M'){std::uint64_t next{};if(number(next)&&!*p&&next<=2){mode=static_cast<unsigned>(next);result=1;}}
        else if(op=='Z'){std::uint64_t world{};if(number(world)&&!*p&&world>envelope.world&&!reward_busy()&&!receipt_enabled){mode=0;reward_inbox={};reward_outbox={};envelope.world=world;entities={};anim_bindings={};deaths={};ownership={};clock_state={};result=1;}}
        else if(op=='B'){collect_render_threads();metric_count=0;previous_frame=0;metric_start=now();thread_start=cpu(GetCurrentThread(),false);process_start=cpu(GetCurrentProcess(),true);network_start=worker?cpu(worker,false):0;measuring=true;result=1;}
        else if(op=='V'){auto thread=CreateThread(nullptr,0,[](void*)->DWORD{QDEnemyEngineTick(0);return 0;},nullptr,0,nullptr);result=thread?1:0;if(thread)CloseHandle(thread);}
        else if(op=='E'){answer=metrics();}
        else if(op=='L'&&host_role&&build_state==1){E::Packet packet{};std::span<const std::uint8_t> bytes(reinterpret_cast<const std::uint8_t*>(command.data()+1),command.size()-1);result=E::text_packet(envelope,bytes,packet)&&send(packet);}
        else if(op=='P'&&!host_role){E::Packet packet{};std::span<const std::uint8_t> text;
            ReceivedPacket received{};if(incoming.pop(received)){packet=received.packet;if(E::text_payload(envelope,packet,text))answer.assign(reinterpret_cast<const char*>(text.data()),text.size());else ++rejected;}result=0;
        }
    }}
    if(!answer.empty())api.setstring(lua,answer.data(),answer.size());else api.setinteger(lua,result);api.destruct(lua);return returns;
}
