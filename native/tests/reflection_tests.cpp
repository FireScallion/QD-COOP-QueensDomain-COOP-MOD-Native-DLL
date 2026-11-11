#include "qdcoop/reflection_view.hpp"
#include <array>
#include <cassert>
#include <cstdio>
namespace U=qdcoop::ue427;
template<class T>void put(void* p,unsigned at,T v){std::memcpy(static_cast<unsigned char*>(p)+at,&v,sizeof(v));}
int main(){alignas(16) std::array<unsigned char,256> prop{},object{},base{},derived{};
 put(prop.data(),76,7);put(prop.data(),60,3);prop[121]=1;prop[122]=32;prop[123]=32;
 for(unsigned mask:{1u,2u,4u,8u,16u,32u,64u,128u,255u}){prop[123]=static_cast<unsigned char>(mask);prop[122]=static_cast<unsigned char>(mask==255?1:mask);for(int index=0;index<3;++index)for(unsigned b=0;b<256;++b)for(bool value:{false,true}){object.fill(0xa5);auto at=8+index*3;object[at]=static_cast<unsigned char>(b);assert(U::get_bool(prop.data(),object.data(),index)==((b&mask)!=0));auto before=object;U::set_bool(prop.data(),object.data(),value,index);assert(object[at]==static_cast<unsigned char>((b&~mask)|(value?prop[122]:0)));for(unsigned i=0;i<object.size();++i)if(i!=static_cast<unsigned>(at))assert(object[i]==before[i]);}}
 put(object.data(),16,static_cast<void*>(derived.data()));put(derived.data(),64,static_cast<void*>(base.data()));assert(U::is_a(object.data(),derived.data())&&U::is_a(object.data(),base.data())&&!U::is_a(object.data(),prop.data()));assert(!U::is_a(nullptr,base.data())&&!U::is_a(object.data(),nullptr));put(base.data(),64,static_cast<void*>(derived.data()));assert(!U::is_a(object.data(),prop.data()));
 put(prop.data(),88,3026);put(prop.data(),182,std::uint16_t{12});assert(*U::property_offset(prop.data())==7&&*U::element_size(prop.data())==3&&*U::properties_size(prop.data())==3026&&*U::parameter_size(prop.data())==12);
 std::puts("PASS native reflection: packed/full-byte bool read/write, preservation of adjacent bits/bytes, array stride/byte offset, inheritance/null/cycle bounds, metadata fields");
}
