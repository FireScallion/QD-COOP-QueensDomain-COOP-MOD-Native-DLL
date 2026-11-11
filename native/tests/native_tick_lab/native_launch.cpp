#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <string>
#include <vector>
#include <cstdint>
static std::uintptr_t remote_module(DWORD pid,const wchar_t* name){auto snap=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);if(snap==INVALID_HANDLE_VALUE)return 0;MODULEENTRY32W entry{};entry.dwSize=sizeof(entry);std::uintptr_t found=0;if(Module32FirstW(snap,&entry))do{if(!_wcsicmp(entry.szModule,name)){found=reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);break;}}while(Module32NextW(snap,&entry));CloseHandle(snap);return found;}
int wmain(int argc,wchar_t** argv){if(argc!=5&&argc!=6)return 2;std::wstring exe=argv[1],dll=argv[2],profile=argv[3],cwd=argv[4];
 wchar_t role[16]{};GetEnvironmentVariableW(L"QD_WORLD_ROLE",role,16);
 const wchar_t* renderer=(role[0]&&std::wcscmp(role,L"client")!=0)?L" -NullRHI":L" -d3d12 -RenderOffscreen";
 wchar_t audio[2]{};GetEnvironmentVariableW(L"QD_NATIVE_LAB_AUDIO",audio,2);
 const wchar_t* sound=audio[0]==L'1'?L"":L" -nosound";
 std::wstring command=L"\""+exe+L"\" -UserDir=\""+profile+L"\" -NoSplash -NoMoviePlayer"+renderer+L" -unattended"+sound+L" -windowed -ResX=1280 -ResY=720 -DefaultViewportMouseCaptureMode=NoCapture -ExecCmds=\"r.VSync 0,t.MaxFPS 90\"";
 if(argc==6){command+=L" ";command+=argv[5];}
 std::vector<wchar_t> buf(command.begin(),command.end());buf.push_back(0);STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
 if(!CreateProcessW(exe.c_str(),buf.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,cwd.c_str(),&si,&pi)){return 3;}CloseHandle(pi.hThread);std::printf("owned_pid=%lu\n",static_cast<unsigned long>(pi.dwProcessId));std::printf("owned_tid=%lu\n",static_cast<unsigned long>(pi.dwThreadId));std::fflush(stdout);int result=4;void* memory=nullptr;
 do{if(WaitForSingleObject(pi.hProcess,5000)!=WAIT_TIMEOUT){DWORD code{};GetExitCodeProcess(pi.hProcess,&code);std::printf("early_game_exit=0x%08lx\n",static_cast<unsigned long>(code));break;}
 auto load=GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"LoadLibraryW");HMODULE owner{};if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(load),&owner))break;
 wchar_t path[32768]{};GetModuleFileNameW(owner,path,32768);auto name=wcsrchr(path,L'\\');if(!name)break;auto remote=remote_module(pi.dwProcessId,name+1);if(!remote)break;auto entry=remote+(reinterpret_cast<std::uintptr_t>(load)-reinterpret_cast<std::uintptr_t>(owner));
 memory=VirtualAllocEx(pi.hProcess,nullptr,(dll.size()+1)*sizeof(wchar_t),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);SIZE_T bytes{};if(!memory||!WriteProcessMemory(pi.hProcess,memory,dll.c_str(),(dll.size()+1)*sizeof(wchar_t),&bytes))break;
 auto thread=CreateRemoteThread(pi.hProcess,nullptr,0,reinterpret_cast<LPTHREAD_START_ROUTINE>(entry),memory,0,nullptr);if(!thread)break;auto waited=WaitForSingleObject(thread,20000);CloseHandle(thread);if(waited!=WAIT_OBJECT_0)break;VirtualFreeEx(pi.hProcess,memory,0,MEM_RELEASE);memory=nullptr;
 std::printf("ue4ss_module=%llu\n",static_cast<unsigned long long>(remote_module(pi.dwProcessId,L"UE4SS.dll")));std::fflush(stdout);
 if(WaitForSingleObject(pi.hProcess,120000)==WAIT_OBJECT_0){DWORD code{};GetExitCodeProcess(pi.hProcess,&code);std::printf("game_exit=%lu\n",static_cast<unsigned long>(code));result=code?5:0;}
 }while(false);
 if(WaitForSingleObject(pi.hProcess,0)==WAIT_TIMEOUT){TerminateProcess(pi.hProcess,88);WaitForSingleObject(pi.hProcess,5000);std::puts("owned game terminated after validation deadline");}CloseHandle(pi.hProcess);return result;
}
