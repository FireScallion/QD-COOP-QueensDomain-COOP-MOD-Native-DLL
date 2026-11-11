namespace TitleUi {
class SaveBackup {
public:
    enum class State { idle, copying, ready, failed };

    State state() const {return state_.load(std::memory_order_acquire);}

    void start(const std::wstring& native_saved){
        if(native_saved.empty()||(state()!=State::idle&&state()!=State::failed))return;
        directory_=native_saved;
        state_.store(State::copying,std::memory_order_release);
        const auto thread=CreateThread(nullptr,0,run,this,0,nullptr);
        if(thread)CloseHandle(thread);
        else state_.store(State::failed,std::memory_order_release);
    }

private:
    struct Entry {std::wstring path;bool directory;};
    std::atomic<State> state_{State::idle};
    std::wstring directory_;

    static bool real_directory(const std::wstring& path){
        const auto attributes=GetFileAttributesW(path.c_str());
        return attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_DIRECTORY)&&
               !(attributes&FILE_ATTRIBUTE_REPARSE_POINT);
    }

    static bool create_directory(const std::wstring& path){
        return CreateDirectoryW(path.c_str(),nullptr)||
               (GetLastError()==ERROR_ALREADY_EXISTS&&real_directory(path));
    }

    static bool copy_entry(const std::wstring& source,const std::wstring& target,const WIN32_FIND_DATAW& before,
                           unsigned depth,std::vector<Entry>& created){
        const std::wstring_view name=before.cFileName;
        if(name==L"."||name==L"..")return true;
        if(before.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)return false;
        const auto path=source+L"\\"+before.cFileName;
        const auto destination=target+L"\\"+before.cFileName;
        const bool directory=(before.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0;
        if(directory){
            if(!CreateDirectoryW(destination.c_str(),nullptr))return false;
            created.push_back({destination,true});
            return copy_tree(path,destination,depth+1,created);
        }
        created.push_back({destination,false});
        if(!CopyFileW(path.c_str(),destination.c_str(),TRUE))return false;
        WIN32_FILE_ATTRIBUTE_DATA after{};
        return GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&after)&&
               !(after.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))&&
               before.nFileSizeHigh==after.nFileSizeHigh&&before.nFileSizeLow==after.nFileSizeLow&&
               CompareFileTime(&before.ftLastWriteTime,&after.ftLastWriteTime)==0;
    }

    static bool copy_tree(const std::wstring& source,const std::wstring& target,unsigned depth,std::vector<Entry>& created){
        if(depth>32||!real_directory(source))return false;
        WIN32_FIND_DATAW entry{};
        const auto find=FindFirstFileW((source+L"\\*").c_str(),&entry);
        if(find==INVALID_HANDLE_VALUE)return GetLastError()==ERROR_FILE_NOT_FOUND;
        bool valid{};
        do {
            valid=copy_entry(source,target,entry,depth,created);
        } while(valid&&FindNextFileW(find,&entry));
        const bool complete=valid&&GetLastError()==ERROR_NO_MORE_FILES;
        FindClose(find);
        return complete;
    }

    bool capture() const {
        const auto source=directory_.substr(0,directory_.find_last_of(L'\\'))+L"\\SaveGames";
        const auto attributes=GetFileAttributesW(source.c_str());
        if(attributes==INVALID_FILE_ATTRIBUTES){
            const auto error=GetLastError();
            return error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND;
        }
        if(!real_directory(source)||!create_directory(directory_))return false;
        const auto backups=directory_+L"\\Backups";
        if(!create_directory(backups))return false;
        SYSTEMTIME utc{};
        GetSystemTime(&utc);
        wchar_t date[24]{};
        if(std::swprintf(date,std::size(date),L"%04u%02u%02u-%02u%02u%02u",unsigned(utc.wYear),
            unsigned(utc.wMonth),unsigned(utc.wDay),unsigned(utc.wHour),unsigned(utc.wMinute),
            unsigned(utc.wSecond))<=0)return false;
        const auto name=std::wstring(date)+L"-"+std::to_wstring(GetCurrentProcessId())+L"-"+
            std::to_wstring(GetTickCount64());
        const auto final=backups+L"\\"+name;
        const auto temporary=final+L".partial";
        if(!CreateDirectoryW(temporary.c_str(),nullptr))return false;
        std::vector<Entry> created{{temporary,true}};
        const auto destination=temporary+L"\\SaveGames";
        if(CreateDirectoryW(destination.c_str(),nullptr)){
            created.push_back({destination,true});
            if(copy_tree(source,destination,0,created)&&
               MoveFileExW(temporary.c_str(),final.c_str(),MOVEFILE_WRITE_THROUGH))return true;
        }
        for(auto entry=created.rbegin();entry!=created.rend();++entry){
            if(entry->directory)RemoveDirectoryW(entry->path.c_str());
            else DeleteFileW(entry->path.c_str());
        }
        return false;
    }

    static DWORD WINAPI run(void* context){
        auto& backup=*static_cast<SaveBackup*>(context);
        const auto state=backup.capture()?State::ready:State::failed;
        backup.state_.store(state,std::memory_order_release);
        return 0;
    }
};
}
