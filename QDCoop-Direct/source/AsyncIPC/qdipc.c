/* QDCoop asynchronous shared-memory snapshots and durable event journals. No Lua or Unreal calls on worker threads.
 * Uses the bundled UE4SS e84db30c exported LuaMadeSimple constructor/accessors;
 * no game addresses, object offsets or private lua_State layout are inspected.
 * Windows x64 member-call ABI; string_view is the MSVC pointer/size return value.
 * The source class has only pointer/vector/string_view/int/optional(6 callbacks)
 * and a registry reference. 512 aligned bytes exceed its storage requirement.
 * Constructor AND destructor are called; we never forge a C++ object prefix.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "mailbox.h"
#define SLOTS 96
#define LIMIT (2*1024*1024)
#define QUEUELIMIT (4*1024*1024)
typedef struct {const char *data;size_t size;} View;
static void* (*construct)(void*,void*);
static void (*destruct)(void*);
static View* (*getstring)(void*,View*,int);
static void (*setstring)(void*,const char*,size_t);
static bool (*isstring)(void*,int);
typedef struct {char name[80];char *read,*pending,*retry;size_t rn,pn,retryN,inFlight;uint64_t rev,delivered;ULONGLONG memoryRevision;int watching,append;FILETIME stamp;DWORD size;} Slot;
static Slot slots[SLOTS];static int count,deliveryCursor;
static SRWLOCK lock=SRWLOCK_INIT;
static wchar_t root[24000];static volatile LONG started,stall;
static char *events;static size_t en;static LONGLONG eventOffset;static uint64_t eventFile;
static volatile LONG64 writes,reads,busy,failures,memoryWrites,memoryReads,cosmeticWrites,cosmeticReads,fileChecks,workerLoops,calls,batches;
static Mail *mail;static char memoryBuffer[LIMIT+1];static ULONGLONG eventRevision;
static DWORD workerTid;static volatile LONG queuedWake,forceFilePoll;
static int validname(const char *s){size_t n=strlen(s);if(!n||n>=80||strstr(s,".."))return 0;for(size_t i=0;i<n;i++)if(!((s[i]>='a'&&s[i]<='z')||(s[i]>='A'&&s[i]<='Z')||(s[i]>='0'&&s[i]<='9')||strchr("._-",s[i])))return 0;return 1;}
static int indexof(const char *s){for(int i=0;i<count;i++)if(!strcmp(slots[i].name,s))return i;if(count==SLOTS)return -1;int i=count++;strcpy(slots[i].name,s);return i;}
static void path(wchar_t *p,const char *name){wcscpy(p,root);wcscat(p,L"/");size_t at=wcslen(p);MultiByteToWideChar(CP_UTF8,0,name,-1,p+at,24000-(int)at);}
static int writefile(const char *name,const char *s,size_t n,int append){wchar_t p[24000],tmp[24000];path(p,name);wcscpy(tmp,p);if(!append)wcscat(tmp,L".qdtmp");HANDLE f=CreateFileW(tmp,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_DELETE,NULL,append?OPEN_ALWAYS:CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);if(f==INVALID_HANDLE_VALUE)return 0;LARGE_INTEGER original={0},zero={0};if(append&&!SetFilePointerEx(f,zero,&original,FILE_END)){CloseHandle(f);return 0;}DWORD done=0;BOOL ok=WriteFile(f,s,(DWORD)n,&done,NULL);if(append&&(!ok||done!=n)){SetFilePointerEx(f,original,NULL,FILE_BEGIN);SetEndOfFile(f);}CloseHandle(f);if(!ok||done!=n)return 0;return append||MoveFileExW(tmp,p,MOVEFILE_REPLACE_EXISTING);}
/* Admission counts both the worker-owned batch and newly queued bytes.
 * Retrying never allocates or merges; ownership of accepted bytes is retained. */
