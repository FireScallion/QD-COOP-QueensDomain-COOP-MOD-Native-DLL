#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
static void tick(float){}
static void full(void*,float,bool){}
int wmain(int argc,wchar_t** argv){if(argc!=2)return 2;auto dll=LoadLibraryW(argv[1]);if(!dll)return 3;int(*simple)(void(*)(float)){};int(*native)(void(*)(void*,float,bool)){};auto a=GetProcAddress(dll,"QDInstallEngineTick"),b=GetProcAddress(dll,"QDInstallNativeTick");std::memcpy(&simple,&a,sizeof(a));std::memcpy(&native,&b,sizeof(b));if(!simple||!native)return 4;if(simple(nullptr)||native(nullptr)||simple(&tick)||native(&full))return 5;FreeLibrary(dll);std::puts("PASS native Tick bridge exports; null callbacks and non-game executable rejected before engine memory access");return 0;}
