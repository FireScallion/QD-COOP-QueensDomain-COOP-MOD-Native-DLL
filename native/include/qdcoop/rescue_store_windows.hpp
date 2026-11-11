#pragma once
#include "qdcoop/rescue_ledger.hpp"
#include "qdcoop/reward_journal_windows.hpp"
#include <atomic>
#include <cwchar>
#include <span>
#include <string>

namespace qdcoop::rescue {
struct FileHandle {
    HANDLE value{INVALID_HANDLE_VALUE};
    FileHandle()=default;
    explicit FileHandle(HANDLE handle):value(handle){}
    FileHandle(const FileHandle&)=delete;
    FileHandle& operator=(const FileHandle&)=delete;
    ~FileHandle(){close();}
    void close(){
        if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);
        value=INVALID_HANDLE_VALUE;
    }
    explicit operator bool() const {return value!=INVALID_HANDLE_VALUE;}
};
inline bool rescue_random(Id& id){
    return BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&id),sizeof(id),
                           BCRYPT_USE_SYSTEM_PREFERRED_RNG)>=0&&id.valid();
}
inline std::wstring rescue_suffix(const Id& id){
    wchar_t text[33]{};
    std::swprintf(text,std::size(text),L"%016llx%016llx",
                  static_cast<unsigned long long>(id.high),static_cast<unsigned long long>(id.low));
    return text;
}
inline bool durable_publish_file(const std::wstring& path,std::span<const std::uint8_t> bytes,bool replace){
    Id random{};
    if(!rescue_random(random))return false;
    const auto stage=path+L".tmp-"+rescue_suffix(random);
    FileHandle file(CreateFileW(stage.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,
                               FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr));
    if(!file)return false;
    DWORD written{};
    const bool stored=WriteFile(file.value,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)&&
                      written==bytes.size()&&FlushFileBuffers(file.value);
    file.close();
    const auto flags=MOVEFILE_WRITE_THROUGH|(replace?MOVEFILE_REPLACE_EXISTING:0);
    if(stored&&MoveFileExW(stage.c_str(),path.c_str(),flags))return true;
    DeleteFileW(stage.c_str());
    return false;
}
inline bool durable_rescue_proof(const std::wstring& path,SaveProof& proof){
    FileHandle file(CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
    BY_HANDLE_FILE_INFORMATION info{};
    if(!file||!GetFileInformationByHandle(file.value,&info)||(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)||
       !FlushFileBuffers(file.value))return false;
    file.close();
    return enemy::reward::journal::fingerprint_save(path.c_str(),proof);
}

// The primary slot is read-only here. A per-slot owner lock serializes
// recovery and writes even if two game processes open the same character.
class Store {
    FileHandle lock_;
    std::wstring path_;
    Ledger latest_{};
public:
    const Ledger& latest() const {return latest_;}
    bool open(const std::wstring& primary,SaveProof& disk,Id branch={}){
        close();
        path_=primary+L".qdrescue-native-v1";
        if(branch.valid())path_+=L"-"+rescue_suffix(branch);
        const auto lock=path_+L".lock";
        lock_.value=CreateFileW(lock.c_str(),GENERIC_READ|GENERIC_WRITE|DELETE,0,nullptr,OPEN_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL|FILE_FLAG_DELETE_ON_CLOSE|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        constexpr auto share=FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE;
        SaveProof check{};
        if(!lock_||!enemy::reward::journal::fingerprint_save(primary.c_str(),disk,share)||
           !enemy::reward::journal::fingerprint_save(primary.c_str(),check,share)||disk!=check)return false;
        FileHandle file(CreateFileW(path_.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
        if(file){
            BY_HANDLE_FILE_INFORMATION info{};
            LARGE_INTEGER size{};
            Blob blob{};
            DWORD got{};
            return GetFileInformationByHandle(file.value,&info)&&!(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)&&
                   GetFileSizeEx(file.value,&size)&&size.QuadPart==static_cast<LONGLONG>(blob.size())&&
                   ReadFile(file.value,blob.data(),static_cast<DWORD>(blob.size()),&got,nullptr)&&got==blob.size()&&
                   decode_ledger(blob,latest_)&&(!branch.valid()||latest_.identity==branch);
        }
        if(GetLastError()!=ERROR_FILE_NOT_FOUND)return false;
        Ledger first{};
        first.identity=branch;
        if(!branch.valid()&&!rescue_random(first.identity))return false;
        first.anchor=disk;
        first.generation=1;
        return commit(first);
    }
    bool commit(const Ledger& value){
        if(!lock_||latest_.generation==UINT64_MAX||value.generation!=latest_.generation+1||
           (latest_.generation&&(value.identity!=latest_.identity||value.anchor!=latest_.anchor)))return false;
        Blob blob{};
        if(!encode_ledger(value,blob)||!durable_publish_file(path_,blob,true))return false;
        latest_=value;
        return true;
    }
    void close(){lock_.close();path_.clear();latest_={};}
};

enum class StorageOp { open, commit, proof, close };
struct StorageResult {
    StorageOp op{};
    Ledger ledger;
    SaveProof proof;
    bool ok{};
};
class StorageWorker {
    enum class Phase { idle, queued, done };
    std::atomic<Phase> phase_{Phase::idle};
    std::atomic<bool> stopping_{};
    HANDLE wake_{},thread_{};
    StorageOp op_{};
    std::wstring path_;
    Ledger candidate_{};
    StorageResult result_{};

    static DWORD WINAPI entry(void* self){return static_cast<StorageWorker*>(self)->run();}
    DWORD run(){
        Store store;
        while(WaitForSingleObject(wake_,INFINITE)==WAIT_OBJECT_0){
            if(stopping_.load(std::memory_order_acquire))break;
            if(phase_.load(std::memory_order_acquire)!=Phase::queued)continue;
            StorageResult result{};
            result.op=op_;
            switch(op_){
            case StorageOp::open:result.ok=store.open(path_,result.proof,candidate_.identity);break;
            case StorageOp::commit:result.ok=store.commit(candidate_);break;
            case StorageOp::proof:
                result.ok=durable_rescue_proof(path_,result.proof);
                break;
            case StorageOp::close:store.close();result.ok=true;break;
            }
            result.ledger=store.latest();
            result_=result;
            phase_.store(Phase::done,std::memory_order_release);
        }
        return 0;
    }
    bool submit(StorageOp op,const std::wstring& path,const Ledger& candidate){
        if(phase_.load(std::memory_order_acquire)!=Phase::idle)return false;
        if(!thread_){
            wake_=CreateEventW(nullptr,FALSE,FALSE,nullptr);
            if(!wake_)return false;
            thread_=CreateThread(nullptr,0,entry,this,0,nullptr);
            if(!thread_){CloseHandle(wake_);wake_=nullptr;return false;}
        }
        op_=op;
        path_=path;
        candidate_=candidate;
        phase_.store(Phase::queued,std::memory_order_release);
        if(SetEvent(wake_))return true;
        phase_.store(Phase::idle,std::memory_order_release);
        return false;
    }
public:
    StorageWorker()=default;
    StorageWorker(const StorageWorker&)=delete;
    StorageWorker& operator=(const StorageWorker&)=delete;
    ~StorageWorker(){
        stopping_.store(true,std::memory_order_release);
        if(wake_)SetEvent(wake_);
        if(thread_){WaitForSingleObject(thread_,INFINITE);CloseHandle(thread_);}
        if(wake_)CloseHandle(wake_);
    }
    bool open(const std::wstring& primary,Id branch={}){
        Ledger binding{};
        binding.identity=branch;
        return submit(StorageOp::open,primary,binding);
    }
    bool commit(const Ledger& value){return submit(StorageOp::commit,{},value);}
    bool proof(const std::wstring& path){return submit(StorageOp::proof,path,{});}
    bool close(){return submit(StorageOp::close,{},{});}
    bool poll(StorageResult& result){
        if(phase_.load(std::memory_order_acquire)!=Phase::done)return false;
        result=result_;
        phase_.store(Phase::idle,std::memory_order_release);
        return true;
    }
};
}
