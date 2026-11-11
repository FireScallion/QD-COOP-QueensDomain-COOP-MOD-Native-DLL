/* Game-thread weak identity checks and batched public socket reads.
 * No UObject is read by a worker. Object metadata stays in UE, while opaque
 * FWeakObjectPtr handles retain serial identity without keeping objects alive.
 * The packed return is an owned Lua string, not an engine-memory reference.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#define CAP 4096
#define GROUPS 32
#define BONES 140
typedef struct{const char*data;size_t size;} View;
typedef union{uint64_t align[32];unsigned char bytes[256];} Weak;
typedef struct{Weak weak;uint64_t token;int used;} Handle;
typedef struct{int count;unsigned char names[BONES][8];} Group;
static void*(*construct)(void*,void*);static void(*destruct)(void*);
static View*(*getstring)(void*,View*,int);static void(*setstring)(void*,const char*,size_t);
static bool(*isstring)(void*,int),(*isnumber)(void*,int);static int64_t(*getinteger)(void*,int);static int(*stacksize)(void*);
static void(*setinteger)(void*,int64_t);static void(*event)(void*,void*,void*);static void*(*fname)(void*,uint32_t,uint32_t);
static int*(*offset)(void*);static int*(*size)(void*);static uint16_t*(*parms)(void*);static bool(*isa)(void*,void*);
static void*(*weakctor)(void*,const void*);static void*(*weakget)(const void*);
static bool(*hashAvailable)(void);static void*(*findPath)(void*,const wchar_t*,bool,bool);
static void(*pushObject)(const void*,void*);
static Handle handles[CAP];static void*objectKeys[CAP];static uint64_t serial;static Group groups[GROUPS];static int groupCount;
static DWORD gameThread;static int bound,ready,nameAt,spaceAt,returnAt;static void *socketFunction,*sceneClass;
static int bind(void){
 if(bound)return 1;
 HMODULE h=GetModuleHandleW(L"UE4SS.dll");if(!h)return 0;
#define API(v,n) {FARPROC p=GetProcAddress(h,n);memcpy(&v,&p,sizeof(p));if(!v)return 0;}
 API(construct,"??0Lua@LuaMadeSimple@RC@@QEAA@PEAUlua_State@@@Z");API(destruct,"??1Lua@LuaMadeSimple@RC@@QEAA@XZ");
 API(getstring,"?get_string@Lua@LuaMadeSimple@RC@@QEBA?AV?$basic_string_view@DU?$char_traits@D@std@@@std@@H@Z");
 API(setstring,"?set_string@Lua@LuaMadeSimple@RC@@QEBAXPEBD_K@Z");API(isstring,"?is_string@Lua@LuaMadeSimple@RC@@QEBA_NH@Z");
 API(isnumber,"?is_number@Lua@LuaMadeSimple@RC@@QEBA_NH@Z");API(getinteger,"?get_integer@Lua@LuaMadeSimple@RC@@QEBA_JH@Z");API(stacksize,"?get_stack_size@Lua@LuaMadeSimple@RC@@QEBAHXZ");
 API(setinteger,"?set_integer@Lua@LuaMadeSimple@RC@@QEBAX_J@Z");API(event,"?ProcessEvent@UObject@Unreal@RC@@QEAAXPEAVUFunction@23@PEAX@Z");
 API(fname,"??0FName@Unreal@RC@@QEAA@II@Z");API(offset,"?GetOffset_Internal@FProperty@Unreal@RC@@QEAAAEAHXZ");API(size,"?GetElementSize@FProperty@Unreal@RC@@QEAAAEAHXZ");API(parms,"?GetParmsSize@UFunction@Unreal@RC@@QEAAAEAGXZ");
 API(isa,"?IsA@UObjectBase@Unreal@RC@@QEBA_NPEAVUClass@23@@Z");
 API(weakctor,"??0FWeakObjectPtr@Unreal@RC@@QEAA@PEBVUObject@12@@Z");API(weakget,"?Get@FWeakObjectPtr@Unreal@RC@@QEBAPEAVUObject@23@XZ");
 API(hashAvailable,"?ShouldUseHashTables@UnrealInitializer@Unreal@RC@@YA_NXZ");
 API(findPath,"?StaticFindObjectByPath@FUObjectHashTables@Unreal@RC@@SAPEAVUObject@23@PEAVUClass@23@PEB_W_N2@Z");
 API(pushObject,"?auto_construct_object@LuaType@RC@@YAXAEBVLua@LuaMadeSimple@2@PEAVUObject@Unreal@2@@Z");
 bound=1;return 1;
}
static int number(char**p,uint64_t*v){char*e;*v=strtoull(*p,&e,10);if(e==*p)return 0;*p=e;return 1;}
static int property(void*p,int bytes,int ownerSize){return p&&*size(p)==bytes&&*offset(p)>=0&&*offset(p)+bytes<=ownerSize;}
static Handle*lookup(uint64_t token){if(!token)return NULL;Handle*h=&handles[(token-1)%CAP];return h->used&&h->token==token?h:NULL;}
static int makeweak(Weak*w,void*object){memset(w,0xcc,sizeof(*w));weakctor(w,object);for(int i=64;i<256;i++)if(w->bytes[i]!=0xcc)return 0;return weakget(w)==object;}
__declspec(dllexport) int qdread(void*L){
 if(!bind())return 0;
 __attribute__((aligned(16))) unsigned char lua[512]={0};construct(lua,L);View v={0};int64_t result=-1;
 if(!isstring(lua,1))goto done;
 getstring(lua,&v,1);if(!v.data||!v.size||v.size>=8192)goto done;
 char command[8192];memcpy(command,v.data,v.size);command[v.size]=0;char*p=command+1;
 if(command[0]=='T'){setstring(lua,"A\0B",3);goto finish;}
 if(command[0]=='U'){setstring(lua,"weak-inline-64",14);goto finish;}
 if(command[0]=='H'){result=hashAvailable()?1:0;goto done;}
 if(command[0]=='Q'){
  /* Separate from weak registration: startup lookups must not bind the weak
   * validator to the mod loader thread. UE4SS's hash entry locks its table.
   * IMPORTANT: ByPath can itself enumerate if the hash backend is unavailable.
   * Guard with the SAME availability/configuration predicate as FindObject.
   * Never call UObjectGlobals::FindObject after a miss. No negative cache. */
  if(!hashAvailable())goto done;
  size_t length=v.size-1;wchar_t path[8192];
  if(!length||memchr(p,0,length))goto done;
  int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,p,(int)length,path,8191);
  if(count<=0)goto done;
  path[count]=0;
  void*object=findPath(NULL,path,false,true);
  if(!object){result=0;goto done;}
  pushObject(lua,object);goto finish;
 }
 if(command[0]=='I'){
  uint64_t a[8];for(int i=0;i<8;i++)if(!number(&p,&a[i]))goto done;if(*p)goto done;
  void*fn=(void*)(uintptr_t)a[0],*cls=(void*)(uintptr_t)a[1];if(!fn||!cls)goto done;
  int bytes=*parms(fn);if(bytes<48||bytes>256)goto done;
  int sizes[3]={8,1,48};for(int i=0;i<3;i++)if(!property((void*)(uintptr_t)a[i+2],sizes[i],bytes))goto done;
  int expectedAt[3]={0,16,32},expectedSize[3]={16,12,12};for(int i=0;i<3;i++){void*f=(void*)(uintptr_t)a[5+i];if(!property(f,expectedSize[i],48)||*offset(f)!=expectedAt[i])goto done;}
  socketFunction=fn;sceneClass=cls;nameAt=*offset((void*)(uintptr_t)a[2]);spaceAt=*offset((void*)(uintptr_t)a[3]);returnAt=*offset((void*)(uintptr_t)a[4]);ready=1;result=1;
 }else if(command[0]=='G'){
  uint64_t n,index,num;if(!number(&p,&n)||n<1||n>BONES)goto done;Group next={0};next.count=(int)n;
  for(int i=0;i<(int)n;i++){if(!number(&p,&index)||!number(&p,&num)||index>UINT32_MAX||num>UINT32_MAX)goto done;fname(next.names[i],(uint32_t)index,(uint32_t)num);}if(*p)goto done;
  for(int i=0;i<groupCount;i++)if(memcmp(&groups[i],&next,sizeof(next))==0){result=i+1;goto done;}
  if(groupCount==GROUPS)goto done;
  groups[groupCount]=next;result=++groupCount;
 }else{
  if(!gameThread)gameThread=GetCurrentThreadId();
  if(gameThread!=GetCurrentThreadId())goto done;
  if(command[0]=='W'){
   if(stacksize(lua)!=1||!isnumber(lua,1))goto done;
   void*object=(void*)(uintptr_t)getinteger(lua,1);if(!object)goto done;
   Weak weak;if(!makeweak(&weak,object))goto done;
   /* Public UE4 FWeakObjectPtr is index + serial. Verify the ABI rather than
    * returning an engine pointer or retaining an entry in the fixed pool.
    * Lua owns this 64-bit integer identity. The wrapper's string reader is
    * not binary safe, so never pass a NUL-containing token back through it. */
   for(int i=8;i<64;i++)if(weak.bytes[i]!=0xcc)goto done;
   uint64_t identity;memcpy(&identity,weak.bytes,8);if(!identity||identity>INT64_MAX)goto done;result=(int64_t)identity;
  }else if(command[0]=='L'){
   if(stacksize(lua)!=1||!isnumber(lua,1))goto done;
   int64_t identity=getinteger(lua,1);if(identity<=0)goto done;
   Weak weak={0};memcpy(weak.bytes,&identity,8);result=weakget(&weak)?1:0;
  }else if(command[0]=='R'){
   if(stacksize(lua)!=1||!isnumber(lua,1))goto done;
   void*object=(void*)(uintptr_t)getinteger(lua,1);if(!object)goto done;
   /* UE4SS may create another wrapper for the same component on each property
    * access. Share its serial-checked handle instead of exhausting the pool. */
   for(int i=0;i<CAP;i++)if(objectKeys[i]==object&&handles[i].used&&weakget(&handles[i].weak)==object){result=(int64_t)handles[i].token;goto done;}
   for(int i=0;i<CAP;i++){uint64_t token=++serial;Handle*h=&handles[(token-1)%CAP];if(h->used&&weakget(&h->weak))continue;
    if(!makeweak(&h->weak,object))goto done;
    h->used=1;h->token=token;objectKeys[(token-1)%CAP]=object;result=(int64_t)token;goto done;
   }
  }else if(command[0]=='V'||command[0]=='F'){
   if(stacksize(lua)!=1||!isnumber(lua,1))goto done;
   Handle*h=lookup((uint64_t)getinteger(lua,1));if(!h){result=0;goto done;}
   if(command[0]=='F'){h->used=0;result=1;}else{result=weakget(&h->weak)?1:0;if(!result)h->used=0;}
  }else if(command[0]=='S'&&ready){
   if(stacksize(lua)!=2||!isnumber(lua,1)||!isnumber(lua,2))goto done;
   uint64_t group=(uint64_t)getinteger(lua,2);Handle*h=lookup((uint64_t)getinteger(lua,1));if(!h||group<1||group>(uint64_t)groupCount){result=0;goto done;}
   void*object=weakget(&h->weak);if(!object){h->used=0;result=0;goto done;}if(!isa(object,sceneClass))goto done;
   float values[BONES][10];Group*g=&groups[group-1];
   for(int i=0;i<g->count;i++){
    __attribute__((aligned(16))) unsigned char args[256]={0};memcpy(args+nameAt,g->names[i],8);args[spaceAt]=2;event(object,socketFunction,args);
    memcpy(values[i],args+returnAt+16,12);memcpy(values[i]+3,args+returnAt,16);memcpy(values[i]+7,args+returnAt+32,12);
    for(int k=0;k<10;k++)if(!isfinite(values[i][k]))goto done;
   }
   setstring(lua,(const char*)values,(size_t)g->count*40);goto finish;
  }
 }
done:setinteger(lua,result);
finish:destruct(lua);return 1;
}
