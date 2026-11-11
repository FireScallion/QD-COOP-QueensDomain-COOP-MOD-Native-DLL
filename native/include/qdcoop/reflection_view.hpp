#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
namespace qdcoop::ue427 {
// Current Steam Win64 layout only; caller owns game thread and valid metadata.
template<class T>T field(const void* p,unsigned at){T value{};std::memcpy(&value,static_cast<const unsigned char*>(p)+at,sizeof(value));return value;}
inline int* property_offset(void* p){return reinterpret_cast<int*>(static_cast<unsigned char*>(p)+76);}
inline int* element_size(void* p){return reinterpret_cast<int*>(static_cast<unsigned char*>(p)+60);}
inline int* properties_size(void* p){return reinterpret_cast<int*>(static_cast<unsigned char*>(p)+88);}
inline std::uint16_t* parameter_size(void* p){return reinterpret_cast<std::uint16_t*>(static_cast<unsigned char*>(p)+182);}
inline std::ptrdiff_t bool_position(const void* p,int index){return field<int>(p,76)+std::ptrdiff_t(index)*field<int>(p,60)+field<unsigned char>(p,121);}
inline bool get_bool(void* p,const void* object,int index){return (static_cast<const unsigned char*>(object)[bool_position(p,index)]&field<unsigned char>(p,123))!=0;}
inline void set_bool(void* p,void* object,bool value,int index){auto& byte=static_cast<unsigned char*>(object)[bool_position(p,index)];byte=static_cast<unsigned char>((byte&~field<unsigned char>(p,123))|(value?field<unsigned char>(p,122):0));}
inline bool is_a(void* object,void* expected){if(!object||!expected)return false;auto type=field<void*>(object,16);for(unsigned depth=0;type&&depth<256;++depth){if(type==expected)return true;type=field<void*>(type,64);}return false;}
}