static int putslot(Slot*s,const char*value,size_t n,int append){
 size_t prefix=append?s->pn:0;
 if(n>QUEUELIMIT||prefix>QUEUELIMIT-n||(append&&s->inFlight>QUEUELIMIT-n-prefix))return 0;
 char*copy=malloc(n+prefix+1);if(!copy)return 0;
 if(prefix)memcpy(copy,s->pending,prefix);
 memcpy(copy+prefix,value,n);
 if(!append&&s->watching){char*cached=malloc(n+1);if(!cached){free(copy);return 0;}
  memcpy(cached,value,n);cached[n]=0;free(s->read);s->read=cached;s->rn=n;s->rev++;}
 free(s->pending);s->pending=copy;s->pn=n+prefix;s->append=append;return 1;
}
static void finishbatch(int i,char*data,size_t n,int again,int append){
 AcquireSRWLockExclusive(&lock);Slot*s=&slots[i];
 if(again&&(append||!s->pending)){s->retry=data;s->retryN=n;s->inFlight=n;data=NULL;}
 else s->inFlight=0;
 ReleaseSRWLockExclusive(&lock);free(data);
}
/* Never splice a transient event into a partially read durable line. The
 * Bridge journal uses the same complete-line rule and 64 KiB read bound. */
static size_t complete_lines(const char*data,size_t n){while(n&&data[n-1]!='\n')n--;return n;}
#ifdef QDIPC_TEST
static volatile LONG testStop;
#endif
static DWORD WINAPI worker(void *unused){(void)unused;workerTid=GetCurrentThreadId();
 ULONGLONG fileDeadline=0;
 for(;;){
#ifdef QDIPC_TEST
  if(InterlockedCompareExchange(&testStop,0,0))break;
#endif
  LONG wait=InterlockedExchange(&stall,0);if(wait>0)Sleep((DWORD)wait);
  int checkFiles=!mail||InterlockedExchange(&forceFilePoll,0)||GetTickCount64()>=fileDeadline,retry=0;if(checkFiles)fileDeadline=GetTickCount64()+250;
  InterlockedIncrement64(&workerLoops);
  int total;AcquireSRWLockShared(&lock);total=count;ReleaseSRWLockShared(&lock);
  for(int i=0;i<total;i++){
   char name[80],*pending=NULL;size_t pn=0;int watching,append;FILETIME stamp;DWORD size;ULONGLONG mr;
   AcquireSRWLockExclusive(&lock);Slot *s=&slots[i];strcpy(name,s->name);if(s->retry&&(!s->pending||s->append)){pending=s->retry;pn=s->retryN;s->retry=NULL;s->retryN=0;}
   else {free(s->retry);s->retry=NULL;s->retryN=0;pending=s->pending;pn=s->pn;s->pending=NULL;s->pn=0;}
   s->inFlight=pn;watching=s->watching;append=s->append;stamp=s->stamp;size=s->size;mr=s->memoryRevision;ReleaseSRWLockExclusive(&lock);
   int mi=mail?mailindex(name):-1;
   if(pending&&mail&&!strcmp(name,"out.cosmetic")){
    size_t consumed=0;while(consumed<pn){char*end=memchr(pending+consumed,'\n',pn-consumed);if(!end)break;size_t len=(size_t)(end-(pending+consumed))+1;if(!qdmail_push(mail,0,pending+consumed,(int)len))break;consumed+=len;InterlockedIncrement64(&cosmeticWrites);}
    if(consumed<pn){retry=1;memmove(pending,pending+consumed,pn-consumed);finishbatch(i,pending,pn-consumed,1,1);}
    else finishbatch(i,pending,0,0,1);
    pending=NULL;
   }
   if(pending){int ok=mi>=0&&!append?qdmail_write(mail,mi,pending,(int)pn):writefile(name,pending,pn,append);
    if(ok){if(mi>=0&&!append)InterlockedIncrement64(&memoryWrites);else InterlockedIncrement64(&writes);
     if(mail&&append&&!strcmp(name,"out.events"))qdmail_event(mail,0,1);
    }else{retry=1;InterlockedIncrement64(&failures);}
    finishbatch(i,pending,pn,!ok,append);
   }
   if(!watching)continue;
   if(mi>=0){int got=0;ULONGLONG written=0;
    if(qdmail_read(mail,mi,memoryBuffer,LIMIT,&mr,&got,&written)==1){char *buf=malloc((size_t)got+1);if(buf){memcpy(buf,memoryBuffer,(size_t)got);buf[got]=0;
     AcquireSRWLockExclusive(&lock);s=&slots[i];if(!s->pending){free(s->read);s->read=buf;s->rn=(size_t)got;s->rev++;s->memoryRevision=mr;buf=NULL;}ReleaseSRWLockExclusive(&lock);free(buf);InterlockedIncrement64(&memoryReads);}}
    continue;
   }
   if(!checkFiles)continue;
   wchar_t p[24000];path(p,name);WIN32_FILE_ATTRIBUTE_DATA attr;InterlockedIncrement64(&fileChecks);if(!GetFileAttributesExW(p,GetFileExInfoStandard,&attr)||attr.nFileSizeHigh||attr.nFileSizeLow>LIMIT)continue;
   if(CompareFileTime(&stamp,&attr.ftLastWriteTime)==0&&size==attr.nFileSizeLow)continue;
   HANDLE f=CreateFileW(p,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(f==INVALID_HANDLE_VALUE)continue;
   char *buf=malloc(attr.nFileSizeLow+1);DWORD got=0;BOOL ok=buf&&ReadFile(f,buf,attr.nFileSizeLow,&got,NULL);CloseHandle(f);if(!ok){free(buf);continue;}buf[got]=0;
   AcquireSRWLockExclusive(&lock);s=&slots[i];if(!s->pending){free(s->read);s->read=buf;s->rn=got;s->rev++;s->stamp=attr.ftLastWriteTime;s->size=attr.nFileSizeLow;buf=NULL;}ReleaseSRWLockExclusive(&lock);free(buf);InterlockedIncrement64(&reads);
  }
  /* The event journal is read incrementally, never reread as it grows. */
  size_t room;AcquireSRWLockShared(&lock);room=QUEUELIMIT-en;ReleaseSRWLockShared(&lock);
  ULONGLONG er=mail?qdmail_event(mail,1,0):0;
  if(room>=65536&&(!mail||er!=eventRevision)){wchar_t p[24000];path(p,"in.events");HANDLE f=CreateFileW(p,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(f!=INVALID_HANDLE_VALUE){BY_HANDLE_FILE_INFORMATION info;if(GetFileInformationByHandle(f,&info)){uint64_t id=((uint64_t)info.nFileIndexHigh<<32)|info.nFileIndexLow;if(id!=eventFile){eventFile=id;eventOffset=0;}}LARGE_INTEGER size,pos;if(GetFileSizeEx(f,&size)){if(size.QuadPart<eventOffset)eventOffset=0;pos.QuadPart=eventOffset;SetFilePointerEx(f,pos,NULL,FILE_BEGIN);char buf[65536];DWORD got=0;if(ReadFile(f,buf,sizeof(buf),&got,NULL)&&got&&(got=(DWORD)complete_lines(buf,got))>0){AcquireSRWLockExclusive(&lock);char *merge=realloc(events,en+got);if(merge){events=merge;memcpy(events+en,buf,got);en+=got;eventOffset+=got;}ReleaseSRWLockExclusive(&lock);}if(eventOffset>=size.QuadPart)eventRevision=er;}CloseHandle(f);}}
  if(mail&&qdmail_pending(mail,1)){AcquireSRWLockExclusive(&lock);room=QUEUELIMIT-en;if(room>=65536){char*merge=realloc(events,en+65536);if(merge){events=merge;int got=qdmail_drain(mail,1,events+en,65536,64);en+=(size_t)got;if(got)InterlockedAdd64(&cosmeticReads,got);}}ReleaseSRWLockExclusive(&lock);}
  if(mail){
   AcquireSRWLockShared(&lock);room=QUEUELIMIT-en;ReleaseSRWLockShared(&lock);
   if(room>=65536&&(qdmail_pending(mail,1)||qdmail_event(mail,1,0)!=eventRevision))retry=1;
   ULONGLONG now=GetTickCount64();DWORD timeout=fileDeadline>now?(DWORD)(fileDeadline-now):0;if(retry&&timeout>8)timeout=8;WaitForSingleObject(qdmail_ready(mail,1),timeout);
  }else Sleep(8);
 }return 0;}
static int bind(void){static int bound;if(bound)return 1;HMODULE h=GetModuleHandleW(L"UE4SS.dll");if(!h)return 0;
#define API(v,n) {FARPROC proc=GetProcAddress(h,n);memcpy(&v,&proc,sizeof(proc));if(!v)return 0;}
 API(construct,"??0Lua@LuaMadeSimple@RC@@QEAA@PEAUlua_State@@@Z");
 API(destruct,"??1Lua@LuaMadeSimple@RC@@QEAA@XZ");
 API(getstring,"?get_string@Lua@LuaMadeSimple@RC@@QEBA?AV?$basic_string_view@DU?$char_traits@D@std@@@std@@H@Z");
 API(setstring,"?set_string@Lua@LuaMadeSimple@RC@@QEBAXPEBD_K@Z");
 API(isstring,"?is_string@Lua@LuaMadeSimple@RC@@QEBA_NH@Z");
 bound=1;return 1;
}
__declspec(dllexport) int qdipc(void *L){if(!bind())return 0;InterlockedIncrement64(&calls);__attribute__((aligned(16))) unsigned char obj[512];memset(obj,0,sizeof(obj));construct(obj,L);char *result=NULL;size_t rn=0;const char *fixed="0";View arg={0};int newWatch=0;char small[128];
 if(!isstring(obj,1))goto done;
 getstring(obj,&arg,1);if(!arg.data||arg.size>LIMIT+100)goto done;char *cmd=arg.size<sizeof(small)?small:malloc(arg.size+1);if(!cmd)goto done;memcpy(cmd,arg.data,arg.size);cmd[arg.size]=0;
 if(cmd[0]=='S'){char *channel=strchr(cmd+1,'\n');if(channel)*channel++=0;
  if(!started&&strlen(cmd+1)<23000){
   if(channel&&*channel){wchar_t name[128];if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,channel,-1,name,128)||(mail=qdmail_open(name))==NULL){if(cmd!=small)free(cmd);goto done;}}
if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,cmd+1,-1,root,24000)){
  /* Lua releases package.loadlib handles when its VM closes. Keep worker code
     mapped until the process exits; the worker never calls a destroyed VM. */
  HMODULE self;if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCWSTR)(uintptr_t)&worker,&self)){
   HANDLE t=CreateThread(NULL,0,worker,NULL,0,NULL);if(t){CloseHandle(t);InterlockedExchange(&started,1);}
  }
 }}fixed=started?"1":"0";}
 else if(cmd[0]=='T'){char b[512];snprintf(b,sizeof(b),"writes=%llu reads=%llu busy=%llu failures=%llu mode=%s workerTid=%lu memoryWrites=%llu memoryReads=%llu cosmeticWrites=%llu cosmeticReadBytes=%llu fileChecks=%llu workerLoops=%llu",(unsigned long long)InterlockedCompareExchange64(&writes,0,0),(unsigned long long)InterlockedCompareExchange64(&reads,0,0),(unsigned long long)InterlockedCompareExchange64(&busy,0,0),(unsigned long long)InterlockedCompareExchange64(&failures,0,0),mail?"memory":"file",workerTid,(unsigned long long)InterlockedCompareExchange64(&memoryWrites,0,0),(unsigned long long)InterlockedCompareExchange64(&memoryReads,0,0),(unsigned long long)InterlockedCompareExchange64(&cosmeticWrites,0,0),(unsigned long long)InterlockedCompareExchange64(&cosmeticReads,0,0),(unsigned long long)InterlockedCompareExchange64(&fileChecks,0,0),(unsigned long long)InterlockedCompareExchange64(&workerLoops,0,0));size_t used=strlen(b);snprintf(b+used,sizeof(b)-used," calls=%llu batches=%llu",(unsigned long long)InterlockedCompareExchange64(&calls,0,0),(unsigned long long)InterlockedCompareExchange64(&batches,0,0));result=_strdup(b);rn=strlen(b);}
