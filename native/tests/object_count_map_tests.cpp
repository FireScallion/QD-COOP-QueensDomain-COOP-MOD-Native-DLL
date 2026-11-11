#include "qdcoop/object_count_map.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
namespace U=qdcoop::ue427;
struct Memory {
 std::vector<unsigned char> bytes=std::vector<unsigned char>(20000);
 template<class T>void put(unsigned offset,T value){std::memcpy(bytes.data()+offset,&value,sizeof(value));}
 bool read(std::uintptr_t address,void* out,std::size_t size){if(address<1000||address-1000>bytes.size()||size>bytes.size()-(address-1000))return false;std::memcpy(out,bytes.data()+address-1000,size);return true;}
};
int main(){
 const std::array<std::int32_t,6> layout{8,16,20,24,8,24};Memory m;auto read=[&](auto p,auto out,auto n){return m.read(p,out,n);};std::int32_t value=-7;
 m.put(44,std::int32_t{128});m.put(48,std::int32_t{-1});assert(U::object_count(1000,0x777,layout,read,value)&&value==0);
 m.put(0,std::uintptr_t{2000});m.put(8,std::int32_t{3});m.put(12,std::int32_t{3});m.put(16,std::uint32_t{5});m.put(40,std::int32_t{3});m.put(48,std::int32_t{1});m.put(52,std::int32_t{1});
 m.put(1000,std::uintptr_t{0x123});m.put(1008,std::int32_t{4});m.put(1024,std::uintptr_t{0x777});m.put(1032,std::int32_t{999});m.put(1048,std::uintptr_t{0x777});m.put(1056,std::int32_t{8});
 assert(U::object_count(1000,0x777,layout,read,value)&&value==8);assert(U::object_count(1000,0x888,layout,read,value)&&value==0);
 m.put(16,std::uint32_t{7});assert(!U::object_count(1000,0x777,layout,read,value));m.put(16,std::uint32_t{5});m.put(1056,std::int32_t{-1});assert(!U::object_count(1000,0x777,layout,read,value));m.put(1056,std::int32_t{8});
 auto bad=layout;bad[0]=4;assert(!U::object_count(1000,0x777,bad,read,value));m.put(0,std::uintptr_t{99999999});assert(!U::object_count(1000,0x777,layout,read,value));
 Memory large;m=large;m.put(0,std::uintptr_t{2000});m.put(8,std::int32_t{129});m.put(12,std::int32_t{129});m.put(32,std::uintptr_t{9000});m.put(40,std::int32_t{129});m.put(44,std::int32_t{160});m.put(48,std::int32_t{-1});
 for(unsigned i=0;i<5;++i)m.put(8000+i*4,std::uint32_t{0xffffffff});
 for(unsigned i=0;i<129;++i){m.put(1000+i*24,std::uintptr_t{10000+i});m.put(1008+i*24,std::int32_t(i));}
 assert(U::object_count(1000,10128,layout,read,value)&&value==128);m.put(32,std::uintptr_t{0});assert(!U::object_count(1000,10128,layout,read,value));
  bool flag=false;m.put(32,std::uintptr_t{9000});m.put(1008+128*24,std::uint32_t{0xffccaa01});assert(U::object_bool(1000,10128,layout,read,flag)&&flag);
 m.put(1008+128*24,std::uint32_t{0xffccaa00});assert(U::object_bool(1000,10128,layout,read,flag)&&!flag);
 m.put(1008+128*24,std::uint32_t{0xffccaa02});assert(!U::object_bool(1000,10128,layout,read,flag));assert(U::object_bool(1000,99999,layout,read,flag)&&!flag);
 std::puts("PASS object-bool map: heap allocation flags, nonzero padding, true/false, absent key and malformed bool");
 std::puts("PASS pinned object-count map: empty, sparse holes, absent key, duplicate/malformed entries, heap bitset, layout mismatch and unreadable memory");
}
