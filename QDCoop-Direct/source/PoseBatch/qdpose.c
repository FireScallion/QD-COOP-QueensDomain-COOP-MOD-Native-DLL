/* Same game-thread, same public SetBoneTransformByName, one Lua crossing.
 * No worker, private UObject layout, executable patch or render setting.
 * Parameter layout is obtained from UE4SS reflected FProperty exports and
 * checked before use. Only trusted local pose data enters this module.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct {const char *data;size_t size;} View;
static void*(*construct)(void*,void*);static void(*destruct)(void*);
static View*(*getstring)(void*,View*,int);static void(*setstring)(void*,const char*,size_t);static bool(*isstring)(void*,int);
static double(*getnumber)(void*,int);static int64_t(*getinteger)(void*,int);static bool(*isnumber)(void*,int);static int(*stacksize)(void*);
static void(*event)(void*,void*,void*);static void*(*name)(void*,uint32_t,uint32_t);
static int*(*offset)(void*);static int*(*size)(void*);static uint16_t*(*parms)(void*);static bool(*isa)(void*,void*);
static void *function,*poseClass;static int bound,ready;
static int integer(char**p,uint64_t*v){char*end;*v=strtoull(*p,&end,10);if(end==*p)return 0;*p=end;return 1;}
static int bind(void){if(bound)return 1;HMODULE h=GetModuleHandleW(L"UE4SS.dll");if(!h)return 0;
#define API(v,n) {FARPROC p=GetProcAddress(h,n);memcpy(&v,&p,sizeof(p));if(!v)return 0;}
 API(construct,"??0Lua@LuaMadeSimple@RC@@QEAA@PEAUlua_State@@@Z");API(destruct,"??1Lua@LuaMadeSimple@RC@@QEAA@XZ");
 API(getstring,"?get_string@Lua@LuaMadeSimple@RC@@QEBA?AV?$basic_string_view@DU?$char_traits@D@std@@@std@@H@Z");
 API(setstring,"?set_string@Lua@LuaMadeSimple@RC@@QEBAXPEBD_K@Z");API(isstring,"?is_string@Lua@LuaMadeSimple@RC@@QEBA_NH@Z");
 API(getnumber,"?get_number@Lua@LuaMadeSimple@RC@@QEBANH@Z");API(getinteger,"?get_integer@Lua@LuaMadeSimple@RC@@QEBA_JH@Z");API(isnumber,"?is_number@Lua@LuaMadeSimple@RC@@QEBA_NH@Z");API(stacksize,"?get_stack_size@Lua@LuaMadeSimple@RC@@QEBAHXZ");
 API(event,"?ProcessEvent@UObject@Unreal@RC@@QEAAXPEAVUFunction@23@PEAX@Z");API(name,"??0FName@Unreal@RC@@QEAA@II@Z");
 API(offset,"?GetOffset_Internal@FProperty@Unreal@RC@@QEAAAEAHXZ");API(size,"?GetElementSize@FProperty@Unreal@RC@@QEAAAEAHXZ");
 API(parms,"?GetParmsSize@UFunction@Unreal@RC@@QEAAAEAGXZ");API(isa,"?IsA@UObjectBase@Unreal@RC@@QEBA_NPEAVUClass@23@@Z");
 bound=1;return 1;
}
__declspec(dllexport) int qdpose(void*L){if(!bind())return 0;
 __attribute__((aligned(16))) unsigned char lua[512]={0};construct(lua,L);View v={0};const char *result="0";char diagnostic[160];
 if(!isstring(lua,1))goto done;
 getstring(lua,&v,1);if(!v.data||!v.size||v.size>=32768)goto done;
 // Only initialization uses ASCII: get_string truncates embedded NULs in this
 // UE4SS build. Per-frame values use exported numeric accessors directly.
 char command[32768];memcpy(command,v.data,v.size);command[v.size]=0;char*p=command+1;
 if(command[0]=='I'){
  uint64_t a[8];for(int i=0;i<8;i++)if(!integer(&p,&a[i]))goto done;if(*p)goto done;
  void*fn=(void*)(uintptr_t)a[0];void*cls=(void*)(uintptr_t)a[1];
  int expectOffset[]={0,16,64,0,16,32},expectSize[]={8,48,1,16,12,12};
  for(int i=0;i<6;i++){void*prop=(void*)(uintptr_t)a[i+2];if(!prop)goto done;if(*offset(prop)!=expectOffset[i]||*size(prop)!=expectSize[i]){snprintf(diagnostic,sizeof(diagnostic),"layout %d offset=%d size=%d",i,*offset(prop),*size(prop));result=diagnostic;goto done;}}
  if(!fn||!cls)goto done;
  if(*parms(fn)<65||*parms(fn)>128){snprintf(diagnostic,sizeof(diagnostic),"parms=%u",*parms(fn));result=diagnostic;goto done;}
  function=fn;poseClass=cls;ready=1;result="1";
 }else if(command[0]=='P'&&ready){
  // LuaMadeSimple getters REMOVE their argument. Read the two headers, then
  // consume pose values from the top to avoid quadratic stack shuffling.
  if(!isnumber(lua,1)||!isnumber(lua,2))goto done;
  uint64_t count=(uint64_t)getinteger(lua,2),address=(uint64_t)getinteger(lua,1);if(count>140||stacksize(lua)!=(int)count*9)goto done;
  void*target=(void*)(uintptr_t)address;if(!target||!isa(target,poseClass))goto done;
  struct {uint32_t index,number;float x[7];} poses[140];
  // Validate ALL values before changing any bone, so fallback is all-or-none.
  for(int i=(int)count-1;i>=0;i--){
   for(int j=6;j>=0;j--){if(!isnumber(lua,-1))goto done;float x=(float)getnumber(lua,-1);if(!isfinite(x)||fabsf(x)>1e7f)goto done;poses[i].x[j]=x;}
   if(!isnumber(lua,-1)||!isnumber(lua,-2))goto done;
   uint64_t number=(uint64_t)getinteger(lua,-1),index=(uint64_t)getinteger(lua,-1);if(index>UINT32_MAX||number>UINT32_MAX)goto done;
   poses[i].index=(uint32_t)index;poses[i].number=(uint32_t)number;
  }
  for(uint32_t i=0;i<count;i++){
   float*x=poses[i].x;
   __attribute__((aligned(16))) unsigned char args[128]={0};name(args,poses[i].index,poses[i].number);
   memcpy(args+16,x+3,16);memcpy(args+32,x,12);float scale[3]={1,1,1};memcpy(args+48,scale,12);args[64]=1;
   event(target,function,args);
  }result="1";
 }
 done:setstring(lua,result,strlen(result));destruct(lua);return 1;
}

/* Optional monotonic diagnostics; never reads or writes an Unreal object. */
__declspec(dllexport) int qdclock(void*L){
 if(!bind())return 0;static void(*setnumber)(void*,double);static LARGE_INTEGER frequency;
 if(!setnumber){FARPROC p=GetProcAddress(GetModuleHandleW(L"UE4SS.dll"),"?set_number@Lua@LuaMadeSimple@RC@@QEBAXN@Z");memcpy(&setnumber,&p,sizeof(p));if(!setnumber||!QueryPerformanceFrequency(&frequency))return 0;}
 LARGE_INTEGER now;QueryPerformanceCounter(&now);__attribute__((aligned(16))) unsigned char lua[512]={0};construct(lua,L);setnumber(lua,(double)now.QuadPart/(double)frequency.QuadPart);destruct(lua);return 1;
}
#ifdef QD_DIAGNOSTIC_TEST
/* Disposable probe only: measure a confirmed no-parameter UFunction through
   the same public ProcessEvent, without per-call Lua marshalling or hooks. */
