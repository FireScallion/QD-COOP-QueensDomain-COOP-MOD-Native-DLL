#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#include "qdcoop/reward_journal.hpp"
namespace qdcoop::enemy::reward::journal {
// Hash an exclusively stable view of bytes. This alone cannot establish that
// the engine serialized the reward cursor; its save completion must also match.
inline bool fingerprint_save(const wchar_t* path,SaveProof& output,DWORD sharing=FILE_SHARE_READ){
 HANDLE file=CreateFileW(path,GENERIC_READ,sharing,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
 LARGE_INTEGER size{};bool ok=GetFileSizeEx(file,&size)&&size.QuadPart>0&&size.QuadPart<=256ll*1024*1024;
 BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};std::array<unsigned char,1024> object{};DWORD needed{},property_size{};
 if(ok)ok=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0;
 if(ok)ok=BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&needed),sizeof(needed),&property_size,0)>=0&&property_size==sizeof(needed)&&needed<=object.size();
 if(ok)ok=BCryptCreateHash(algorithm,&hash,object.data(),needed,nullptr,0,0)>=0;
 std::array<unsigned char,32768> buffer{};std::uint64_t total{};
 while(ok&&total<static_cast<std::uint64_t>(size.QuadPart)){DWORD count{};ok=ReadFile(file,buffer.data(),static_cast<DWORD>(buffer.size()),&count,nullptr)&&count&&total+count<=static_cast<std::uint64_t>(size.QuadPart);if(ok){ok=BCryptHashData(hash,buffer.data(),count,0)>=0;total+=count;}}
 SaveProof proof{};proof.bytes=total;if(ok)ok=BCryptFinishHash(hash,proof.sha256.data(),static_cast<ULONG>(proof.sha256.size()),0)>=0&&proof.valid();
 if(hash){BCryptDestroyHash(hash);}
 if(algorithm){BCryptCloseAlgorithmProvider(algorithm,0);}
 CloseHandle(file);if(ok){output=proof;}return ok;
}
// Exclusive file ownership; malformed/torn tails are never silently truncated.
// Callers must supply proof from actual persisted game-save bytes, not an ACK.
class File {
 HANDLE handle_{INVALID_HANDLE_VALUE};State state_;bool writable_{},scanned_{};SaveProof opened_save_{};
 bool append(Kind kind,std::uint64_t sequence,const Event* event,const SaveProof& proof){
  if(!writable_||state_.records()>=40000)return false;
  Record r{};if(!encode(state_.identity(),state_.records()+1,kind,sequence,event,proof,r))return false;
  State next=state_;if(!next.consume(r))return false;
  DWORD count{};if(!WriteFile(handle_,r.data.data(),record_size,&count,nullptr)||count!=record_size||!FlushFileBuffers(handle_)){writable_=false;return false;}
  state_=next;return true;
 }
public:
 explicit File(Identity id):state_(id){}
 ~File(){if(handle_!=INVALID_HANDLE_VALUE)CloseHandle(handle_);}
 File(const File&)=delete;File& operator=(const File&)=delete;
 bool create(const wchar_t* path,const SaveProof& baseline){
  if(handle_!=INVALID_HANDLE_VALUE||!baseline.valid())return false;
  handle_=CreateFileW(path,GENERIC_READ|GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr);
  if(handle_==INVALID_HANDLE_VALUE)return false;
  writable_=true;if(append(Kind::baseline,0,nullptr,baseline))return true;writable_=false;return false;
 }
 bool open(const wchar_t* path,const SaveProof& current_save){
  if(handle_!=INVALID_HANDLE_VALUE)return false;
  handle_=CreateFileW(path,GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr);
  if(handle_==INVALID_HANDLE_VALUE)return false;
  LARGE_INTEGER size{};if(!GetFileSizeEx(handle_,&size)||size.QuadPart<record_size||size.QuadPart>static_cast<LONGLONG>(record_size)*40000||size.QuadPart%record_size)return false;
  for(LONGLONG at=0;at<size.QuadPart;at+=record_size){Record r{};DWORD count{};if(!ReadFile(handle_,r.data.data(),record_size,&count,nullptr)||count!=record_size||!state_.consume(r))return false;}
  // Opening successfully does not permit replay if game persistence is uncertain.
  scanned_=true;opened_save_=current_save;writable_=state_.matches_save(current_save)&&!state_.recovery_required();return true;
 }
 // Only the local adapter reading a verified loaded save may provide a marker.
 // A stale marker cannot undo a recorded side effect; a remote ACK is rejected
 // by construction at the adapter boundary, not authenticated by this codec.
 bool reconcile_loaded_marker(const Marker& marker){
  if(!scanned_||!opened_save_.valid()||marker.identity!=state_.identity()||marker.applied==UINT64_MAX)return false;
  const auto pending=state_.pending().sequence;
  if((pending&&marker.applied!=pending)||(!pending&&marker.applied!=state_.applied()))return false;
  scanned_=false;writable_=true;
  if(pending&&!append(Kind::applied,pending,nullptr,{})){writable_=false;return false;}
  if(!append(Kind::saved,state_.applied(),nullptr,opened_save_)){writable_=false;return false;}return true;
 }
 bool begin(const Event& event){scanned_=false;return append(Kind::begin,event.sequence,&event,{});}
 bool applied(std::uint64_t sequence){scanned_=false;return append(Kind::applied,sequence,nullptr,{});}
 bool saved(const SaveProof& proof){scanned_=false;return append(Kind::saved,state_.applied(),nullptr,proof);}
 const State& state()const{return state_;}bool writable()const{return writable_;}
};
}
