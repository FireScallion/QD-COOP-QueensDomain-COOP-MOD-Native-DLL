#include "qdcoop/object_count_map.hpp"
static Weak reward_item_class{};
static std::array<Weak,3> bestiary_metadata{};static bool bestiary_ready;
static std::array<std::int32_t,6> bestiary_layout{};static int bestiary_offset;
static std::uint64_t bestiary_applied{};
struct RewardAsset {RW::Text path{};Weak object{};};static std::array<RewardAsset,512> reward_assets{};
static bool reward_read(std::uintptr_t address,void* output,std::size_t size){SIZE_T read{};return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),output,size,&read)&&read==size;}
static bool bestiary_alive(){if(!bestiary_ready)return false;for(auto& w:bestiary_metadata)if(!api.weakget(&w)){bestiary_ready=false;return false;}return true;}
static void* reward_asset(const RW::Text& path){for(auto& a:reward_assets)if(a.path.view()==path.view())return api.weakget(&a.object);return nullptr;}
static std::string reward_needed(){const auto* e=reward_inbox.next();if(e&&(e->kind==RW::Kind::bestiary||e->kind==RW::Kind::drop)){for(unsigned i:{0u,2u,3u})if(e->text[i].size&&!reward_asset(e->text[i]))return std::string(e->text[i].view());}return {};}
static bool bestiary_count(void* save,void* data,std::int32_t& count){return qdcoop::ue427::object_count(reinterpret_cast<std::uintptr_t>(save)+bestiary_offset,reinterpret_cast<std::uintptr_t>(data),bestiary_layout,reward_read,count);}
static bool bestiary_apply(const RW::Event& event,void* player,void* gi,void* save){
 if(!bestiary_alive())return false;
 auto data=reward_asset(event.text[0]);if(!data||!api.isa(data,api.weakget(&bestiary_metadata[2])))return false;
 std::int32_t before{};if(!bestiary_count(save,data,before))return false;
 const bool set_one=event.flags&1,only_zero=event.flags&2;
 if(!set_one&&!(only_zero&&before>0)&&before==INT32_MAX)return false;
 const auto expected=(only_zero&&before>0)?before:(set_one?1:before+1);
 if(!reward_begin())return false;
 alignas(16) unsigned char args[24]{};std::memcpy(args,&data,8);args[8]=set_one;args[9]=only_zero;std::memcpy(args+16,&player,8);
 api.event(api.weakget(&bestiary_metadata[0]),api.weakget(&bestiary_metadata[1]),args);
 if(api.weakget(&reward_metadata[5])!=gi||api.weakget(&reward_metadata[6])!=save){reward_inbox.uncertain();return false;}
 void* current{};std::memcpy(&current,static_cast<char*>(gi)+reward_save_offset,8);std::int32_t after{};
 if(current!=save||!bestiary_count(save,data,after)||after!=expected){reward_inbox.uncertain();return false;}
 return reward_complete();
}
static bool bestiary_control(char op,char* p){
 auto number=[&](std::uint64_t& v){while(*p==' ')++p;if(*p<'0'||*p>'9')return false;char* end;v=std::strtoull(p,&end,10);if(end==p)return false;p=end;return true;};
 if(op=='b'&&reward_alive()){
  std::array<std::uint64_t,4> a{};for(auto& n:a)if(!number(n))return false;if(*p)return false;
  auto ptr=[&](unsigned i){return reinterpret_cast<void*>(static_cast<std::uintptr_t>(a[i]));};for(auto v:a)if(!v)return false;
  if(*api.parms(ptr(1))!=24||!property(ptr(3),80,*api.propertiesSize(api.weakget(&reward_metadata[2]))))return false;
  void*(*layout)(void*){};void**(*key)(void*){};void**(*value)(void*){};auto h=GetModuleHandleW(L"UE4SS.dll");
  if(!proc(h,layout,"?GetMapLayout@FMapProperty@Unreal@RC@@QEAAAEAUFScriptMapLayout@23@XZ")||!proc(h,key,"?GetKeyProp@FMapProperty@Unreal@RC@@QEAAAEAPEAVFProperty@23@XZ")||!proc(h,value,"?GetValueProp@FMapProperty@Unreal@RC@@QEAAAEAPEAVFProperty@23@XZ"))return false;
  auto kp=*key(ptr(3)),vp=*value(ptr(3));if(!kp||!vp||*api.size(kp)!=8||*api.size(vp)!=4)return false;
  std::array<std::int32_t,6> shape{};if(!reward_read(reinterpret_cast<std::uintptr_t>(layout(ptr(3))),shape.data(),sizeof(shape))||!qdcoop::ue427::object_int_layout(shape))return false;
  std::array<Weak,3> weak{};for(unsigned i=0;i<3;++i)if(!makeweak(weak[i],ptr(i)))return false;
  bestiary_metadata=weak;bestiary_layout=shape;bestiary_offset=*api.offset(ptr(3));bestiary_ready=true;return true;
 }
 if(op=='a'&&bestiary_alive()){
  std::uint64_t address{};if(!number(address)||!address)return false;while(*p==' ')++p;RW::Text path{};if(!path.set(p)||!RW::asset(path))return false;
  auto object=reinterpret_cast<void*>(static_cast<std::uintptr_t>(address));if(!api.isa(object,api.weakget(&bestiary_metadata[2]))&&(!api.weakget(&reward_item_class)||!api.isa(object,api.weakget(&reward_item_class))))return false;Weak weak{};if(!makeweak(weak,object))return false;
  RewardAsset* slot=nullptr;for(auto& a:reward_assets){if(a.path.view()==path.view()){slot=&a;break;}if(!slot&&!api.weakget(&a.object))slot=&a;}if(!slot)return false;slot->path=path;slot->object=weak;return true;
 }
 if(op=='k'&&host_role){std::uint64_t flags{};if(!number(flags)||flags>3)return false;while(*p==' ')++p;RW::Event e{};e.kind=RW::Kind::bestiary;e.flags=static_cast<unsigned>(flags);if(!e.text[0].set(p)||!RW::asset(e.text[0]))return false;const auto peers=active_peer_mask;return reward_outbox.enqueue(e,peers);}
 return false;
}
