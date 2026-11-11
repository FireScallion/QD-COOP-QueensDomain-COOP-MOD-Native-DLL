#pragma once
#include "qdcoop/object_count_map.hpp"
#include <type_traits>
#include <vector>

namespace qdcoop::world_progress {
template<class Pair,unsigned stride,class Reader>
bool read_sparse_map(std::uintptr_t address,Reader read,std::vector<Pair>& out){
    static_assert(sizeof(Pair)<stride&&std::is_trivially_copyable_v<Pair>);
    using qdcoop::ue427::scalar;
    std::array<unsigned char,80> header{},after{};
    if(!address||!read(address,header.data(),header.size()))return false;
    const auto data=scalar<std::uintptr_t>(header.data(),0),heap=scalar<std::uintptr_t>(header.data(),32);
    const auto count=scalar<int>(header.data(),8),capacity=scalar<int>(header.data(),12);
    const auto bits=scalar<int>(header.data(),40),bit_capacity=scalar<int>(header.data(),44);
    const auto free=scalar<int>(header.data(),48),free_count=scalar<int>(header.data(),52);
    constexpr unsigned max_sparse=16384;
    if(count<0||static_cast<unsigned>(count)>max_sparse||capacity<count||capacity>65536||bits!=count||
       bit_capacity<count||bit_capacity>65536||free_count<0||free_count>count||
       (!heap&&bit_capacity>128)||(count&&!data)||
       (free_count?(free<0||free>=count):free!=-1)||
       data>std::numeric_limits<std::uintptr_t>::max()-static_cast<unsigned>(count)*stride)return false;
    std::array<std::uint32_t,max_sparse/32> live{};
    const auto words=(static_cast<unsigned>(count)+31)/32;
    if(words){
        if(heap){if(!read(heap,live.data(),words*4))return false;}
        else std::memcpy(live.data(),header.data()+16,words*4);
    }
    out.clear();
    out.reserve(count-free_count);
    for(unsigned i=0;i<static_cast<unsigned>(count);++i){
        if(!(live[i/32]&(1u<<(i%32))))continue;
        Pair entry{};
        if(!read(data+i*stride,&entry,sizeof(entry)))return false;
        out.push_back(entry);
    }
    return out.size()==static_cast<unsigned>(count-free_count)&&read(address,after.data(),after.size())&&header==after;
}
}
