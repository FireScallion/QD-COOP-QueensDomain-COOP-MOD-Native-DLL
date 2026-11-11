/* Game-thread state gather through the bundled UE4SS exported API.
 * No private game offsets: FProperty supplies offsets, sizes and bool access.
 * Retained objects use UE FWeakObjectPtr construction/resolution, not raw-pointer
 * validity caches. Weak handles do not keep streamed actors or meshes alive.
 * Engine getters and full transform scale match the original Lua path.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#define MAX_HANDLES 4096
typedef struct {const char*data;size_t size;} View;
typedef union {uint64_t alignment[32];unsigned char bytes[256];} WeakStorage;
typedef struct {WeakStorage actor,mesh;uint64_t token;int used;} Handle;
typedef struct {void*fn,*ret;int bytes,at;} Getter;
static void*(*construct)(void*,void*);static void(*destruct)(void*);static View*(*getstring)(void*,View*,int);
static bool(*isstring)(void*,int),(*isnumber)(void*,int);static int64_t(*getinteger)(void*,int);static int(*stacksize)(void*);
static void(*setstring)(void*,const char*,size_t),(*setnumber)(void*,double),(*setinteger)(void*,int64_t);
static void(*event)(void*,void*,void*);static int*(*offset)(void*);static int*(*elementSize)(void*);static int*(*propertiesSize)(void*);static uint16_t*(*parms)(void*);
static bool(*isa)(void*,void*),(*getbool)(void*,const void*,int);static void*(*weakctor)(void*,const void*),*(*weakget)(const void*);
static int bound,ready,hpAt,meshAt;static DWORD gameThread;
static void *baseClass,*meshClass,*boolProps[4];static WeakStorage baseWeak;static Getter getters[6];
static Handle handles[MAX_HANDLES];static uint64_t serial;
static int bind(void){if(bound)return 1;HMODULE h=GetModuleHandleW(L"UE4SS.dll");if(!h)return 0;
#define API(v,n) {FARPROC p=GetProcAddress(h,n);memcpy(&v,&p,sizeof(p));if(!v)return 0;}
 API(construct,"??0Lua@LuaMadeSimple@RC@@QEAA@PEAUlua_State@@@Z");API(destruct,"??1Lua@LuaMadeSimple@RC@@QEAA@XZ");
 API(getstring,"?get_string@Lua@LuaMadeSimple@RC@@QEBA?AV?$basic_string_view@DU?$char_traits@D@std@@@std@@H@Z");
 API(isstring,"?is_string@Lua@LuaMadeSimple@RC@@QEBA_NH@Z");API(isnumber,"?is_number@Lua@LuaMadeSimple@RC@@QEBA_NH@Z");
 API(getinteger,"?get_integer@Lua@LuaMadeSimple@RC@@QEBA_JH@Z");API(stacksize,"?get_stack_size@Lua@LuaMadeSimple@RC@@QEBAHXZ");
 API(setstring,"?set_string@Lua@LuaMadeSimple@RC@@QEBAXPEBD_K@Z");API(setnumber,"?set_number@Lua@LuaMadeSimple@RC@@QEBAXN@Z");API(setinteger,"?set_integer@Lua@LuaMadeSimple@RC@@QEBAX_J@Z");
 API(event,"?ProcessEvent@UObject@Unreal@RC@@QEAAXPEAVUFunction@23@PEAX@Z");API(isa,"?IsA@UObjectBase@Unreal@RC@@QEBA_NPEAVUClass@23@@Z");
 API(offset,"?GetOffset_Internal@FProperty@Unreal@RC@@QEAAAEAHXZ");API(elementSize,"?GetElementSize@FProperty@Unreal@RC@@QEAAAEAHXZ");API(propertiesSize,"?GetPropertiesSize@UStruct@Unreal@RC@@QEAAAEAHXZ");API(parms,"?GetParmsSize@UFunction@Unreal@RC@@QEAAAEAGXZ");
 API(getbool,"?GetPropertyValueInContainer@FBoolProperty@Unreal@RC@@QEAA_NPEBXH@Z");
 API(weakctor,"??0FWeakObjectPtr@Unreal@RC@@QEAA@PEBVUObject@12@@Z");API(weakget,"?Get@FWeakObjectPtr@Unreal@RC@@QEBAPEAVUObject@23@XZ");
 bound=1;return 1;
}
static int integer(char**p,uint64_t*v){char*e;*v=strtoull(*p,&e,10);if(e==*p)return 0;*p=e;return 1;}
static int property(void*p,int want,int ownerSize){return p&&*elementSize(p)==want&&*offset(p)>=0&&*offset(p)+want<=ownerSize;}
static Handle*lookup(uint64_t token){if(!token)return NULL;Handle*h=&handles[(token-1)%MAX_HANDLES];return h->used&&h->token==token?h:NULL;}
static int makeweak(WeakStorage*w,void*object){
 memset(w,0xcc,sizeof(*w));weakctor(w,object);
 /* The bundled implementation writes an index/serial pair. Keep ample opaque
  * storage and verify its unused guard, without reading its private fields. */
 for(size_t i=64;i<sizeof(*w);i++)if(w->bytes[i]!=0xcc)return -1;
 return weakget(w)==object;
}
static int readgetter(int which,void*object,unsigned char*out){Getter*g=&getters[which];memset(out,0,256);event(object,g->fn,out);return g->at;}
__declspec(dllexport) int qdcapture(void*L){
 if(!bind())return 0;
 __attribute__((aligned(16))) unsigned char lua[512]={0};construct(lua,L);View v={0};const char*message=NULL;char diagnostic[160];int returns=1;int64_t status=-3;
 if(!isstring(lua,1))goto done;
 getstring(lua,&v,1);if(!v.data||!v.size||v.size>=4096)goto done;
 char command[4096];memcpy(command,v.data,v.size);command[v.size]=0;
 if(command[0]=='I'){
  ready=0;uint64_t a[23];char*p=command+1;for(int i=0;i<23;i++)if(!integer(&p,&a[i]))goto done;if(*p)goto done;
  void*base=(void*)(uintptr_t)a[0],*mesh=(void*)(uintptr_t)a[1];if(!base||!mesh)goto done;
  int objectSize=*propertiesSize(base);if(objectSize<1||objectSize>1048576)goto done;
  if(!property((void*)(uintptr_t)a[2],4,objectSize)||!property((void*)(uintptr_t)a[3],8,objectSize))goto done;
  for(int i=0;i<4;i++){void*b=(void*)(uintptr_t)a[4+i];if(!property(b,1,objectSize))goto done;boolProps[i]=b;}
  int expected[6]={12,12,12,1,48,1};
  for(int i=0;i<6;i++){
   void*fn=(void*)(uintptr_t)a[8+i*2],*ret=(void*)(uintptr_t)a[9+i*2];if(!fn||!ret)goto done;
   int bytes=*parms(fn),at=*offset(ret);if(bytes>256||bytes<expected[i]||!property(ret,expected[i],bytes))goto done;
   getters[i]=(Getter){fn,ret,bytes,at};
  }
  int off[3]={0,16,32},sizes[3]={16,12,12};for(int i=0;i<3;i++){void*f=(void*)(uintptr_t)a[20+i];if(!property(f,sizes[i],48)||*offset(f)!=off[i])goto done;}
  if(makeweak(&baseWeak,base)!=1){message="weak handle storage validation";goto done;}
  baseClass=base;meshClass=mesh;hpAt=*offset((void*)(uintptr_t)a[2]);meshAt=*offset((void*)(uintptr_t)a[3]);
  memset(handles,0,sizeof(handles));serial=((serial/MAX_HANDLES)+1)*MAX_HANDLES;gameThread=GetCurrentThreadId();ready=1;status=1;
 }else if(command[0]=='X'){
  memset(handles,0,sizeof(handles));ready=0;status=1;
 }else if(ready&&GetCurrentThreadId()==gameThread&&weakget(&baseWeak)==baseClass){
  if(command[0]=='R'){
   if(stacksize(lua)!=2||!isnumber(lua,1)||!isnumber(lua,2))goto done;
   void*mesh=(void*)(uintptr_t)getinteger(lua,2),*actor=(void*)(uintptr_t)getinteger(lua,1);
   if(!actor||!mesh||!isa(actor,baseClass)||!isa(mesh,meshClass))goto done;
   void*actual;memcpy(&actual,(char*)actor+meshAt,8);if(actual!=mesh){status=-2;goto done;}
   for(int i=0;i<MAX_HANDLES;i++){
    uint64_t token=++serial;Handle*h=&handles[(token-1)%MAX_HANDLES];if(h->used)continue;
    /* Expired actor is terminal even before a handle was issued. A missing
     * mesh is not: a live actor may replace its presentation component. */
    int actorWeak=makeweak(&h->actor,actor);
    if(actorWeak<0){message="weak handle storage validation";goto done;}
    if(actorWeak==0){status=0;goto done;}
    int meshWeak=makeweak(&h->mesh,mesh);
    if(meshWeak<0){message="weak handle storage validation";goto done;}
    if(meshWeak==0){status=-2;goto done;}
    h->token=token;h->used=1;status=(int64_t)token;goto done;
   }
   message="capture handle capacity";
  }else if(command[0]=='S'||command[0]=='F'){
   if(stacksize(lua)!=1||!isnumber(lua,1))goto done;
   uint64_t token=(uint64_t)getinteger(lua,1);Handle*h=lookup(token);if(!h){status=0;goto done;}
   if(command[0]=='F'){h->used=0;status=1;goto done;}
   void*actor=weakget(&h->actor),*mesh=weakget(&h->mesh);if(!actor){h->used=0;status=0;goto done;}
   void*actual;memcpy(&actual,(char*)actor+meshAt,8);if(!mesh||actual!=mesh){h->used=0;status=-2;goto done;}
   __attribute__((aligned(16))) unsigned char args[256];readgetter(3,actor,args);
   if(getbool(getters[3].ret,args,0)){status=0;goto done;}
   double values[26]={1};float hp;memcpy(&hp,(char*)actor+hpAt,4);values[1]=hp;
   for(int i=0;i<4;i++)values[2+i]=getbool(boolProps[i],actor,0)?1:0;
   readgetter(5,mesh,args);values[6]=getbool(getters[5].ret,args,0)?1:0;
   for(int i=0;i<3;i++){int at=readgetter(i,actor,args);float xyz[3];memcpy(xyz,args+at,12);for(int j=0;j<3;j++)values[7+i*3+j]=xyz[j];}
   int at=readgetter(4,mesh,args);float q[4],p[3],s[3];memcpy(q,args+at,16);memcpy(p,args+at+16,12);memcpy(s,args+at+32,12);
   for(int i=0;i<3;i++){values[16+i]=p[i];}
   for(int i=0;i<4;i++){values[19+i]=q[i];}
   for(int i=0;i<3;i++){values[23+i]=s[i];}
   for(int i=0;i<26;i++)if(!isfinite(values[i])){snprintf(diagnostic,sizeof(diagnostic),"non-finite capture %d",i);message=diagnostic;goto done;}
   for(int i=0;i<26;i++){setnumber(lua,values[i]);}
   returns=26;goto finish;
  }
 }
done:
 if(message)setstring(lua,message,strlen(message));else setinteger(lua,status);
finish:destruct(lua);return returns;
}
