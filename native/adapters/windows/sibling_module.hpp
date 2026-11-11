#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstring>
#include <cwchar>
#include <iterator>

namespace qdcoop::windows {
inline HMODULE load_system(const wchar_t* filename) {
    wchar_t path[MAX_PATH]{};
    const auto size=GetSystemDirectoryW(path,MAX_PATH);
    if(!size||size+1+std::wcslen(filename)>=MAX_PATH)return nullptr;
    path[size]=L'\\';
    std::wcscpy(path+size+1,filename);
    return LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
}

template<class Function>
bool load_export(HMODULE module,const char* name,Function& function) {
    const auto address=GetProcAddress(module,name);
    static_assert(sizeof(function)==sizeof(address));
    std::memcpy(&function,&address,sizeof(function));
    return function!=nullptr;
}

template<std::size_t N>
bool sibling_path(HMODULE self,const wchar_t* filename,wchar_t (&path)[N]) {
    const auto length=GetModuleFileNameW(self,path,static_cast<DWORD>(N));
    if(!length||length>=N)return false;
    const auto slash=std::wcsrchr(path,L'\\');
    if(!slash)return false;
    const auto offset=static_cast<std::size_t>(slash+1-path);
    if(offset+std::wcslen(filename)>=N)return false;
    std::wcscpy(path+offset,filename);
    return true;
}

inline HMODULE load_sibling(HMODULE self,const wchar_t* filename) {
    wchar_t path[32768]{};
    if(!sibling_path(self,filename,path))return nullptr;
    return LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
}
}
