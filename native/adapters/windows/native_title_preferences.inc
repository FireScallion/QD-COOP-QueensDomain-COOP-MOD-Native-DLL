namespace TitleUi {
struct Preferences {
    std::wstring directory;

    bool bind(std::wstring saved) {
        for(auto& ch:saved)if(ch==L'/')ch=L'\\';
        while(!saved.empty()&&saved.back()==L'\\')saved.pop_back();
        const auto separator=saved.find_last_of(L'\\');
        if(saved.size()<8||saved[1]!=L':'||saved[2]!=L'\\'||separator==saved.npos||
           _wcsicmp(saved.c_str()+separator+1,L"Saved")!=0)return false;
        directory=std::move(saved)+L"\\QDCoopNative";
        return true;
    }
    std::wstring path(std::wstring_view name) const {
        return directory.empty()?std::wstring{}:directory+L"\\"+std::wstring(name);
    }
    std::string read(std::wstring_view name) const {
        const auto target=path(name);
        if(target.empty())return {};
        const auto file=CreateFileW(target.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
            nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        if(file==INVALID_HANDLE_VALUE)return {};
        BY_HANDLE_FILE_INFORMATION info{};
        char bytes[64]{};
        DWORD count{};
        const bool valid=GetFileInformationByHandle(file,&info)&&!info.nFileSizeHigh&&info.nFileSizeLow<sizeof(bytes)&&
            !(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))&&
            ReadFile(file,bytes,info.nFileSizeLow,&count,nullptr)&&count==info.nFileSizeLow;
        CloseHandle(file);
        return valid?std::string(bytes,count):std::string{};
    }
    bool write(std::wstring_view name,std::string_view value) const {
        const auto target=path(name);
        if(target.empty()||value.empty()||value.size()>=64)return false;
        if(!CreateDirectoryW(directory.c_str(),nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS)return false;
        const auto attributes=GetFileAttributesW(directory.c_str());
        if(attributes==INVALID_FILE_ATTRIBUTES||!(attributes&FILE_ATTRIBUTE_DIRECTORY)||(attributes&FILE_ATTRIBUTE_REPARSE_POINT))
            return false;
        const auto temporary=target+L".new."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(GetTickCount64());
        const auto file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE)return false;
        DWORD written{};
        const auto length=static_cast<DWORD>(value.size());
        const bool valid=WriteFile(file,value.data(),length,&written,nullptr)&&written==length&&FlushFileBuffers(file);
        CloseHandle(file);
        const auto backup=target+L".bak";
        bool committed=false;
        if(valid){
            if(GetFileAttributesW(target.c_str())==INVALID_FILE_ATTRIBUTES)
                committed=MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_WRITE_THROUGH)!=0;
            else committed=ReplaceFileW(target.c_str(),temporary.c_str(),backup.c_str(),REPLACEFILE_IGNORE_MERGE_ERRORS,nullptr,nullptr)!=0;
        }
        if(!committed)DeleteFileW(temporary.c_str());
        return committed;
    }
    static unsigned parse_skin(std::string_view text) {
        constexpr std::string_view header="QDSKIN1\n";
        if(!text.starts_with(header)||text.size()<=header.size()+1||text.back()!='\n')return 0;
        const auto id=text.substr(header.size(),text.size()-header.size()-1);
        for(unsigned index=1;index<Player::skin_names.size();++index){
            const auto name=Player::skin_names[index];
            if(name.size()==id.size()&&std::equal(name.begin(),name.end(),id.begin()))return index;
        }
        return 0;
    }
    unsigned skin() const {
        if(const auto value=parse_skin(read(L"player-skin.txt")))return value;
        return parse_skin(read(L"player-skin.txt.bak"));
    }
    bool skin(unsigned index) const {
        if(!Player::valid_skin(index))return false;
        const auto id=Player::skin_names[index];
        return write(L"player-skin.txt","QDSKIN1\n"+std::string(id.begin(),id.end())+"\n");
    }
    void lan(std::array<wchar_t,16>& address,unsigned& port) const {
        auto value=read(L"last-join.txt");
        if(value.empty())value=read(L"last-join.txt.bak");
        const auto separator=value.find('|');
        if(separator==value.npos)return;
        const std::wstring wide(value.begin(),value.end());
        Room::Request request{Room::Operation::join_lan};
        if(Room::set_ipv4(std::wstring_view(wide).substr(0,separator),request.ipv4)&&
           Room::parse_lan_port(std::wstring_view(wide).substr(separator+1),request.port)){
            address=request.ipv4;
            port=request.port;
        }
    }
    bool remember_lan(const Room::Request& request) const {
        if(request.operation!=Room::Operation::join_lan||!Room::valid(request))return false;
        const auto address=Room::ipv4_view(request.ipv4);
        return write(L"last-join.txt",std::string(address.begin(),address.end())+"|"+std::to_string(request.port));
    }
};
}
