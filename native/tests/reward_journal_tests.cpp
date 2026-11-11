#include "qdcoop/reward_journal_windows.hpp"
#include <cassert>
#include <cstdio>
#include <string>
namespace R=qdcoop::enemy::reward;namespace J=R::journal;
static const J::Identity id{11,12,13,21,22,31};
static J::SaveProof proof(unsigned n){J::SaveProof p{};p.bytes=100+n;p.sha256[0]=static_cast<unsigned char>(n);return p;}
static R::Event event(){R::Event e{};e.sequence=1;e.kind=R::Kind::drop;e.entity=2;e.generation=3;e.counts={-1,1};e.text[0].set("/Game/Enemies/Cocker.Cocker");e.text[2].set("/Game/Items/Herb.Herb");return e;}
static std::wstring join(const std::wstring& root,const wchar_t* name){return root+L"\\"+name;}
int wmain(int argc,wchar_t** argv){
 if(argc==3){J::File f(id);assert(f.open(argv[2],proof(1))&&f.writable());assert(f.begin(event()));if(wcscmp(argv[1],L"begin")!=0)assert(f.applied(1));if(wcscmp(argv[1],L"saved")==0)assert(f.saved(proof(2)));TerminateProcess(GetCurrentProcess(),77);return 78;}
  J::MarkerWords words{};J::Marker decoded{};const J::Marker marker{id,0xfedcba9876543210ull};assert(J::marker_encode(marker,words)&&J::marker_decode(words,decoded)&&decoded.identity==id&&decoded.applied==marker.applied);for(unsigned i=0;i<16;++i){auto broken=words;broken[i]^=1;assert(!J::marker_decode(broken,decoded));}words[0]=0;assert(!J::marker_decode(words,decoded));
  R::Inbox restored;assert(!restored.restore_applied(UINT64_MAX));assert(restored.restore_applied(1)&&restored.applied()==1);assert(restored.receive(event())==R::Result::duplicate);auto next=event();next.sequence=2;assert(restored.receive(next)==R::Result::accepted&&!restored.restore_applied(9));assert(restored.begin()&&restored.complete()&&restored.applied()==2);R::Inbox waiting;assert(waiting.receive(event())==R::Result::accepted&&!waiting.restore_applied(1));R::Inbox quarantined;quarantined.conflict();assert(!quarantined.restore_applied(1));
 J::Record baseline{},begin{},applied{},saved{};assert(J::encode(id,1,J::Kind::baseline,0,nullptr,proof(1),baseline));auto e=event();assert(J::encode(id,2,J::Kind::begin,1,&e,{},begin));assert(J::encode(id,3,J::Kind::applied,1,nullptr,{},applied));assert(J::encode(id,4,J::Kind::saved,1,nullptr,proof(2),saved));
 J::State state(id);assert(state.consume(baseline)&&state.matches_save(proof(1))&&!state.recovery_required());assert(!state.consume(applied));assert(state.consume(begin)&&state.recovery_required()&&state.pending()==e);assert(!state.consume(begin));assert(!state.consume(saved));assert(state.consume(applied)&&state.recovery_required()&&state.applied()==1&&state.saved()==0);assert(state.consume(saved)&&!state.recovery_required()&&state.saved()==1&&state.matches_save(proof(2))&&!state.matches_save(proof(1)));
 for(unsigned i=0;i<J::record_size;++i){auto corrupt=baseline;corrupt.data[i]^=1;J::State reader(id);assert(!reader.consume(corrupt));}
 for(unsigned field=0;field<6;++field){auto corrupt=baseline;J::put(corrupt,24+field*8,std::uint64_t{999});J::put(corrupt,J::checksum_offset,J::crc(corrupt));J::State reader(id);assert(!reader.consume(corrupt));}
 J::Record invalid{};auto wrong=e;wrong.sequence=2;assert(J::encode(id,2,J::Kind::begin,2,&wrong,{},invalid));J::State order(id);assert(order.consume(baseline)&&!order.consume(invalid));
 wchar_t temp[MAX_PATH]{},exe[32768]{};assert(GetTempPathW(MAX_PATH,temp));assert(GetModuleFileNameW(nullptr,exe,32768));std::wstring root=std::wstring(temp)+L"QDNativeRewardJournal-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());assert(CreateDirectoryW(root.c_str(),nullptr));
 for(const auto* phase:{L"begin",L"applied",L"saved"}){
  auto path=join(root,phase);{J::File f(id);assert(f.create(path.c_str(),proof(1)));J::File concurrent(id);assert(!concurrent.open(path.c_str(),proof(1)));}
  std::wstring command=L"\""+std::wstring(exe)+L"\" "+phase+L" \""+path+L"\"";STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};assert(CreateProcessW(exe,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process));assert(WaitForSingleObject(process.hProcess,10000)==WAIT_OBJECT_0);DWORD code{};assert(GetExitCodeProcess(process.hProcess,&code)&&code==77);CloseHandle(process.hThread);CloseHandle(process.hProcess);
  const bool durable=wcscmp(phase,L"saved")==0;{J::File f(id);assert(f.open(path.c_str(),durable?proof(2):proof(1)));assert(f.writable()==durable);assert(f.state().recovery_required()!=durable);if(!durable){auto next=e;next.sequence=2;assert(!f.begin(next)&&!f.applied(1)&&!f.saved(proof(2)));}}
  if(durable){J::File stale(id);assert(stale.open(path.c_str(),proof(1))&&!stale.writable());}
    {J::File resumed(id);assert(resumed.open(path.c_str(),proof(2)));auto wrong_id=id;++wrong_id.character_low;assert(!resumed.reconcile_loaded_marker({wrong_id,1}));assert(!resumed.reconcile_loaded_marker({id,2}));assert(resumed.reconcile_loaded_marker({id,1})&&resumed.writable()&&resumed.state().saved()==1&&!resumed.state().recovery_required());assert(!resumed.reconcile_loaded_marker({id,1}));}
  {J::File reread(id);assert(reread.open(path.c_str(),proof(2))&&reread.writable()&&reread.state().saved()==1);}
  assert(DeleteFileW(path.c_str()));
 }
 auto path=join(root,L"torn");{J::File f(id);assert(f.create(path.c_str(),proof(1)));}
 HANDLE h=CreateFileW(path.c_str(),FILE_APPEND_DATA,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);assert(h!=INVALID_HANDLE_VALUE);DWORD count{};assert(WriteFile(h,begin.data.data(),J::record_size-1,&count,nullptr)&&count==J::record_size-1);assert(FlushFileBuffers(h));CloseHandle(h);{J::File f(id);assert(!f.open(path.c_str(),proof(1))&&!f.writable());}assert(DeleteFileW(path.c_str())); auto savepath=join(root,L"game.sav");h=CreateFileW(savepath.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);assert(h!=INVALID_HANDLE_VALUE);assert(WriteFile(h,"abc",3,&count,nullptr)&&count==3);assert(FlushFileBuffers(h));J::SaveProof actual{};assert(!J::fingerprint_save(savepath.c_str(),actual));CloseHandle(h);assert(J::fingerprint_save(savepath.c_str(),actual)&&actual.bytes==3);
 const std::array<std::uint8_t,32> abc{0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};assert(actual.sha256==abc);
 h=CreateFileW(savepath.c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);assert(h!=INVALID_HANDLE_VALUE);assert(WriteFile(h,"xbc",3,&count,nullptr)&&count==3);assert(FlushFileBuffers(h));CloseHandle(h);J::SaveProof changed{};assert(J::fingerprint_save(savepath.c_str(),changed)&&changed.bytes==actual.bytes&&changed!=actual);assert(DeleteFileW(savepath.c_str()));assert(RemoveDirectoryW(root.c_str()));
 std::puts("PASS reward journal: identity/CRC/order checks, exclusive file ownership, torn tail rejection; real subprocess termination after begin/applied/saved; stale game save cannot unlock replay");
 return 0;
}
