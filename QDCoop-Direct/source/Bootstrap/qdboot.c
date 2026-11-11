/* Start the session supervisor without a console or a shell. No worker touches Lua. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
typedef struct {const char *data;size_t size;} View;
static void* (*construct)(void*,void*);
static void (*destruct)(void*);
static View* (*getstring)(void*,View*,int);
static void (*setstring)(void*,const char*,size_t);
static bool (*isstring)(void*,int);
static int bind(void){HMODULE h=GetModuleHandleW(L"UE4SS.dll");if(!h)return 0;
#define API(v,n) {FARPROC p=GetProcAddress(h,n);memcpy(&v,&p,sizeof(p));if(!v)return 0;}
API(construct,"??0Lua@LuaMadeSimple@RC@@QEAA@PEAUlua_State@@@Z");
API(destruct,"??1Lua@LuaMadeSimple@RC@@QEAA@XZ");
API(getstring,"?get_string@Lua@LuaMadeSimple@RC@@QEBA?AV?$basic_string_view@DU?$char_traits@D@std@@@std@@H@Z");
API(setstring,"?set_string@Lua@LuaMadeSimple@RC@@QEBAXPEBD_K@Z");
API(isstring,"?is_string@Lua@LuaMadeSimple@RC@@QEBA_NH@Z");return 1;}
__declspec(dllexport) int qdboot(void *L){
 if(!bind())return 0;
 __attribute__((aligned(16))) unsigned char obj[512]={0};construct(obj,L);
 char result[100]="ERROR: bootstrap failed";View arg={0};
 if(!isstring(obj,1))goto done;
 getstring(obj,&arg,1);if(!arg.data||arg.size==0||arg.size>8000)goto done;
 wchar_t saved[9000],script[9000],exe[1024],cmd[20000];
 int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,arg.data,(int)arg.size,saved,8999);
 if(n<=0)goto done;
 saved[n]=0;if(wcschr(saved,L'"'))goto done;
 HMODULE self=NULL;
 if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCWSTR)(uintptr_t)&qdboot,&self))goto done;
 DWORD len=GetModuleFileNameW(self,script,9000);if(!len||len>=8900)goto done;
 wchar_t *last=wcsrchr(script,L'\\');if(!last)goto done;wcscpy(last+1,L"Native-Session.ps1");
 if(!GetSystemDirectoryW(exe,900))goto done;
 wcscat(exe,L"\\WindowsPowerShell\\v1.0\\powershell.exe");
 FILETIME created,ended,kernel,user;if(!GetProcessTimes(GetCurrentProcess(),&created,&ended,&kernel,&user))goto done;
 ULARGE_INTEGER stamp;stamp.LowPart=created.dwLowDateTime;stamp.HighPart=created.dwHighDateTime;
 swprintf(cmd,20000,L"\"%ls\" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \"%ls\" -OwnerPid %lu -OwnerFileTime %llu -SaveDirectory \"%ls\"",exe,script,GetCurrentProcessId(),(unsigned long long)stamp.QuadPart,saved);
 STARTUPINFOW si={0};si.cb=sizeof(si);si.dwFlags=STARTF_USESHOWWINDOW;si.wShowWindow=SW_HIDE;PROCESS_INFORMATION pi={0};
 if(!CreateProcessW(exe,cmd,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&si,&pi)){snprintf(result,sizeof(result),"ERROR: CreateProcess %lu",GetLastError());goto done;}
 CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
 snprintf(result,sizeof(result),"%lu_%llu",GetCurrentProcessId(),(unsigned long long)stamp.QuadPart);
done:setstring(obj,result,strlen(result));destruct(obj);return 1;
}