__declspec(dllexport) int qdbench(void*L){
 if(!bind())return 0;__attribute__((aligned(16))) unsigned char lua[512]={0};construct(lua,L);const char*answer="invalid";char out[160];
 if(stacksize(lua)==3&&isnumber(lua,1)&&isnumber(lua,2)&&isnumber(lua,3)){
  uint64_t n=(uint64_t)getinteger(lua,3);void*fn=(void*)(uintptr_t)getinteger(lua,2);void*target=(void*)(uintptr_t)getinteger(lua,1);
  if(n>0&&n<=5000&&target&&fn&&*parms(fn)==0){LARGE_INTEGER freq,a,b;QueryPerformanceFrequency(&freq);QueryPerformanceCounter(&a);for(uint64_t i=0;i<n;i++)event(target,fn,NULL);QueryPerformanceCounter(&b);snprintf(out,sizeof(out),"%.9f",(double)(b.QuadPart-a.QuadPart)*1000/(double)freq.QuadPart/(double)n);answer=out;}
 }setstring(lua,answer,strlen(answer));destruct(lua);return 1;
}
#endif

/* Called once from the engine-frame callback to identify its OS thread. */
__declspec(dllexport) int qdthread(void* L) {
    if (!bind()) return 0;
    __attribute__((aligned(16))) unsigned char lua[512] = {0};
    char result[64];
    construct(lua, L);
    snprintf(result, sizeof(result), "%lu|%lu", GetCurrentProcessId(), GetCurrentThreadId());
    setstring(lua, result, strlen(result));
    destruct(lua);
    return 1;
}