#ifdef QDIPC_TEST
 else if(cmd[0]=='Z'){InterlockedExchange(&stall,atoi(cmd+1));fixed="1";}
#endif
 else if(started&&TryAcquireSRWLockExclusive(&lock)){
  if(cmd[0]=='B'){
   int selected[SLOTS],ns=0;size_t bytes=0;fixed="=";
   for(int k=0;k<count;k++){int i=(deliveryCursor+k)%count;Slot*s=&slots[i];if(!s->watching||s->rev==s->delivered)continue;char header[112];int n=snprintf(header,sizeof(header),"%s\n%zu\n",s->name,s->rn);if(bytes&&bytes+(size_t)n+s->rn>LIMIT){deliveryCursor=i;break;}selected[ns++]=i;bytes+=(size_t)n+s->rn;}
   if(bytes&&(result=malloc(bytes))!=NULL){for(int k=0;k<ns;k++){Slot*s=&slots[selected[k]];char header[112];int n=snprintf(header,sizeof(header),"%s\n%zu\n",s->name,s->rn);memcpy(result+rn,header,(size_t)n);rn+=(size_t)n;if(s->rn)memcpy(result+rn,s->read,s->rn);rn+=s->rn;s->delivered=s->rev;}InterlockedIncrement64(&batches);}
  }
  else if(cmd[0]=='E'){if(en<=65536){result=events;rn=en;events=NULL;en=0;}else{result=malloc(65536);if(result){rn=65536;memcpy(result,events,rn);memmove(events,events+rn,en-rn);en-=rn;}}fixed="";}
  else {char *name=cmd+1,*value=strchr(name,'\n');if(value)*value++=0;if(validname(name)){int i=indexof(name);if(i>=0){Slot *s=&slots[i];
   if(cmd[0]=='R'){newWatch=!s->watching;s->watching=1;fixed="=";if(s->delivered!=s->rev){result=malloc(s->rn+1);if(result){result[0]='1';if(s->rn)memcpy(result+1,s->read,s->rn);rn=s->rn+1;s->delivered=s->rev;}}}
   /* Menu reset of an inbound snapshot is local only. The bridge remains
      the sole shared-slot writer, even during room switches. */
   else if(value&&cmd[0]=='W'&&mail&&mailindex(name)>0&&((mailindex(name)&1)||mailindex(name)==10)){
    size_t n=arg.size-(size_t)(value-cmd);char *copy=malloc(n+1);if(copy){memcpy(copy,value,n);copy[n]=0;
     int mi=mailindex(name);s->memoryRevision=(ULONGLONG)InterlockedCompareExchange64(&mail->data->slot[mi].revision,0,0);
     free(s->read);s->read=copy;s->rn=n;s->rev++;fixed="1";}
   }
   else if(value&&(cmd[0]=='W'||cmd[0]=='A')){size_t n=arg.size-(size_t)(value-cmd);if(putslot(s,value,n,cmd[0]=='A'))fixed="1";}
  }}}
  ReleaseSRWLockExclusive(&lock);
  if(mail){
   if(cmd[0]=='W'||cmd[0]=='A')InterlockedExchange(&queuedWake,1);
   if(newWatch)InterlockedExchange(&forceFilePoll,1);
   if(newWatch||(cmd[0]=='E'&&rn))SetEvent(qdmail_ready(mail,1));
  }
 }else {InterlockedIncrement64(&busy);fixed=cmd[0]=='R'?"0":"=";}
 if(mail&&cmd[0]=='B'&&InterlockedExchange(&queuedWake,0))SetEvent(qdmail_ready(mail,1));
 if(cmd!=small)free(cmd);
done: setstring(obj,result?result:fixed,result?rn:strlen(fixed));free(result);destruct(obj);return 1;}
