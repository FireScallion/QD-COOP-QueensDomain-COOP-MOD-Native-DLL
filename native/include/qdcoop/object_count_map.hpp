#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
namespace qdcoop::ue427 {
// Read-only object -> int32 map view for the pinned Windows game build.
// Layout is checked against FMapProperty::GetMapLayout, not assumed from size.
// Caller supplies a bounded memory reader and owns the game thread.
template<class T>inline T scalar(const unsigned char* data,unsigned offset){T v{};std::memcpy(&v,data+offset,sizeof(v));return v;}
inline bool object_int_layout(const std::array<std::int32_t,6>& shape){return shape==std::array<std::int32_t,6>{8,16,20,24,8,24};}
template<class Reader>bool object_count(std::uintptr_t map,std::uintptr_t key,const std::array<std::int32_t,6>& shape,Reader read,std::int32_t& output){
 if(!map||!key||!object_int_layout(shape))return false;
 std::array<unsigned char,80> h{},after{};if(!read(map,h.data(),h.size()))return false;
 const auto data=scalar<std::uintptr_t>(h.data(),0),heap=scalar<std::uintptr_t>(h.data(),32);
 const auto n=scalar<std::int32_t>(h.data(),8),capacity=scalar<std::int32_t>(h.data(),12),bits=scalar<std::int32_t>(h.data(),40),bit_capacity=scalar<std::int32_t>(h.data(),44),free=scalar<std::int32_t>(h.data(),48),free_count=scalar<std::int32_t>(h.data(),52);
 if(n<0||n>4096||capacity<n||capacity>65536||bits!=n||bit_capacity<n||bit_capacity>65536||free_count<0||free_count>n||(!heap&&bit_capacity>128)||(n&&!data))return false;
 if((free_count==0&&free!=-1)||(free_count>0&&(free<0||free>=n)))return false;
 if(data>std::numeric_limits<std::uintptr_t>::max()-static_cast<unsigned>(n)*24u)return false;
 const auto word_count=(static_cast<unsigned>(n)+31u)/32u;
 std::array<std::uint32_t,128> flags{};
 if(word_count){if(heap){if(!read(heap,flags.data(),word_count*4))return false;}else std::memcpy(flags.data(),h.data()+16,word_count*4);}
 unsigned live=0;bool found=false;std::int32_t value=0;
 for(unsigned i=0;i<static_cast<unsigned>(n);++i){if(!(flags[i/32]&(1u<<(i%32))))continue;++live;std::array<unsigned char,12> entry{};if(!read(data+i*24u,entry.data(),entry.size()))return false;
  if(scalar<std::uintptr_t>(entry.data(),0)==key){if(found)return false;found=true;value=scalar<std::int32_t>(entry.data(),8);if(value<0)return false;}
 }
 if(live!=static_cast<unsigned>(n-free_count)||!read(map,after.data(),after.size())||after!=h)return false;
 output=value;return true;
}
template<class Reader>bool object_bool(std::uintptr_t map,std::uintptr_t key,const std::array<std::int32_t,6>& shape,Reader read,bool& output){
 if(!map||!key||!object_int_layout(shape))return false;
 std::array<unsigned char,80> h{},after{};if(!read(map,h.data(),h.size()))return false;
 const auto data=scalar<std::uintptr_t>(h.data(),0),heap=scalar<std::uintptr_t>(h.data(),32);
 const auto n=scalar<std::int32_t>(h.data(),8),capacity=scalar<std::int32_t>(h.data(),12),bits=scalar<std::int32_t>(h.data(),40),bit_capacity=scalar<std::int32_t>(h.data(),44),free=scalar<std::int32_t>(h.data(),48),free_count=scalar<std::int32_t>(h.data(),52);
 if(n<0||n>4096||capacity<n||capacity>65536||bits!=n||bit_capacity<n||bit_capacity>65536||free_count<0||free_count>n||(!heap&&bit_capacity>128)||(n&&!data))return false;
 if((free_count==0&&free!=-1)||(free_count>0&&(free<0||free>=n)))return false;
 if(data>std::numeric_limits<std::uintptr_t>::max()-static_cast<unsigned>(n)*24u)return false;
 const auto word_count=(static_cast<unsigned>(n)+31u)/32u;
 std::array<std::uint32_t,128> flags{};
 if(word_count){if(heap){if(!read(heap,flags.data(),word_count*4))return false;}else std::memcpy(flags.data(),h.data()+16,word_count*4);}
 unsigned live=0;bool found=false;bool value=false;
 for(unsigned i=0;i<static_cast<unsigned>(n);++i){if(!(flags[i/32]&(1u<<(i%32))))continue;++live;std::array<unsigned char,12> entry{};if(!read(data+i*24u,entry.data(),entry.size()))return false;
  if(scalar<std::uintptr_t>(entry.data(),0)==key){if(found)return false;found=true;const auto raw=entry[8];if(raw>1)return false;value=raw!=0;}
 }
 if(live!=static_cast<unsigned>(n-free_count)||!read(map,after.data(),after.size())||after!=h)return false;
 output=value;return true;
}

}
