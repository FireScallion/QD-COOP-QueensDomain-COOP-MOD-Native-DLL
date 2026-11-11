// Included after the adapter's API/Weak helpers; all callers are game-thread owned.
namespace A=qdcoop::enemy::animation;
struct AnimField {void* property{};unsigned offset{};char kind{};};
struct AnimProfile {Weak type{};std::array<void*,5> flags{};unsigned aim{};std::array<AnimField,8> fields{};bool ready{};};
struct AnimAsset {Weak weak{};void* address{};float length{};bool catalog_checked{};};
static std::array<AnimProfile,std::size(A::layouts)> anim_profiles{};
static std::array<AnimAsset,std::size(A::montages)> anim_montages{};
static std::array<AnimAsset,std::size(A::sequences)> anim_sequences{};
static std::array<Weak,9> anim_functions{};
static bool anim_ready;
static std::uint64_t anim_captured{},anim_applied{},anim_rejected{},anim_missing{},anim_starts{},anim_stops{},anim_corrections{};
struct AnimBinding {
 unsigned profile{};Weak instance{};std::array<A::Frame,24> frames{};unsigned count{};
 std::uint64_t serial{},applied_sequence{};unsigned old_montage{};float old_position{};
 unsigned playing{};std::uint64_t playing_serial{};Weak playing_asset{};
 bool catalog{};
};
static std::array<AnimBinding,E::capacity> anim_bindings{};
static unsigned native_catalog_asset_id(unsigned kind,void* asset);
static void* anim_function(unsigned i){return api.weakget(&anim_functions[i]);}
static void* anim_instance(AnimBinding& b,void* mesh){
 if(!anim_ready||!b.profile||!mesh)return nullptr;
 auto& p=anim_profiles[b.profile];auto type=api.weakget(&p.type);auto fn=anim_function(0);
 if(!p.ready||!type||!fn)return nullptr;
 void* instance{};api.event(mesh,fn,&instance);
 if(!instance||!api.isa(instance,type))return nullptr;
 if(api.weakget(&b.instance)!=instance){
  if(!makeweak(b.instance,instance))return nullptr;
  b.applied_sequence=0;b.playing=0;b.playing_serial=0;b.playing_asset={};
 }
 return instance;
}
template<std::size_t N>static unsigned anim_asset_id(const std::array<AnimAsset,N>& assets,void* ptr){
 if(!ptr)return 0;
 for(unsigned i=1;i<N;++i)if(assets[i].address==ptr&&api.weakget(&assets[i].weak)==ptr)return i;
 return UINT32_MAX;
}
static bool anim_capture(unsigned index,void* mesh,const E::State& state,A::Frame& out){
 auto& b=anim_bindings[index];auto instance=anim_instance(b,mesh);if(!instance)return false;
 auto& p=anim_profiles[b.profile];out={};out.id=state.id;out.generation=state.generation;out.sequence=state.sequence;out.time=state.time;out.profile=b.profile;
 for(unsigned i=0;i<5;++i)if(api.getbool(p.flags[i],instance,0))out.flags|=1u<<i;
 std::memcpy(&out.aim,static_cast<char*>(instance)+p.aim,4);
 auto current=anim_function(1),position=anim_function(4),rate=anim_function(5);if(!current||!position||!rate)return false;
 void* montage{};api.event(instance,current,&montage);
 out.montage=b.catalog?native_catalog_asset_id(0,montage):anim_asset_id(anim_montages,montage);
 if(out.montage==UINT32_MAX){++anim_missing;return false;}
 alignas(16) unsigned char args[32]{};
 if(montage){std::memcpy(args,&montage,8);api.event(instance,position,args);std::memcpy(&out.position,args+8,4);std::memcpy(args,&montage,8);api.event(instance,rate,args);std::memcpy(&out.rate,args+8,4);}
 if(A::restart(b.old_montage,out.montage,b.old_position,out.position))++b.serial;
 b.old_montage=out.montage;b.old_position=out.position;out.serial=b.serial;
 for(unsigned i=0;i<A::layouts[b.profile].count;++i){const auto& f=p.fields[i];auto address=static_cast<char*>(instance)+f.offset;
  if(f.kind=='b')out.inputs[i]=api.getbool(f.property,instance,0)?1.f:0.f;
  else if(f.kind=='i'){std::int32_t value{};std::memcpy(&value,address,4);out.inputs[i]=static_cast<float>(value);}
  else if(f.kind=='a'){
   void* asset{};std::memcpy(&asset,address,8);
   const auto id=b.catalog?native_catalog_asset_id(1,asset):anim_asset_id(anim_sequences,asset);
   if(id==UINT32_MAX){++anim_missing;return false;}
   out.inputs[i]=static_cast<float>(id);
  }
  else std::memcpy(&out.inputs[i],address,4);
 }
 if(!A::valid(out)){++anim_rejected;return false;}++anim_captured;return true;
}
static bool anim_push(const A::Frame& f){
 auto& b=anim_bindings[f.id-1];auto& e=entities[f.id-1];
 if(!e.used||e.generation!=f.generation||b.profile!=f.profile)return false;
 if(b.count&&(f.sequence<=b.frames[b.count-1].sequence||f.time<=b.frames[b.count-1].time))return false;
 if(b.count==b.frames.size()){for(unsigned i=1;i<b.count;++i)b.frames[i-1]=b.frames[i];--b.count;}
 b.frames[b.count++]=f;return true;
}
static void* anim_resolve_asset(const AnimAsset& asset,bool catalog){
 return !catalog||asset.catalog_checked?api.weakget(&asset.weak):nullptr;
}
static void anim_apply(unsigned index,double host_time){
 auto& e=entities[index];auto& b=anim_bindings[index];if(!e.used||!b.count)return;
 auto mesh=api.weakget(e.split?&e.visual_mesh:&e.mesh);auto instance=anim_instance(b,mesh);if(!instance)return;
 unsigned at=0;while(at+1<b.count&&b.frames[at+1].time<=host_time)++at;
 const auto& a=b.frames[at];auto f=a;
 if(at+1<b.count){const auto& z=b.frames[at+1];f=A::blend(a,z,(host_time-a.time)/(z.time-a.time));}
 auto& p=anim_profiles[b.profile];
 // Resolve every asset before writes; missing asset leaves this snapshot retryable.
 void* montage=f.montage?anim_resolve_asset(anim_montages[f.montage],b.catalog):nullptr;
 if(f.montage&&!montage){++anim_missing;return;}
 std::array<void*,8> assets{};
 for(unsigned i=0;i<A::layouts[b.profile].count;++i)if(p.fields[i].kind=='a'&&f.inputs[i]>0){assets[i]=anim_resolve_asset(anim_sequences[static_cast<unsigned>(f.inputs[i])],b.catalog);if(!assets[i]){++anim_missing;return;}}
 for(unsigned i=0;i<A::layouts[b.profile].count;++i){const auto& field=p.fields[i];auto address=static_cast<char*>(instance)+field.offset;
  if(field.kind=='b')api.setbool(field.property,instance,f.inputs[i]!=0,0);
  else if(field.kind=='i'){const auto value=static_cast<std::int32_t>(f.inputs[i]);std::memcpy(address,&value,4);}
  else if(field.kind=='a')std::memcpy(address,&assets[i],8);
  else std::memcpy(address,&f.inputs[i],4);
 }
 if(b.applied_sequence==a.sequence)return;
 for(unsigned i=0;i<5;++i)api.setbool(p.flags[i],instance,(f.flags&(1u<<i))!=0,0);
 std::memcpy(static_cast<char*>(instance)+p.aim,&f.aim,4);
 auto play=anim_function(2),stop=anim_function(3),position=anim_function(4),setrate=anim_function(6),setposition=anim_function(7),isplaying=anim_function(8);
 if(!play||!stop||!position||!setrate||!setposition||!isplaying)return;
 alignas(16) unsigned char args[32]{};
 if(!f.montage){auto old=api.weakget(&b.playing_asset);if(old){const float blend=.1f;std::memcpy(args,&blend,4);std::memcpy(args+8,&old,8);api.event(instance,stop,args);++anim_stops;}b.playing=0;b.playing_asset={};b.playing_serial=f.serial;}
 else {
  const float target=static_cast<float>(A::advance(f.montage,f.position,host_time-f.time,f.rate));
  if(b.playing!=f.montage||b.playing_serial!=f.serial){
   if(target<anim_montages[f.montage].length){std::memcpy(args,&montage,8);std::memcpy(args+8,&f.rate,4);std::memcpy(args+16,&target,4);args[20]=1;api.event(instance,play,args);++anim_starts;}
   b.playing=f.montage;b.playing_serial=f.serial;makeweak(b.playing_asset,montage);
  }else{
   std::memcpy(args,&montage,8);api.event(instance,isplaying,args);
   if(args[8]){std::memset(args,0,sizeof(args));std::memcpy(args,&montage,8);std::memcpy(args+8,&f.rate,4);api.event(instance,setrate,args);api.event(instance,position,args);float current{};std::memcpy(&current,args+8,4);
    if(target<anim_montages[f.montage].length&&A::drift(f.montage,current,target)>.18){std::memcpy(args+8,&target,4);api.event(instance,setposition,args);++anim_corrections;}}
  }
 }
 b.applied_sequence=a.sequence;++anim_applied;
}
static void* anim_pointer(std::uint64_t value){
 return reinterpret_cast<void*>(static_cast<std::uintptr_t>(value));
}
static bool configure_anim_functions(const std::uint64_t* functions,unsigned count){
 if(!functions||count!=anim_functions.size())return false;
 std::array<Weak,9> candidate{};
 constexpr unsigned sizes[]{8,8,28,16,12,12,12,12,9};
 for(unsigned i=0;i<count;++i){
  auto fn=anim_pointer(functions[i]);
  if(!fn||*api.parms(fn)!=sizes[i]||!makeweak(candidate[i],fn))return false;
 }
 anim_functions=candidate;
 anim_ready=true;
 return true;
}
static unsigned anim_field_width(char kind){return kind=='r'||kind=='v'?3:1;}
static unsigned anim_field_count(std::uint64_t profile){
 if(!profile||profile>=anim_profiles.size())return 0;
 const auto& layout=A::layouts[profile];
 unsigned count=6;
 for(unsigned i=0;i<layout.count;i+=anim_field_width(layout.kinds[i]))++count;
 return count;
}
static bool configure_anim_profile(std::uint64_t id,void* type,const std::uint64_t* fields,unsigned count){
 const auto expected=anim_field_count(id);
 if(!expected||count!=expected||!type||!fields)return false;
 const int size=*api.propertiesSize(type);
 if(size<1||size>1048576)return false;
 AnimProfile candidate{};
 if(!makeweak(candidate.type,type))return false;
 unsigned next=0;
 for(auto& field:candidate.flags){
  field=anim_pointer(fields[next++]);
  if(!property(field,1,size))return false;
 }
 auto aim=anim_pointer(fields[next++]);
 if(!property(aim,4,size))return false;
 candidate.aim=*api.offset(aim);
 const auto& layout=A::layouts[id];
 for(unsigned i=0;i<layout.count;){
  const char kind=layout.kinds[i];
  auto prop=anim_pointer(fields[next++]);
  const unsigned width=anim_field_width(kind);
  const int bytes=width==3?12:kind=='b'?1:kind=='a'?8:4;
  if(!property(prop,bytes,size))return false;
  const auto offset=static_cast<unsigned>(*api.offset(prop));
  for(unsigned j=0;j<width;++j)candidate.fields[i+j]={prop,offset+4*j,kind};
  i+=width;
 }
 candidate.ready=true;
 anim_profiles[id]=candidate;
 return true;
}
static bool can_bind_anim(std::uint64_t id,std::uint64_t profile){
 return id&&id<=E::capacity&&profile&&profile<anim_profiles.size()&&anim_profiles[profile].ready&&entities[id-1].used;
}
static bool register_anim_asset(std::uint64_t kind,std::uint64_t id,void* ptr,double length){
 if(!ptr||!std::isfinite(length)||length<=0||length>3600||kind>1||!id||
    id>=(kind?anim_sequences.size():anim_montages.size()))return false;
 AnimAsset asset{};asset.address=ptr;asset.length=static_cast<float>(length);if(!makeweak(asset.weak,ptr))return false;
 if(kind)anim_sequences[id]=asset;else anim_montages[id]=asset;
 return true;
}
static bool anim_control(char op,char* text){
 char* cursor=text;bool ok=true;
 auto number=[&](){while(*cursor==' ')++cursor;char* end{};const auto n=std::strtoull(cursor,&end,10);if(*cursor<'0'||*cursor>'9'||end==cursor)ok=false;cursor=end;return n;};
 if(op=='G'){
  std::array<std::uint64_t,9> functions{};
  for(auto& fn:functions)fn=number();
  return ok&&!*cursor&&configure_anim_functions(functions.data(),static_cast<unsigned>(functions.size()));
 }
 if(op=='A'){
  const auto id=number();auto type=anim_pointer(number());
  const auto count=anim_field_count(id);
  std::array<std::uint64_t,14> fields{};
  for(unsigned i=0;i<count;++i)fields[i]=number();
  return ok&&!*cursor&&configure_anim_profile(id,type,fields.data(),count);
 }
 if(op=='T'){
  const auto id=number(),profile=number();if(!ok||*cursor||!can_bind_anim(id,profile))return false;
  anim_bindings[id-1]={};anim_bindings[id-1].profile=static_cast<unsigned>(profile);return true;
 }
 if(op=='Y'){
  const auto kind=number(),id=number();auto ptr=anim_pointer(number());while(*cursor==' ')++cursor;char* end{};const auto length=std::strtod(cursor,&end);
  return ok&&end!=cursor&&!*end&&register_anim_asset(kind,id,ptr,length);
 }
 return false;
}
