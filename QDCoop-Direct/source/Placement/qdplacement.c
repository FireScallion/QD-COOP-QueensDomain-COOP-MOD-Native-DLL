/* Same game-thread, same public K2 actor/component placement calls.
 * No worker, private UObject layout, executable patch or render setting.
 * Parameter layout is obtained from UE4SS reflected FProperty exports and
 * checked before use. Full scale retained. Called only with validated local handles.
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
static int bound;
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

/* Independent full-transform placement entry. This is NOT the unit-scale
 * player bone helper. Scale, root movement, collision, interpolation and
 * engine call order are identical to the Lua K2 functions. No worker thread. */
static void*actorFn,*actorClass,*meshFn,*sceneClass;static int placementReady;
__declspec(dllexport) int qdplacement(void*L){
 if(!bind())return 0;
 __attribute__((aligned(16))) unsigned char lua[512]={0};construct(lua,L);View v={0};const char*answer="0";char message[128];
 if(!isstring(lua,1))goto done;getstring(lua,&v,1);if(!v.data||!v.size||v.size>=1024)goto done;
 char command[1024];memcpy(command,v.data,v.size);command[v.size]=0;
 if(command[0]=='I'){
  char*p=command+1;uint64_t a[17];for(int i=0;i<17;i++)if(!integer(&p,&a[i]))goto done;if(*p)goto done;
  int offsets[]={0,12,24,28,164,165,0,48,52,188,0,16,32},sizes[]={12,12,1,136,1,1,48,1,136,1,16,12,12};
  int indices[]={2,3,4,5,6,7,10,11,12,13,14,15,16};
  for(int i=0;i<13;i++){void*prop=(void*)(uintptr_t)a[indices[i]];if(!prop)goto done;if(*offset(prop)!=offsets[i]||*size(prop)!=sizes[i]){snprintf(message,sizeof(message),"layout %d offset=%d size=%d",i,*offset(prop),*size(prop));answer=message;goto done;}}
  void*af=(void*)(uintptr_t)a[0],*mf=(void*)(uintptr_t)a[8];if(!af||!mf||*parms(af)<166||*parms(af)>256||*parms(mf)<189||*parms(mf)>256)goto done;
  actorFn=af;actorClass=(void*)(uintptr_t)a[1];meshFn=mf;sceneClass=(void*)(uintptr_t)a[9];placementReady=actorClass&&sceneClass;answer=placementReady?"1":"0";
 }else if(placementReady&&(command[0]=='A'||command[0]=='M')){
  int actor=command[0]=='A',n=actor?6:10;if(stacksize(lua)!=n+1||!isnumber(lua,1))goto done;void*target=(void*)(uintptr_t)getinteger(lua,1);if(!target||!isa(target,actor?actorClass:sceneClass))goto done;
  float f[10];for(int i=n-1;i>=0;i--){if(!isnumber(lua,-1))goto done;f[i]=(float)getnumber(lua,-1);if(!isfinite(f[i])||fabsf(f[i])>1e7f)goto done;}
  __attribute__((aligned(16))) unsigned char args[256]={0};
  if(actor){memcpy(args,f,24);args[164]=1;event(target,actorFn,args);}
  else{memcpy(args,f+3,16);memcpy(args+16,f,12);memcpy(args+32,f+7,12);args[188]=1;event(target,meshFn,args);}
  answer="1";
 }
 done:setstring(lua,answer,strlen(answer));destruct(lua);return 1;
}
