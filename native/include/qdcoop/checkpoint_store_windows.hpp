#pragma once
#include "qdcoop/checkpoint_wire.hpp"
#include "qdcoop/rescue_store_windows.hpp"

namespace qdcoop::checkpoint {
inline constexpr DWORD shared_slot=FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE;
inline bool copy_checkpoint(const std::wstring& primary,const std::wstring& archive,SaveProof& proof){
    using rescue::FileHandle;
    Id random{};
    if(!rescue::rescue_random(random))return false;
    const auto stage=archive+L".tmp-"+rescue::rescue_suffix(random);
    FileHandle source(CreateFileW(primary.c_str(),GENERIC_READ,shared_slot,nullptr,OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_SEQUENTIAL_SCAN,nullptr));
    if(!source)return false;
    BY_HANDLE_FILE_INFORMATION initial{};
    LARGE_INTEGER length{};
    if(!GetFileInformationByHandle(source.value,&initial)||(initial.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)||
       !GetFileSizeEx(source.value,&length)||length.QuadPart<4||length.QuadPart>256ll*1024*1024)return false;
    FileHandle dest(CreateFileW(stage.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,
                               FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr));
    if(!dest)return false;
    std::array<unsigned char,32768> buffer{};
    LONGLONG total{};
    bool copied=true;
    while(total<length.QuadPart){
        DWORD got{},written{};
        if(!ReadFile(source.value,buffer.data(),static_cast<DWORD>(buffer.size()),&got,nullptr)||!got||
           total+got>length.QuadPart||(!total&&(got<4||std::memcmp(buffer.data(),"GVAS",4)))||
           !WriteFile(dest.value,buffer.data(),got,&written,nullptr)||written!=got){
            copied=false;
            break;
        }
        total+=got;
    }
    copied=copied&&FlushFileBuffers(dest.value);
    source.close();
    dest.close();
    SaveProof staged{},current{};
    const auto fingerprint=enemy::reward::journal::fingerprint_save;
    const bool verified=copied&&fingerprint(stage.c_str(),staged,FILE_SHARE_READ)&&
                        fingerprint(primary.c_str(),current,shared_slot)&&staged==current;
    if(verified&&MoveFileExW(stage.c_str(),archive.c_str(),MOVEFILE_WRITE_THROUGH)){
        proof=staged;
        return true;
    }
    DeleteFileW(stage.c_str());
    return false;
}
inline bool publish_checkpoint_manifest(const std::wstring& path,const enemy::Packet& packet,SaveProof& proof){
    using rescue::FileHandle;
    if(packet.size!=wire_size)return false;
    if(!rescue::durable_publish_file(path,{packet.data.data(),packet.size},false)){
        FileHandle file(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
        BY_HANDLE_FILE_INFORMATION info{};
        LARGE_INTEGER size{};
        std::array<unsigned char,wire_size> bytes{};
        DWORD read{};
        if(!file||!GetFileInformationByHandle(file.value,&info)||(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)||
           !GetFileSizeEx(file.value,&size)||size.QuadPart!=wire_size||
           !ReadFile(file.value,bytes.data(),wire_size,&read,nullptr)||read!=wire_size||
           std::memcmp(bytes.data(),packet.data.data(),bytes.size()))return false;
    }
    return enemy::reward::journal::fingerprint_save(path.c_str(),proof);
}

// Checkpoint jobs are infrequent. All disk operations run with an owned,
// immutable request, and the owner consumes the result after the thread ends.
enum class Job { capture, verify, manifest };
struct JobResult { Job op{}; SaveProof proof; bool ok{}; };
class Worker {
    rescue::FileHandle thread_;
    std::wstring source_,target_;
    enemy::Packet manifest_{};
    JobResult result_{};
    static DWORD WINAPI entry(void* pointer){
        auto& worker=*static_cast<Worker*>(pointer);
        auto& result=worker.result_;
        switch(result.op){
        case Job::capture:result.ok=copy_checkpoint(worker.source_,worker.target_,result.proof);break;
        case Job::verify:result.ok=enemy::reward::journal::fingerprint_save(worker.target_.c_str(),result.proof);break;
        case Job::manifest:result.ok=publish_checkpoint_manifest(worker.target_,worker.manifest_,result.proof);break;
        }
        return 0;
    }
    bool launch(Job op,const std::wstring& source,const std::wstring& target,const enemy::Packet& packet){
        if(thread_)return false;
        source_=source;
        target_=target;
        manifest_=packet;
        result_={op,{},false};
        const auto thread=CreateThread(nullptr,0,entry,this,0,nullptr);
        if(!thread)return false;
        thread_.value=thread;
        return true;
    }
public:
    bool busy() const {return static_cast<bool>(thread_);}
    bool capture(const std::wstring& primary,const std::wstring& archive){return launch(Job::capture,primary,archive,{});}
    bool verify(const std::wstring& archive){return launch(Job::verify,{},archive,{});}
    bool commit(const std::wstring& path,const enemy::Packet& packet){return launch(Job::manifest,{},path,packet);}
    bool poll(JobResult& result){
        if(!thread_||WaitForSingleObject(thread_.value,0)!=WAIT_OBJECT_0)return false;
        result=result_;
        thread_.close();
        return true;
    }
};
}
