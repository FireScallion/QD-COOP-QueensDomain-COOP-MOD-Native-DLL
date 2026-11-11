namespace RJ=RW::journal;
static std::unique_ptr<RJ::File> receipt_file;
static RJ::Worker<> receipt_worker;
enum class ReceiptStage {idle,begin_pending,begin_ready,effects,applied_pending,saved_pending};
static ReceiptStage receipt_stage{};static std::uint64_t receipt_sequence{},receipt_saved_cursor{},receipt_io_us{},receipt_io_max_us{};static unsigned receipt_created{},receipt_worker_thread{};
static std::array<Weak,4> receipt_meta{};
static void* receipt_map{};
static std::array<std::uint64_t,16> receipt_names{};
static std::wstring receipt_slot,receipt_path;
static RJ::Identity receipt_identity{};
static bool receipt_enabled{},receipt_ready{};static unsigned receipt_error{};
static bool receipt_fail(unsigned code){receipt_error=code;receipt_ready=false;reward_inbox.conflict();return false;}
static bool receipt_alive(){if(!reward_alive()||!drop_alive())return false;for(auto& w:receipt_meta)if(!api.weakget(&w))return false;void* current{};auto gi=api.weakget(&reward_metadata[5]);return reward_read(reinterpret_cast<std::uintptr_t>(gi)+reward_save_offset,&current,8)&&current==api.weakget(&reward_metadata[6]);}
static bool receipt_wide(std::string_view value,std::wstring& out){if(value.empty()||value.size()>2048)return false;int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0);if(n<=0)return false;out.resize(n);return MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),out.data(),n)==n;}
static void receipt_string(unsigned char* args,const std::wstring& text){auto chars=text.c_str();const std::int32_t length=static_cast<std::int32_t>(text.size()+1);std::memcpy(args,&chars,8);std::memcpy(args+8,&length,4);std::memcpy(args+12,&length,4);}
static bool receipt_read_words(void* save,RJ::MarkerWords& words){
 if(!save||!receipt_alive()||!api.isa(save,api.weakget(&reward_metadata[2])))return false;
 words={};for(unsigned i=0;i<16;++i){alignas(16) unsigned char args[16]{};std::memcpy(args,&receipt_names[i],8);api.event(save,api.weakget(&receipt_meta[0]),args);if(args[8]>1)return false;if(args[8])std::memcpy(&words[i],args+12,4);}return true;
}
static bool receipt_write_marker(void* save,std::uint64_t sequence){
 RJ::MarkerWords words{},after{};if(!RJ::marker_encode({receipt_identity,sequence},words))return false;
 auto map=static_cast<char*>(save)+*api.offset(receipt_map);std::uint32_t invalid=0;
 drop_map_add(map,receipt_map,&receipt_names[0],&invalid);
 for(unsigned i=1;i<16;++i)drop_map_add(map,receipt_map,&receipt_names[i],&words[i]);
 drop_map_add(map,receipt_map,&receipt_names[0],&words[0]);
 return receipt_read_words(save,after)&&after==words;
}
static bool receipt_disk(RJ::Marker& marker,RJ::SaveProof& proof){
 if(!receipt_alive())return false;
 RJ::SaveProof before{},after{};if(!RJ::fingerprint_save(receipt_path.c_str(),before))return false;
 alignas(16) unsigned char args[32]{};receipt_string(args,receipt_slot);api.event(api.weakget(&receipt_meta[1]),api.weakget(&receipt_meta[3]),args);void* loaded{};std::memcpy(&loaded,args+24,8);
 RJ::MarkerWords words{};if(!receipt_read_words(loaded,words)||!RJ::marker_decode(words,marker)||!RJ::fingerprint_save(receipt_path.c_str(),after)||before!=after)return false;proof=after;return true;
}
static bool receipt_save_game(void* save){
 alignas(16) unsigned char args[32]{};std::memcpy(args,&save,8);receipt_string(args+8,receipt_slot);api.event(api.weakget(&receipt_meta[1]),api.weakget(&receipt_meta[2]),args);return args[28]==1;
}
static bool receipt_checkpoint(){
 if(!receipt_ready||!receipt_worker.idle()||receipt_stage!=ReceiptStage::idle||!receipt_alive()||reward_inbox.phase()!=RW::Phase::waiting)return false;
 auto save=api.weakget(&reward_metadata[6]);RJ::MarkerWords words{};RJ::Marker memory{},disk{};RJ::SaveProof proof{};
 if(!receipt_read_words(save,words)||!RJ::marker_decode(words,memory)||memory.identity!=receipt_identity||memory.applied!=reward_inbox.applied())return receipt_fail(30);
 if(!receipt_save_game(save)||!receipt_disk(disk,proof)||disk.identity!=receipt_identity||disk.applied!=memory.applied)return receipt_fail(31);
 if(!receipt_worker.saved(memory.applied,proof))return receipt_fail(32);
 receipt_sequence=memory.applied;receipt_stage=ReceiptStage::saved_pending;
 return true;
}
static bool receipt_control(char op,char* p){
 if(op=='s'){while(*p==' '){++p;}return !*p&&receipt_checkpoint();}
 if(op!='j'||host_role||receipt_enabled||reward_inbox.applied()||reward_inbox.pending()||reward_inbox.phase()!=RW::Phase::waiting||!reward_alive()||!drop_alive())return false;
 receipt_enabled=true;
 std::array<std::uint64_t,5> addresses{};for(auto& a:addresses){while(*p==' '){++p;}if(*p<'0'||*p>'9')return receipt_fail(1);char* end{};a=std::strtoull(p,&end,10);p=end;if(!a)return receipt_fail(1);}
 while(*p==' '){++p;}if(*p!='|')return receipt_fail(1);std::string paths(p+1);const auto first=paths.find('|'),second=paths.find('|',first==std::string::npos?first:first+1);if(first==std::string::npos||second==std::string::npos||paths.find('|',second+1)!=std::string::npos)return receipt_fail(1);
 std::wstring directory;if(!receipt_wide(std::string_view(paths).substr(0,first),receipt_slot)||!receipt_wide(std::string_view(paths).substr(first+1,second-first-1),receipt_path)||!receipt_wide(std::string_view(paths).substr(second+1),directory))return receipt_fail(1);
 for(auto c:receipt_slot)if(!((c>=L'0'&&c<=L'9')||(c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||c==L'_'))return receipt_fail(1);
 std::replace(receipt_path.begin(),receipt_path.end(),L'/',L'\\');if(receipt_path.size()<receipt_slot.size()+7||receipt_path[1]!=L':'||receipt_path[2]!=L'\\'||receipt_path.substr(receipt_path.size()-receipt_slot.size()-5)!=L"\\"+receipt_slot+L".sav")return receipt_fail(1);
 auto ptr=[&](unsigned i){return reinterpret_cast<void*>(static_cast<std::uintptr_t>(addresses[i]));};
 if(*api.parms(ptr(0))!=16||*api.parms(ptr(2))!=29||*api.parms(ptr(3))!=32||!property(ptr(4),80,*api.propertiesSize(api.weakget(&reward_metadata[2]))))return receipt_fail(2);
 void**(*key)(void*){};void**(*value)(void*){};auto h=GetModuleHandleW(L"UE4SS.dll");if(!proc(h,key,"?GetKeyProp@FMapProperty@Unreal@RC@@QEAAAEAPEAVFProperty@23@XZ")||!proc(h,value,"?GetValueProp@FMapProperty@Unreal@RC@@QEAAAEAPEAVFProperty@23@XZ")||!*key(ptr(4))||!*value(ptr(4))||*api.size(*key(ptr(4)))!=8||*api.size(*value(ptr(4)))!=4)return receipt_fail(2);
 for(unsigned i=0;i<4;++i)if(!makeweak(receipt_meta[i],ptr(i)))return receipt_fail(2);
 receipt_map=ptr(4);
 for(unsigned i=0;i<16;++i){wchar_t name[80]{};std::swprintf(name,80,L"QDCOOP_Native_Reward_v1_W%02u",i);alignas(16) unsigned char args[24]{};const std::wstring text(name);receipt_string(args,text);api.event(api.weakget(&drop_meta[4]),api.weakget(&drop_meta[5]),args);std::memcpy(&receipt_names[i],args+16,8);}
 auto save=api.weakget(&reward_metadata[6]);RJ::MarkerWords words{};if(!receipt_read_words(save,words))return receipt_fail(3);
 bool empty=true;for(auto word:words)empty&=word==0;RJ::Marker memory{},disk{};RJ::SaveProof proof{};
 if(empty){std::array<std::uint64_t,3> random{};if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(random.data()),sizeof(random),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)return receipt_fail(4);receipt_identity={envelope.room_high,envelope.room_low,envelope.world,random[0],random[1],random[2]};if(!receipt_write_marker(save,0)||!receipt_save_game(save))return receipt_fail(4);memory={receipt_identity,0};}
 else {if(!RJ::marker_decode(words,memory))return receipt_fail(5);receipt_identity=memory.identity;}
 if(receipt_identity.room_high!=envelope.room_high||receipt_identity.room_low!=envelope.room_low||receipt_identity.world!=envelope.world)return receipt_fail(6);
 if(!receipt_disk(disk,proof)||disk.identity!=receipt_identity||disk.applied!=memory.applied)return receipt_fail(7);
 wchar_t name[160]{};std::swprintf(name,160,L"\\reward-%016llx-%016llx-%016llx.qdj",receipt_identity.character_high,receipt_identity.character_low,receipt_identity.save_chain);const auto path=directory+name;
 receipt_file.reset(new(std::nothrow) RJ::File(receipt_identity));if(!receipt_file)return receipt_fail(8);
 const auto attributes=GetFileAttributesW(path.c_str());if(attributes==INVALID_FILE_ATTRIBUTES){if(GetLastError()!=ERROR_FILE_NOT_FOUND||memory.applied||!receipt_file->create(path.c_str(),proof))return receipt_fail(9);}
 else {if(!receipt_file->open(path.c_str(),proof))return receipt_fail(10);if(!receipt_file->writable()&&!receipt_file->reconcile_loaded_marker(memory))return receipt_fail(11);if(receipt_file->state().applied()!=memory.applied)return receipt_fail(12);}
 if(!reward_inbox.restore_applied(memory.applied))return receipt_fail(13);
 receipt_saved_cursor=receipt_file->state().saved();if(!receipt_worker.start(std::move(receipt_file)))return receipt_fail(14);receipt_ready=true;return true;
}
static void receipt_poll(){
 RJ::Completion done{};if(!receipt_worker.poll(done))return;
 receipt_io_us+=done.microseconds;receipt_io_max_us=std::max(receipt_io_max_us,done.microseconds);receipt_worker_thread=done.thread;
 if(!done.ok||done.thread==game_thread.load()||done.sequence!=receipt_sequence){receipt_fail(40);return;}
 receipt_saved_cursor=done.saved;
 if(!receipt_ready||!receipt_alive()){receipt_fail(41);return;}
 if(receipt_stage==ReceiptStage::saved_pending&&done.operation==RJ::Operation::saved){receipt_stage=ReceiptStage::idle;return;}
 if(receipt_stage==ReceiptStage::begin_pending&&done.operation==RJ::Operation::begin){receipt_stage=ReceiptStage::begin_ready;return;}
 if(receipt_stage!=ReceiptStage::applied_pending||done.operation!=RJ::Operation::applied||reward_inbox.phase()!=RW::Phase::started){receipt_fail(42);return;}
 const auto* e=reward_inbox.next();if(!e||e->sequence!=done.applied){receipt_fail(43);return;}
 const auto kind=e->kind;const auto amount=e->amount;
 if(!reward_inbox.complete()){receipt_fail(43);return;}
 ++reward_applied;if(kind==RW::Kind::xp)reward_xp_total+=static_cast<unsigned>(amount);else if(kind==RW::Kind::bestiary)++bestiary_applied;else if(kind==RW::Kind::drop){++drop_applied;drop_items+=receipt_created;}
 receipt_stage=ReceiptStage::idle;reward_ack();
}
static bool reward_begin(){
 const auto* e=reward_inbox.next();if(!receipt_ready||!receipt_alive()||!e)return receipt_fail(20);
 if(receipt_stage==ReceiptStage::idle){receipt_sequence=e->sequence;if(!receipt_worker.begin(*e))return receipt_fail(20);receipt_stage=ReceiptStage::begin_pending;return false;}
 if(receipt_stage==ReceiptStage::begin_pending)return false;
 if(receipt_stage!=ReceiptStage::begin_ready||receipt_sequence!=e->sequence||!reward_inbox.begin())return receipt_fail(21);
 receipt_stage=ReceiptStage::effects;return true;
}
static bool reward_complete(unsigned created){
 const auto* e=reward_inbox.next();if(!e||receipt_stage!=ReceiptStage::effects||!receipt_ready||!receipt_alive()||!receipt_write_marker(api.weakget(&reward_metadata[6]),e->sequence)||!receipt_worker.applied(e->sequence))return receipt_fail(22);
 receipt_created=created;receipt_stage=ReceiptStage::applied_pending;return false;
}
