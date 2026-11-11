#include "qdcoop/weak_object_view.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
namespace U=qdcoop::ue427;
struct Memory{std::vector<unsigned char> data=std::vector<unsigned char>(4000000);bool read(std::uintptr_t p,void* out,std::size_t size){if(p<1000||p-1000>data.size()||size>data.size()-(p-1000))return false;std::memcpy(out,data.data()+p-1000,size);return true;}template<class T>void put(std::uintptr_t p,T value){assert(p>=1000&&p-1000+sizeof(value)<=data.size());std::memcpy(data.data()+p-1000,&value,sizeof(value));}};
int main(){Memory m;auto read=[&](auto p,auto out,auto n){return m.read(p,out,n);};U::ObjectArray a{2000,0,131072,65537,2,2};m.put(1000,a);m.put(2000,std::uintptr_t{3000});m.put(2008,std::uintptr_t{2000000});const auto last=3000+65535*24;U::ObjectItem item{0x123400,0,0,37,0};m.put(last,item);m.put(2000000,item);
 assert(U::resolve_weak(1000,{65535,37},read)==item.object&&U::resolve_weak(1000,{65536,37},read)==item.object);
 assert(!U::resolve_weak(1000,{-1,37},read)&&!U::resolve_weak(1000,{65537,37},read)&&!U::resolve_weak(1000,{65536,0},read)&&!U::resolve_weak(1000,{65536,38},read));
 for(auto flags:{0x10000000u,0x20000000u,0x30000000u}){item.flags=flags;m.put(2000000,item);assert(!U::resolve_weak(1000,{65536,37},read));}item.flags=0x01000000u;m.put(2000000,item);assert(U::resolve_weak(1000,{65536,37},read)==item.object);
 item.serial=38;item.object=0x345600;m.put(2000000,item);assert(!U::resolve_weak(1000,{65536,37},read)&&U::resolve_weak(1000,{65536,38},read)==item.object);
 unsigned reads=0;auto racing=[&](auto p,auto out,auto n){if(p==2000000&&++reads==2){auto changed=item;++changed.serial;m.put(2000000,changed);}return m.read(p,out,n);};assert(!U::resolve_weak(1000,{65536,38},racing));m.put(2000000,item);
 m.put(2008,std::uintptr_t{});assert(!U::resolve_weak(1000,{65536,38},read));m.put(2008,std::uintptr_t{999999999});assert(!U::resolve_weak(1000,{65536,38},read));m.put(2008,std::uintptr_t{2000000});a.num_chunks=1;m.put(1000,a);assert(!U::resolve_weak(1000,{65536,38},read));a.num_chunks=2;a.num_elements=-1;m.put(1000,a);assert(!U::resolve_weak(1000,{65536,38},read));a.num_elements=65537;a.chunks=UINTPTR_MAX-1;m.put(1000,a);assert(!U::resolve_weak(1000,{65536,38},read));
 std::puts("PASS native weak view: chunk boundary, null/stale/reused slots, pending-kill/unreachable, serial change during read, corrupt shape and unreadable/overflow pointers");return 0;
}
