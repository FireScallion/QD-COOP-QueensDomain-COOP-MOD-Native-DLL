/* Game-thread-only cutscene delegate escrow. A detached completion delegate
 * cannot commit story/world state on the presentation peer. Reflected offsets,
 * bounded arrays and engine weak serials are validated before any mutation.
 * No network data reaches this API and no callback is executed by this DLL. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
typedef struct{const char*data;size_t size;} View;
typedef union{uint64_t align[32];unsigned char bytes[256];} Weak;
typedef struct{uint64_t object,name;} Delegate;
typedef struct{void*data;int32_t num,max;} Array;
typedef struct{int64_t token;Weak owner;void*cls;int offset,count;Delegate entries[8];} Escrow;
typedef struct{Weak fn;unsigned char*data;unsigned char saved[2];int64_t token;} Guard;
static Escrow escrow[32];static Guard guards[64];static int64_t serial;static DWORD thread;
static void*(*construct)(void*,void*);static void(*destruct)(void*);
static View*(*getstring)(void*,View*,int);static bool(*isnumber)(void*,int);static int64_t(*getinteger)(void*,int);static int(*stacksize)(void*);
static void(*setinteger)(void*,int64_t);static int*(*offset)(void*);static int*(*size)(void*);static int*(*structsize)(void*);static bool(*isa)(void*,void*);
static void*(*weakctor)(void*,const void*);static void*(*weakget)(const void*);static void(*pushObject)(const void*,void*);
static void*(*function)(void*,uint64_t);static unsigned char*(*params)(void*);static Array*(*script)(void*);static int bound;
static int bind(void){if(bound)return 1;HMODULE h=GetModuleHandleW(L"UE4SS.dll");if(!h)return 0;
#define API(v,n) {FARPROC p=GetProcAddress(h,n);memcpy(&v,&p,sizeof(p));if(!v)return 0;}
 API(construct,"??0Lua@LuaMadeSimple@RC@@QEAA@PEAUlua_State@@@Z");API(destruct,"??1Lua@LuaMadeSimple@RC@@QEAA@XZ");
 API(getstring,"?get_string@Lua@LuaMadeSimple@RC@@QEBA?AV?$basic_string_view@DU?$char_traits@D@std@@@std@@H@Z");
 API(isnumber,"?is_number@Lua@LuaMadeSimple@RC@@QEBA_NH@Z");API(getinteger,"?get_integer@Lua@LuaMadeSimple@RC@@QEBA_JH@Z");API(stacksize,"?get_stack_size@Lua@LuaMadeSimple@RC@@QEBAHXZ");API(setinteger,"?set_integer@Lua@LuaMadeSimple@RC@@QEBAX_J@Z");
 API(offset,"?GetOffset_Internal@FProperty@Unreal@RC@@QEAAAEAHXZ");API(size,"?GetElementSize@FProperty@Unreal@RC@@QEAAAEAHXZ");API(structsize,"?GetPropertiesSize@UStruct@Unreal@RC@@QEAAAEAHXZ");API(isa,"?IsA@UObjectBase@Unreal@RC@@QEBA_NPEAVUClass@23@@Z");
 API(weakctor,"??0FWeakObjectPtr@Unreal@RC@@QEAA@PEBVUObject@12@@Z");API(weakget,"?Get@FWeakObjectPtr@Unreal@RC@@QEBAPEAVUObject@23@XZ");
 API(pushObject,"?auto_construct_object@LuaType@RC@@YAXAEBVLua@LuaMadeSimple@2@PEAVUObject@Unreal@2@@Z");
 API(function,"?GetFunctionByNameInChain@UObject@Unreal@RC@@QEAAPEAVUFunction@23@VFName@23@@Z");API(params,"?GetNumParms@UFunction@Unreal@RC@@QEAAAEAEXZ");
 API(script,"?GetScript@UStruct@Unreal@RC@@QEAAAEAV?$TArray@EV?$TSizedDefaultAllocator@$0CA@@Unreal@RC@@@23@XZ");
 bound=1;return 1;}
static void*getweak(uint64_t id){Weak w={0};memcpy(w.bytes,&id,8);return weakget(&w);}
static int makeweak(Weak*w,void*o){memset(w,0xcc,sizeof(*w));weakctor(w,o);for(int i=8;i<256;i++)if(w->bytes[i]!=0xcc)return 0;return weakget(w)==o;}
__declspec(dllexport) int qdscene(void*L){if(!bind())return 0;
 __attribute__((aligned(16)))unsigned char lua[512]={0};construct(lua,L);View v={0};getstring(lua,&v,1);int64_t result=-1;int returns=1;
 if(!v.data||v.size!=1)goto done;
 if(!thread)thread=GetCurrentThreadId();if(thread!=GetCurrentThreadId())goto done;
 int count=stacksize(lua);int64_t a[4]={0};if(count>4)goto done;for(int i=count-1;i>=0;i--){if(!isnumber(lua,i+1))goto done;a[i]=getinteger(lua,i+1);}
 if(*v.data=='T'){result=1;goto done;}
 if(*v.data=='D'&&count==3){
  void*o=(void*)(uintptr_t)a[0],*cls=(void*)(uintptr_t)a[1],*prop=(void*)(uintptr_t)a[2];Weak w;
  if(!o||!cls||!prop||!makeweak(&w,o)||!isa(o,cls)||*size(prop)!=(int)sizeof(Array))goto done;
  int at=*offset(prop);if(at<0||at+16>*structsize(cls))goto done;Array*list=(Array*)((char*)o+at);
  if(list->num<0||list->num>8||list->max<list->num||list->max>64||(list->num&&!list->data))goto done;
  Delegate*entries=list->data;
  for(int i=0;i<list->num;i++){void*target=getweak(entries[i].object);if(!target)continue;void*fn=function(target,entries[i].name);if(!fn||*params(fn)!=0)goto done;}
  int slot=-1;for(int i=0;i<32;i++)if(!escrow[i].token){slot=i;break;}if(slot<0)goto done;
  Escrow*e=&escrow[slot];e->owner=w;e->cls=cls;e->offset=at;e->count=list->num;e->token=++serial;if(e->count)memcpy(e->entries,entries,e->count*sizeof(Delegate));
  /* FScriptDelegate is just weak object identity and FName; no owned destructor.
   * Keep the engine allocation/capacity for restoration, detach only its count. */
  list->num=0;result=e->token;
 }else if((*v.data=='C'||*v.data=='O'||*v.data=='F'||*v.data=='R'||*v.data=='X')&&count>=1){
  Escrow*e=NULL;for(int i=0;i<32;i++)if(escrow[i].token==a[0]&&a[0]>0){e=&escrow[i];break;}if(!e)goto done;
  if(*v.data=='C')result=e->count;
  else if(*v.data=='X'){e->token=0;result=1;}
  else if(*v.data=='R'){
   void*o=weakget(&e->owner);if(!o){e->token=0;result=0;goto done;}Array*list=(Array*)((char*)o+e->offset);
   if(list->num!=0||list->max<e->count||(e->count&&!list->data))goto done;
   if(e->count)memcpy(list->data,e->entries,e->count*sizeof(Delegate));list->num=e->count;e->token=0;result=1;
  }else if(count==2&&a[1]>=1&&a[1]<=e->count){Delegate*d=&e->entries[a[1]-1];void*o=getweak(d->object);if(!o){result=0;goto done;}if(*v.data=='F')o=function(o,d->name);if(!o){result=0;goto done;}pushObject(lua,o);goto finish;}
 }else if(*v.data=='G'&&count==1){
  void*fn=(void*)(uintptr_t)a[0];Weak w;if(!fn||!makeweak(&w,fn))goto done;
  Array*s=script(fn);if(!s||!s->data||s->num<3||s->num>1048576||s->max<s->num)goto done;
  for(int i=0;i<64;i++)if(guards[i].token&&weakget(&guards[i].fn)==fn){result=guards[i].token;goto done;}
  int slot=-1;for(int i=0;i<64;i++)if(!guards[i].token){slot=i;break;}if(slot<0)goto done;
  Guard*g=&guards[slot];g->fn=w;g->data=s->data;memcpy(g->saved,s->data,2);g->token=++serial;
  ((unsigned char*)s->data)[0]=0x04;((unsigned char*)s->data)[1]=0x0b;result=g->token;
 }else if(*v.data=='V'&&count==1){
  result=0;for(int i=0;i<64;i++)if(guards[i].token==a[0]&&a[0]>0){Guard*g=&guards[i];void*fn=weakget(&g->fn);if(fn){Array*s=script(fn);result=s->data==g->data&&s->num>=2&&((unsigned char*)s->data)[0]==4&&((unsigned char*)s->data)[1]==11;}break;}
 }else if(*v.data=='U'&&count==1){
  for(int i=0;i<64;i++)if(guards[i].token==a[0]&&a[0]>0){Guard*g=&guards[i];void*fn=weakget(&g->fn);if(fn){Array*s=script(fn);if(s->data!=g->data||s->num<2||((unsigned char*)s->data)[0]!=4||((unsigned char*)s->data)[1]!=11)goto done;memcpy(s->data,g->saved,2);}g->token=0;result=1;break;}
 }
done:setinteger(lua,result);
finish:destruct(lua);return returns;
}
