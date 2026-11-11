#pragma once
#include <cstdint>
#include <string>
#include <limits>
namespace qdcoop::ue427 {
struct NameKey{std::uint32_t index{},number{};};
inline void utf8_append(std::string& out,std::uint32_t c){if(c<0x80)out+=static_cast<char>(c);else if(c<0x800){out+=static_cast<char>(0xc0|(c>>6));out+=static_cast<char>(0x80|(c&63));}else if(c<0x10000){out+=static_cast<char>(0xe0|(c>>12));out+=static_cast<char>(0x80|((c>>6)&63));out+=static_cast<char>(0x80|(c&63));}else{out+=static_cast<char>(0xf0|(c>>18));out+=static_cast<char>(0x80|((c>>12)&63));out+=static_cast<char>(0x80|((c>>6)&63));out+=static_cast<char>(0x80|(c&63));}}
template<class Reader>bool name_string(std::uintptr_t pool,NameKey key,Reader read,std::string& out){
 out.clear();std::uint32_t block{},cursor{};if(!pool||pool>std::numeric_limits<std::uintptr_t>::max()-65552||!read(pool+8,&block,4)||!read(pool+12,&cursor,4)||block>=8192||cursor>131072)return false;
 const auto page=key.index>>16,offset=(key.index&65535u)*2u;if(page>block)return false;
 std::uintptr_t data{};if(!read(pool+16+page*8,&data,8)||!data||data>std::numeric_limits<std::uintptr_t>::max()-131072)return false;
 const auto end=page==block?cursor:131072u;if(offset>end||end-offset<2)return false;std::uint16_t header{};if(!read(data+offset,&header,2))return false;
 const auto length=header>>6;const auto wide=(header&1)!=0;if(!length||length>1023||unsigned(length)*(wide?2u:1u)>end-offset-2)return false;
 if(wide){std::uint16_t text[1023]{};if(!read(data+offset+2,text,length*2))return false;for(int i=0;i<length;++i){std::uint32_t c=text[i];if(c>=0xd800&&c<=0xdbff){if(++i>=length||text[i]<0xdc00||text[i]>0xdfff){out.clear();return false;}c=0x10000+((c-0xd800)<<10)+(text[i]-0xdc00);}else if(c>=0xdc00&&c<=0xdfff){out.clear();return false;}if(!c){out.clear();return false;}utf8_append(out,c);}}
 else{char text[1023]{};if(!read(data+offset+2,text,length))return false;for(int i=0;i<length;++i){if(!text[i]){out.clear();return false;}out+=text[i];}}
 if(key.number){out+='_' ;out+=std::to_string(key.number-1);}return true;
}
}
