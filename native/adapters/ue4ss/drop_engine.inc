// Pinned UE4.27 bridge. Resource preflight precedes the non-repeatable roll.
static std::array<Weak,9> drop_meta{};
static std::array<void*,2> drop_maps{};static void* drop_soft_property;
static std::array<int,2> drop_map_offsets{};
static std::array<std::int32_t,6> drop_map_layout{};
static void(*drop_destroy)(void*,void*);
static void(*drop_map_add)(void*,void*,const void*,const void*);
static bool drop_ready;static std::uint64_t drop_applied{},drop_items{};
static bool drop_alive(){if(!drop_ready)return false;for(auto& w:drop_meta)if(!api.weakget(&w)){drop_ready=false;return false;}return true;}
static bool drop_control(char op,char* p){
 if(op=='d'&&host_role){
  while(*p==' '){++p;}const auto size=std::strlen(p);if(size%2||size<312||size>2712)return false;
  E::Packet packet{};packet.size=static_cast<unsigned>(size/2);
  auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};
  for(unsigned i=0;i<packet.size;++i){int a=digit(p[i*2]),b=digit(p[i*2+1]);if(a<0||b<0)return false;packet.data[i]=static_cast<std::uint8_t>((a<<4)|b);}
  RW::Event e{};if(!RW::decode(envelope,packet,e)||e.kind!=RW::Kind::drop)return false;
  const auto peers=active_peer_mask;return reward_outbox.enqueue(e,peers);
 }
 if(op!='l'||!reward_alive()||!bestiary_alive())return false;
 std::array<std::uint64_t,12> a{};for(auto& n:a){while(*p==' ')++p;if(*p<'0'||*p>'9')return false;char* end{};n=std::strtoull(p,&end,10);p=end;if(!n)return false;}if(*p)return false;
 auto ptr=[&](unsigned i){return reinterpret_cast<void*>(static_cast<std::uintptr_t>(a[i]));};
 if(*api.parms(ptr(1))!=224||*api.parms(ptr(3))!=48||*api.parms(ptr(5))!=24||*api.parms(ptr(7))!=16||!property(ptr(11),40,48)||*api.offset(ptr(11))!=8)return false;
 auto h=GetModuleHandleW(L"UE4SS.dll");void*(*layout)(void*){};void**(*key)(void*){};void**(*value)(void*){};
 if(!proc(h,layout,"?GetMapLayout@FMapProperty@Unreal@RC@@QEAAAEAUFScriptMapLayout@23@XZ")||!proc(h,key,"?GetKeyProp@FMapProperty@Unreal@RC@@QEAAAEAPEAVFProperty@23@XZ")||!proc(h,value,"?GetValueProp@FMapProperty@Unreal@RC@@QEAAAEAPEAVFProperty@23@XZ")||!proc(h,drop_destroy,"?DestroyValue@FProperty@Unreal@RC@@QEAAXPEAX@Z"))return false;
 std::array<std::int32_t,6> shape{};
 for(unsigned i=9;i<11;++i){if(!property(ptr(i),80,*api.propertiesSize(api.weakget(&reward_metadata[2]))))return false;auto kp=*key(ptr(i)),vp=*value(ptr(i));if(!kp||!vp||*api.size(kp)!=8||*api.size(vp)!=1||!reward_read(reinterpret_cast<std::uintptr_t>(layout(ptr(i))),shape.data(),sizeof(shape))||!qdcoop::ue427::object_int_layout(shape))return false;}
 // This thunk and call target were inspected on the SHA-pinned Steam build.
 // The outer adapter verifies the executable hash before permitting engine work.
 auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));std::uintptr_t exec{};
 if(!reward_read(a[8]+216,&exec,8)||exec!=base+0x2d7f640)return false;
 unsigned char call[5]{};std::int32_t displacement{};if(!reward_read(exec+0x20a,call,5)||call[0]!=0xe8)return false;std::memcpy(&displacement,call+1,4);auto target=exec+0x20f+displacement;if(target!=base+0x2755ec0)return false;
 std::array<Weak,9> weak{};for(unsigned i=0;i<9;++i)if(!makeweak(weak[i],ptr(i)))return false;
 std::memcpy(&drop_map_add,&target,sizeof(target));drop_meta=weak;reward_item_class=weak[6];drop_map_layout=shape;
 for(unsigned i=0;i<2;++i){drop_maps[i]=ptr(9+i);drop_map_offsets[i]=*api.offset(ptr(9+i));}drop_soft_property=ptr(11);drop_ready=true;return true;
}
static bool drop_apply(const RW::Event& e,void* player,void* gi,void* save){
 if(!drop_alive()||!bestiary_alive())return false;
 auto data=reward_asset(e.text[0]);if(!data||!api.isa(data,api.weakget(&bestiary_metadata[2])))return false;
 std::array<void*,2> items{};for(unsigned i=0;i<2;++i)if(e.text[i+2].size){items[i]=reward_asset(e.text[i+2]);if(!items[i]||!api.isa(items[i],api.weakget(&drop_meta[6])))return false;}
 void* actor=nullptr;if(e.entity){auto& entity=entities[e.entity-1];if(!entity.used||entity.generation!=e.generation)return false;actor=api.weakget(&entity.actor);}
 // A missing actor may use the player as in preview3; a reused identity may not.
 void* dropper=actor?actor:player;
 for(unsigned i=0;i<2;++i){bool value{};if(!qdcoop::ue427::object_bool(reinterpret_cast<std::uintptr_t>(save)+drop_map_offsets[i],reinterpret_cast<std::uintptr_t>(data),drop_map_layout,reward_read,value))return false;}
 alignas(16) unsigned char args[224]{};
 for(unsigned i=0;i<2;++i){alignas(16) unsigned char soft[48]{};std::memcpy(soft,&items[i],8);api.event(api.weakget(&drop_meta[2]),api.weakget(&drop_meta[3]),soft);std::memcpy(args+(i?64:8),soft+8,40);std::memset(soft+8,0,40);}
 auto cleanup=[&](){drop_destroy(drop_soft_property,args+8);drop_destroy(drop_soft_property,args+64);};
 // Borrowed, immutable UTF-16 input is used only during this synchronous call.
 std::array<wchar_t,301> name{};const int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,e.text[1].data.data(),e.text[1].size,name.data(),300);
 if(e.text[1].size&&count<=0){cleanup();return false;}
 alignas(16) unsigned char name_args[24]{};auto chars=name.data();const std::int32_t length=count+1;std::memcpy(name_args,&chars,8);std::memcpy(name_args+8,&length,4);std::memcpy(name_args+12,&length,4);
 api.event(api.weakget(&drop_meta[4]),api.weakget(&drop_meta[5]),name_args);std::memcpy(args+160,name_args+16,8);
 std::memcpy(args,e.chances.data(),8);std::memcpy(args+52,e.chances.data()+2,8);std::memcpy(args+48,&e.counts[0],4);std::memcpy(args+104,&e.counts[1],4);
 std::memcpy(args+112,e.values.data(),24);args[136]=(e.flags&1)!=0;std::memcpy(args+144,&dropper,8);args[152]=(e.flags&2)!=0;args[153]=(e.flags&4)!=0;args[154]=(e.flags&8)!=0;
 std::memcpy(args+156,&e.values[12],4);std::memcpy(args+168,&e.values[13],4);std::memcpy(args+172,e.values.data()+6,12);args[184]=(e.flags&16)!=0;args[185]=(e.flags&32)!=0;std::memcpy(args+188,e.values.data()+9,12);std::memcpy(args+200,&player,8);
 if(!reward_begin()){cleanup();return false;}
 api.event(api.weakget(&drop_meta[0]),api.weakget(&drop_meta[1]),args);
 std::array<void*,2> spawned{};std::memcpy(spawned.data(),args+208,16);cleanup();
 void* current{};if(api.weakget(&reward_metadata[5])!=gi||api.weakget(&reward_metadata[6])!=save||!reward_read(reinterpret_cast<std::uintptr_t>(gi)+reward_save_offset,&current,8)||current!=save){reward_inbox.uncertain();return false;}
 std::array<Weak,2> spawned_weak{};unsigned created=0;
 for(unsigned i=0;i<2;++i)if(spawned[i]){if(!makeweak(spawned_weak[i],spawned[i])){reward_inbox.uncertain();return false;}unsigned char value=1;drop_map_add(static_cast<char*>(save)+drop_map_offsets[i],drop_maps[i],&data,&value);bool after{};if(!qdcoop::ue427::object_bool(reinterpret_cast<std::uintptr_t>(save)+drop_map_offsets[i],reinterpret_cast<std::uintptr_t>(data),drop_map_layout,reward_read,after)||!after){reward_inbox.uncertain();return false;}++created;}
 if(actor&&api.weakget(&entities[e.entity-1].actor)==actor){api.event(actor,api.weakget(&drop_meta[7]),spawned.data());}
  if(api.weakget(&reward_metadata[5])!=gi||api.weakget(&reward_metadata[6])!=save||!reward_read(reinterpret_cast<std::uintptr_t>(gi)+reward_save_offset,&current,8)||current!=save){reward_inbox.uncertain();return false;}
 for(unsigned i=0;i<2;++i)if(spawned[i]){bool after{};if(!qdcoop::ue427::object_bool(reinterpret_cast<std::uintptr_t>(save)+drop_map_offsets[i],reinterpret_cast<std::uintptr_t>(data),drop_map_layout,reward_read,after)||!after){reward_inbox.uncertain();return false;}}
 return reward_complete(created);
}
