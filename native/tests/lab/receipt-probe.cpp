// Temporary M1 adapter. All Unreal access occurs on the registered EngineTick
// thread. The worker owns packet bytes only, and never touches Unreal objects.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#include <bcrypt.h>
#include <tlhelp32.h>
#include "qdcoop/enemy_stream.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

namespace E=qdcoop::enemy;
struct View{const char* data;std::size_t size;};
struct alignas(16) Weak{unsigned char bytes[256]{};};
struct Api {
    void*(*construct)(void*,void*);void(*destruct)(void*);
    View*(*getstring)(void*,View*,int);void(*setstring)(void*,const char*,std::size_t);
    bool(*isstring)(void*,int),(*isnumber)(void*,int);std::int64_t(*getinteger)(void*,int);
    double(*getnumber)(void*,int);void(*setinteger)(void*,std::int64_t);void(*setnumber)(void*,double);int(*stacksize)(void*);
    void(*event)(void*,void*,void*);int*(*offset)(void*);int*(*size)(void*);int*(*propertiesSize)(void*);std::uint16_t*(*parms)(void*);
    bool(*isa)(void*,void*),(*getbool)(void*,const void*,int);void(*setbool)(void*,void*,bool,int);
    void*(*weakctor)(void*,const void*);void*(*weakget)(const void*);
}api{};
static bool bound;
template<class F>static bool proc(HMODULE h,F& fn,const char* name){auto p=GetProcAddress(h,name);static_assert(sizeof(p)==sizeof(fn));std::memcpy(&fn,&p,sizeof(p));return p!=nullptr;}
static bool bind(){
    if(bound)return true;
    auto h=GetModuleHandleW(L"UE4SS.dll");if(!h)return false;
#define API(v,n) if(!proc(h,api.v,n))return false
    API(construct,"??0Lua@LuaMadeSimple@RC@@QEAA@PEAUlua_State@@@Z");API(destruct,"??1Lua@LuaMadeSimple@RC@@QEAA@XZ");
    API(getstring,"?get_string@Lua@LuaMadeSimple@RC@@QEBA?AV?$basic_string_view@DU?$char_traits@D@std@@@std@@H@Z");
    API(setstring,"?set_string@Lua@LuaMadeSimple@RC@@QEBAXPEBD_K@Z");API(isstring,"?is_string@Lua@LuaMadeSimple@RC@@QEBA_NH@Z");
    API(isnumber,"?is_number@Lua@LuaMadeSimple@RC@@QEBA_NH@Z");API(getinteger,"?get_integer@Lua@LuaMadeSimple@RC@@QEBA_JH@Z");
    API(getnumber,"?get_number@Lua@LuaMadeSimple@RC@@QEBANH@Z");API(setinteger,"?set_integer@Lua@LuaMadeSimple@RC@@QEBAX_J@Z");
    API(setnumber,"?set_number@Lua@LuaMadeSimple@RC@@QEBAXN@Z");API(stacksize,"?get_stack_size@Lua@LuaMadeSimple@RC@@QEBAHXZ");
    API(event,"?ProcessEvent@UObject@Unreal@RC@@QEAAXPEAVUFunction@23@PEAX@Z");
    API(offset,"?GetOffset_Internal@FProperty@Unreal@RC@@QEAAAEAHXZ");API(size,"?GetElementSize@FProperty@Unreal@RC@@QEAAAEAHXZ");
    API(propertiesSize,"?GetPropertiesSize@UStruct@Unreal@RC@@QEAAAEAHXZ");API(parms,"?GetParmsSize@UFunction@Unreal@RC@@QEAAAEAGXZ");
    API(isa,"?IsA@UObjectBase@Unreal@RC@@QEBA_NPEAVUClass@23@@Z");
    API(getbool,"?GetPropertyValueInContainer@FBoolProperty@Unreal@RC@@QEAA_NPEBXH@Z");
    API(setbool,"?SetPropertyValueInContainer@FBoolProperty@Unreal@RC@@QEAA@PEAX_NH@Z");
    API(weakctor,"??0FWeakObjectPtr@Unreal@RC@@QEAA@PEBVUObject@12@@Z");API(weakget,"?Get@FWeakObjectPtr@Unreal@RC@@QEBAPEAVUObject@23@XZ");
#undef API
    bound=true;return true;
}
extern "C" __declspec(dllexport) int qdreceipt(void* L){
 if(!bind())return 0;
 alignas(16) unsigned char lua[512]{};api.construct(lua,L);View v{};api.getstring(lua,&v,1);std::string command(v.data,v.size);char* p=command.data();char* end{};std::uint64_t a[5]{};for(auto& n:a){n=std::strtoull(p,&end,10);p=end;}bool ok=false;
 if(a[0]&&a[1]&&a[2]&&a[3]&&a[4]&&!*p){auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));std::uintptr_t exec{};SIZE_T n{};auto prop=reinterpret_cast<void*>(a[3]);auto fn=reinterpret_cast<void*>(a[1]);
  if(*api.parms(fn)==24&&*api.size(prop)==80&&*api.offset(prop)>=0&&*api.offset(prop)<65536&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(a[2]+216),&exec,8,&n)&&n==8&&exec==base+0x2d7f640){
   unsigned char call[5]{};std::int32_t displacement{};if(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(exec+0x20a),call,5,&n)&&n==5&&call[0]==0xe8){std::memcpy(&displacement,call+1,4);auto target=exec+0x20f+displacement;
    if(target==base+0x2755ec0){using Add=void(*)(void*,void*,const void*,const void*);Add add{};std::memcpy(&add,&target,8);for(unsigned i=0;i<16;++i){wchar_t name[80]{};auto length=std::swprintf(name,80,L"QDCOOP_NATIVE_TEST_Receipt_%02u",i)+1;alignas(16) unsigned char args[24]{};auto chars=name;std::memcpy(args,&chars,8);std::memcpy(args+8,&length,4);std::memcpy(args+12,&length,4);api.event(reinterpret_cast<void*>(a[0]),fn,args);std::uint32_t value=0xdeadbeefu+i;add(reinterpret_cast<void*>(a[4]+*api.offset(prop)),prop,args+16,&value);}ok=true;}
   }
  }
 }
 api.setinteger(lua,ok?1:0);api.destruct(lua);return 1;
}
