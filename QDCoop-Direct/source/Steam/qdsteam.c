/* Game-side overlay/callback adapter. Initialization runs once on a worker.
 * Only short callback dispatch and explicit overlay commands run on game thread.
 * QDCoop's separate broker owns transport and room operations. x64 Windows only. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
typedef struct {const char *data;size_t size;} View;
static void* (*construct)(void*,void*);
static void (*destruct)(void*);
static View* (*getstring)(void*,View*,int);
static void (*setstring)(void*,const char*,size_t);
static bool (*isstring)(void*,int);
static void *friends,*utils;
static void (*overlay)(void*,const char*);
static void (*invite)(void*,uint64_t);
static bool (*enabled)(void*);
static LONG64 pending;
static bool startRead;
static LONG ready,started;
static wchar_t sdkPath[9000];
static const char *initError="initializing";
static void (*pump)(void);
typedef struct {void **vtable;uint8_t flags;uint8_t pad[3];int id;} Callback;
_Static_assert(offsetof(Callback,id)==12,"Steam callback ABI");
static bool lobbyid(uint64_t id){return id && ((id>>52)&15)==8;}
static uint64_t parse(const char *s){
 if(!s || !*s)return 0;
 uint64_t n=0;int count=0;
 while(*s>='0'&&*s<='9'){unsigned d=(unsigned)(*s++-'0');if(++count>20||n>(UINT64_MAX-d)/10)return 0;n=n*10+d;}
 while(*s==' '||*s=='\t')s++;
 return !*s&&lobbyid(n)?n:0;
}
static void run(Callback *self,void *data){
 uint64_t id=0;if(!data)return;
 if(self->id==333)memcpy(&id,data,8);
 else if(self->id==337){char text[257];memcpy(text,(char*)data+8,256);text[256]=0;
  if(!strncmp(text,"+qdcoop_lobby ",14))id=parse(text+14);
 }
 if(lobbyid(id))InterlockedExchange64(&pending,(LONG64)id);
}
/* Both Run overloads share this adapter; extra x64 arguments are ignored. */
static int size(Callback *self){return self->id==333?16:264;}
static void *vtable[3]={(void*)run,(void*)run,(void*)size};
static Callback lobbyCallback={vtable,0,{0},333},presenceCallback={vtable,0,{0},337};
#define LOAD(h,v,n) do{FARPROC p=GetProcAddress(h,n);memcpy(&v,&p,sizeof(p));if(!v){initError=n;return false;}}while(0)
static bool bindLua(void){HMODULE h=GetModuleHandleW(L"UE4SS.dll");if(!h)return false;
 LOAD(h,construct,"??0Lua@LuaMadeSimple@RC@@QEAA@PEAUlua_State@@@Z");
 LOAD(h,destruct,"??1Lua@LuaMadeSimple@RC@@QEAA@XZ");
 LOAD(h,getstring,"?get_string@Lua@LuaMadeSimple@RC@@QEBA?AV?$basic_string_view@DU?$char_traits@D@std@@@std@@H@Z");
 LOAD(h,setstring,"?set_string@Lua@LuaMadeSimple@RC@@QEBAXPEBD_K@Z");
 LOAD(h,isstring,"?is_string@Lua@LuaMadeSimple@RC@@QEBA_NH@Z");return true;
}
static bool init(void){
 HMODULE h=GetModuleHandleW(L"steam_api64.dll");
 if(!h)h=LoadLibraryExW(sdkPath,NULL,LOAD_WITH_ALTERED_SEARCH_PATH);
 if(!h){initError="Steam API could not be loaded";return false;}
 int (*pipe)(void);void*(*getFriends)(void)=NULL;void*(*getUtils)(void);uint32_t(*appid)(void*);void(*reg)(void*,int);
 LOAD(h,pipe,"SteamAPI_GetHSteamPipe");
 if(!pipe()){
  int (*initialize)(char*);LOAD(h,initialize,"SteamAPI_InitFlat");char error[1024]={0};
  SetEnvironmentVariableW(L"SteamAppId",L"1799840");
  if(initialize(error)!=0){initError="Steam API initialization failed";return false;}
 }
 LOAD(h,pump,"SteamAPI_RunCallbacks");
 for(int v=20;v>=17;v--){char name[64];snprintf(name,sizeof(name),"SteamAPI_SteamFriends_v%03d",v);FARPROC p=GetProcAddress(h,name);if(p){memcpy(&getFriends,&p,sizeof(p));break;}}
 if(!getFriends){initError="Steam friends interface missing";return false;}
 LOAD(h,getUtils,"SteamAPI_SteamUtils_v010");
 LOAD(h,appid,"SteamAPI_ISteamUtils_GetAppID");LOAD(h,reg,"SteamAPI_RegisterCallback");
 LOAD(h,overlay,"SteamAPI_ISteamFriends_ActivateGameOverlay");LOAD(h,invite,"SteamAPI_ISteamFriends_ActivateGameOverlayInviteDialog");LOAD(h,enabled,"SteamAPI_ISteamUtils_IsOverlayEnabled");
 friends=getFriends();utils=getUtils();if(!friends||!utils||appid(utils)!=1799840)return false;
 // Steam retains callback pointers. Keep our tiny module mapped for process life.
 HMODULE pinned=NULL;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCWSTR)(uintptr_t)&init,&pinned))return false;
 reg(&lobbyCallback,333);reg(&presenceCallback,337);return true;
}
static DWORD WINAPI worker(void *unused){(void)unused;bool ok=init();if(!ok&&strcmp(initError,"initializing")==0)initError="Steam interface unavailable";InterlockedExchange(&ready,ok?1:-1);return 0;}
static void launchRequest(void){
 if(startRead)return;
 startRead=true;
 const wchar_t *cmd=GetCommandLineW(),*p=wcsstr(cmd,L"+qdcoop_lobby ");int len=14;
 if(!p){p=wcsstr(cmd,L"+connect_lobby ");len=15;}
 if(p){p+=len;char b[32];int n=0;while(*p>=L'0'&&*p<=L'9'&&n<30)b[n++]=(char)*p++;b[n]=0;
  uint64_t id=parse(b);if(id)InterlockedExchange64(&pending,(LONG64)id);
 }
}
__declspec(dllexport) int qdsteam(void *L){
 if(!bindLua())return 0;
 __attribute__((aligned(16))) unsigned char obj[512]={0};construct(obj,L);
 char out[128]="unavailable",cmd[12000]={0};View arg={0};
 if(!isstring(obj,1))goto done;
 getstring(obj,&arg,1);if(!arg.data||arg.size==0||arg.size>=sizeof(cmd))goto done;memcpy(cmd,arg.data,arg.size);
 launchRequest();
 if(cmd[0]=='S'&&InterlockedCompareExchange(&started,1,0)==0){
  int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,cmd+1,-1,sdkPath,9000);
  HMODULE pinned=NULL;
  if(n<=0||!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCWSTR)(uintptr_t)&worker,&pinned)){InterlockedExchange(&ready,-1);goto done;}
  HANDLE thread=CreateThread(NULL,0,worker,NULL,0,NULL);if(thread)CloseHandle(thread);else InterlockedExchange(&ready,-1);
 }
 LONG initialized=InterlockedCompareExchange(&ready,0,0);
 if(initialized!=1){snprintf(out,sizeof(out),"%s",initialized==0?"initializing":initError);goto done;}
 // Regular nonblocking dispatch guarantees title-screen joins even if the
 // original game initialized Steam without ticking its online subsystem.
 if(cmd[0]=='Q')pump();
 if(cmd[0]=='Q'){uint64_t id=(uint64_t)InterlockedExchange64(&pending,0);if(id)snprintf(out,sizeof(out),"join|%llu",(unsigned long long)id);else strcpy(out,enabled(utils)?"ready":"overlay_disabled");}
 else if(cmd[0]=='F'){if(enabled(utils)){overlay(friends,"friends");strcpy(out,"ok");}else strcpy(out,"overlay_disabled");}
 else if(cmd[0]=='I'){uint64_t id=parse(cmd+1);if(!id)strcpy(out,"invalid_lobby");else if(enabled(utils)){invite(friends,id);strcpy(out,"ok");}else strcpy(out,"overlay_disabled");}
 else strcpy(out,enabled(utils)?"ready":"overlay_disabled");
done:setstring(obj,out,strlen(out));destruct(obj);return 1;
}
