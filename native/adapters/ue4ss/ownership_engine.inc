// Cold authority transitions. Snapshot state before changing any engine object.
// Weak handles are retained only to restore a living replica on disconnect.
struct Ownership {
 bool active{};std::uint32_t generation{};std::array<Weak,7> objects{};
 bool actor_tick{},collision{},hidden{},mesh_tick{},mesh_visible{},move_tick{},controller_tick{},brain_running{},uro_tick{},los_tick{},ai_disabled{},los_enabled{};
 float dilation{};
 struct Audio {Weak component{};bool changed{};};std::array<Audio,16> audio{};unsigned audio_count{};
};
static std::array<Ownership,E::capacity> ownership{};
static std::array<Weak,18> ownership_metadata{};
static std::array<void*,6> ownership_properties{};
// UE4.27 EAudioComponentPlayState: Playing=0, Stopped=1, Paused=2.
static bool ownership_ready;
static std::uint64_t ownership_taken{},ownership_released{},ownership_failed{};
static bool ownership_replaceable(unsigned index,std::uint32_t generation){const auto& s=ownership[index];return !s.active||(generation>s.generation&&(deaths[index].terminal||!api.weakget(&s.objects[0])));}
static bool ownership_alive(){if(!ownership_ready||!death_metadata_alive())return false;for(unsigned i=0;i<ownership_metadata.size();++i)if((i!=14||ownership_properties[5])&&!api.weakget(&ownership_metadata[i])){ownership_ready=false;return false;}return true;}
static void* own_meta(unsigned i){return api.weakget(&ownership_metadata[i]);}
static bool own_bool(void* object,unsigned fn){unsigned char value{};api.event(object,own_meta(fn),&value);return value!=0;}
static void* own_pointer(void* object,unsigned prop){void* out{};if(object&&ownership_properties[prop])std::memcpy(&out,static_cast<char*>(object)+*api.offset(ownership_properties[prop]),8);return out;}
static void own_set(void* object,void* fn,bool value){unsigned char arg=static_cast<unsigned char>(value);api.event(object,fn,&arg);}
static bool ownership_take(unsigned id,std::uint32_t generation,const std::array<void*,16>& audio,unsigned audio_count){
 if(host_role||!ownership_alive())return false;
 auto& e=entities[id-1];auto& saved=ownership[id-1];
 if(!e.used||!e.split||e.generation!=generation||deaths[id-1].terminal)return false;
 if(saved.active){void* checked_mesh{};auto checked_actor=resolve(e,checked_mesh);return saved.generation==generation&&checked_actor&&checked_actor==api.weakget(&saved.objects[0])&&checked_mesh==api.weakget(&saved.objects[1]);}
 void* mesh{};auto actor=resolve(e,mesh);if(!actor)return false;
 void* controller{};api.event(actor,own_meta(4),&controller);
 if(controller&&!api.isa(controller,own_meta(12)))return false;
 void* move=own_pointer(actor,0);void* uro=own_pointer(actor,1);void* los=own_pointer(actor,2);void* brain=own_pointer(controller,4);
 void* objects[]{actor,mesh,move,controller,brain,uro,los};
 const unsigned type[]{0,0,11,12,13,10,14};auto owner_fn=api.weakget(&e.owner_fn);if(!owner_fn)return false;
 Ownership candidate{};candidate.generation=generation;
 for(unsigned i=0;i<7;++i)if(objects[i]){
  if(i>=2&&!api.isa(objects[i],own_meta(type[i])))return false;
  if(!makeweak(candidate.objects[i],objects[i]))return false;
  if(i==1||i==2||i==4||i==5||i==6){void* owner{};api.event(objects[i],owner_fn,&owner);if(owner!=(i==4?controller:actor))return false;}
 }
 candidate.actor_tick=own_bool(actor,0);candidate.collision=own_bool(actor,1);candidate.hidden=api.getbool(bool_props[3],actor,0);
 candidate.mesh_tick=own_bool(mesh,3);unsigned char args[256]{};api.event(mesh,getters[5].fn,args);candidate.mesh_visible=api.getbool(getters[5].ret,args,0);
 candidate.move_tick=move&&own_bool(move,3);candidate.controller_tick=controller&&own_bool(controller,0);candidate.brain_running=brain&&own_bool(brain,9);
 candidate.uro_tick=uro&&own_bool(uro,3);candidate.los_tick=los&&own_bool(los,3);
 candidate.ai_disabled=api.getbool(ownership_properties[3],actor,0);candidate.los_enabled=los&&api.getbool(ownership_properties[5],los,0);
 std::memcpy(&candidate.dilation,static_cast<char*>(actor)+death_dilation,4);if(!std::isfinite(candidate.dilation))return false;
 candidate.audio_count=audio_count;
 for(unsigned i=0;i<audio_count;++i){auto c=audio[i];if(!c||!api.isa(c,own_meta(15))||!makeweak(candidate.audio[i].component,c))return false;void* owner{};api.event(c,owner_fn,&owner);if(owner!=actor)return false;for(unsigned j=0;j<i;++j)if(audio[j]==c)return false;unsigned char state{};api.event(c,own_meta(17),&state);if(state>4)return false;candidate.audio[i].changed=state!=1&&state!=2;}
 candidate.active=true;saved=candidate;
 for(unsigned i=0;i<audio_count;++i)if(candidate.audio[i].changed)own_set(audio[i],own_meta(16),true);
 api.setbool(ownership_properties[3],actor,true,0);
 if(uro)own_set(uro,death_mesh_tick,false);
 if(los){api.setbool(ownership_properties[5],los,false,0);own_set(los,death_mesh_tick,false);}
 own_set(actor,hide_fn,true);own_set(actor,own_meta(2),false);own_set(actor,death_actor_tick,false);
 const float zero=0;std::memcpy(static_cast<char*>(actor)+death_dilation,&zero,4);
 if(move){api.event(move,own_meta(5),nullptr);own_set(move,death_mesh_tick,false);}
 if(controller){api.event(controller,own_meta(6),nullptr);own_set(controller,death_actor_tick,false);}
 if(brain&&candidate.brain_running){const wchar_t reason[]=L"QDCoop native authority";struct String{const wchar_t* data;std::int32_t size,capacity;}text{reason,static_cast<std::int32_t>(std::size(reason)),static_cast<std::int32_t>(std::size(reason))};static_assert(sizeof(text)==16);api.event(brain,own_meta(7),&text);}
 own_set(mesh,death_mesh_tick,false);unsigned char hidden[2]{};api.event(mesh,visible_fn,hidden);
 ++ownership_taken;return true;
}
static bool ownership_release(unsigned id){
 auto& s=ownership[id-1];auto& e=entities[id-1];if(!s.active)return true;
 if(!ownership_alive()||deaths[id-1].terminal||s.generation!=e.generation)return false;
 auto actor=api.weakget(&s.objects[0]);if(!actor){s={};e.used=false;return true;}if(actor!=api.weakget(&e.actor))return false;
 unsigned char destroyed[256]{};api.event(actor,getters[3].fn,destroyed);if(api.getbool(getters[3].ret,destroyed,0)){s={};e.used=false;return true;}
 // Do not restore a component after engine replacement, even if its old weak
 // handle is still valid (detached components may outlive their original owner).
 void* objects[7]{};for(unsigned i=0;i<7;++i)objects[i]=api.weakget(&s.objects[i]);
 void* controller{};api.event(actor,own_meta(4),&controller);
 void* actual_mesh{};std::memcpy(&actual_mesh,static_cast<char*>(actor)+mesh_at,8);
 if(objects[1]!=actual_mesh||objects[2]!=own_pointer(actor,0)||objects[3]!=controller||objects[4]!=own_pointer(controller,4)||objects[5]!=own_pointer(actor,1)||objects[6]!=own_pointer(actor,2))return false;
 e.used=false;auto visual=api.weakget(&e.visual);if(visual)own_set(visual,hide_fn,true);
 auto owner_fn=api.weakget(&e.owner_fn);for(unsigned i=0;i<s.audio_count;++i){auto c=api.weakget(&s.audio[i].component);if(c&&owner_fn){void* owner{};api.event(c,owner_fn,&owner);if(owner==actor&&s.audio[i].changed)own_set(c,own_meta(16),false);}}
 api.setbool(ownership_properties[3],actor,s.ai_disabled,0);std::memcpy(static_cast<char*>(actor)+death_dilation,&s.dilation,4);
 own_set(actor,hide_fn,s.hidden);own_set(actor,own_meta(2),s.collision);own_set(actor,death_actor_tick,s.actor_tick);
 if(objects[1]){own_set(objects[1],death_mesh_tick,s.mesh_tick);unsigned char args[2]{static_cast<unsigned char>(s.mesh_visible),0};api.event(objects[1],visible_fn,args);}
 if(objects[2])own_set(objects[2],death_mesh_tick,s.move_tick);
 if(objects[5])own_set(objects[5],death_mesh_tick,s.uro_tick);
 if(objects[6]){api.setbool(ownership_properties[5],objects[6],s.los_enabled,0);own_set(objects[6],death_mesh_tick,s.los_tick);}
 if(controller)own_set(controller,death_actor_tick,s.controller_tick);
 if(objects[4]&&s.brain_running)api.event(objects[4],own_meta(8),nullptr);
 s={};++ownership_released;return true;
}
static bool configure_ownership(const std::array<std::uint64_t,24>& args,bool optional_los=false){
    auto ptr=[&](unsigned i){return reinterpret_cast<void*>(static_cast<std::uintptr_t>(args[i]));};
    constexpr unsigned sizes[]{1,1,1,1,8,0,0,16,0,1};
    std::array<Weak,18> weak{};
    for(unsigned i=0;i<15;++i){
        if(i==14&&optional_los&&!ptr(14)&&!ptr(20))continue;
        if(!ptr(i)||(i<10&&*api.parms(ptr(i))!=sizes[i])||!makeweak(weak[i],ptr(i)))return false;
    }
    for(unsigned i=0;i<6;++i){
        if(i==5&&optional_los&&!ptr(14)&&!ptr(20))continue;
        const auto type=i==4?ptr(12):(i==5?ptr(14):base_class);
        if(!property(ptr(15+i),(i==3||i==5)?1:8,*api.propertiesSize(type)))return false;
    }
    if(!ptr(21)||!ptr(22)||!ptr(23)||*api.parms(ptr(22))!=1||*api.parms(ptr(23))!=1||
       !makeweak(weak[15],ptr(21))||!makeweak(weak[16],ptr(22))||!makeweak(weak[17],ptr(23)))return false;

    ownership_metadata=weak;
    for(unsigned i=0;i<6;++i)ownership_properties[i]=ptr(15+i);
    ownership_ready=true;
    return true;
}
static bool ownership_control(char op,char* p){
 auto number=[&](std::uint64_t& v){while(*p==' ')++p;if(*p<'0'||*p>'9')return false;char* end;v=std::strtoull(p,&end,10);if(end==p)return false;p=end;return true;};
 if(op=='C'){
  std::array<std::uint64_t,24> args{};
  for(auto& value:args)if(!number(value))return false;
  return !*p&&configure_ownership(args);
 }
 std::uint64_t id{},generation{};if(!number(id)||id<1||id>E::capacity)return false;
 if(op=='U'&&!*p)return ownership_release(static_cast<unsigned>(id));
 if(op=='F'&&number(generation)&&generation&&generation<=UINT32_MAX){std::uint64_t count{};if(!number(count)||count>16)return false;std::array<void*,16> audio{};for(unsigned i=0;i<count;++i){std::uint64_t address{};if(!number(address))return false;audio[i]=reinterpret_cast<void*>(static_cast<std::uintptr_t>(address));}if(*p)return false;return ownership_take(static_cast<unsigned>(id),static_cast<std::uint32_t>(generation),audio,static_cast<unsigned>(count));}
 return false;
}
