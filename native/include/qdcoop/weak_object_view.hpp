#pragma once
#include <atomic>
#include <cstdint>
#include <cstring>
#include <limits>
namespace qdcoop::ue427 {
// Pinned Win64 layout, verified against the current Steam executable in memory.
// Caller owns the game thread; this does not make UObject access thread-safe.
struct WeakKey{std::int32_t index{-1},serial{};};
struct ObjectArray {std::uintptr_t chunks{},preallocated{};std::int32_t max_elements{},num_elements{},max_chunks{},num_chunks{};};
struct ObjectItem {std::uintptr_t object{};std::uint32_t flags{};std::int32_t cluster{},serial{},padding{};};
static_assert(sizeof(WeakKey)==8&&sizeof(ObjectArray)==32&&sizeof(ObjectItem)==24);
constexpr unsigned objects_per_chunk=65536;
inline bool object_array_shape(const ObjectArray& a){return a.chunks&&a.max_chunks>0&&a.max_chunks<=4096&&a.num_chunks>0&&a.num_chunks<=a.max_chunks&&a.num_elements>=0&&a.max_elements>=a.num_elements&&a.max_elements<=a.max_chunks*static_cast<int>(objects_per_chunk)&&a.num_elements<=a.num_chunks*static_cast<int>(objects_per_chunk);}
template<class Reader>std::uintptr_t resolve_weak(std::uintptr_t address,WeakKey weak,Reader read){
 if(!address||weak.index<0||weak.serial<=0)return 0;
 ObjectArray a{};if(!read(address,&a,sizeof(a))||!object_array_shape(a)||weak.index>=a.num_elements)return 0;
 const auto chunk=static_cast<unsigned>(weak.index)/objects_per_chunk,within=static_cast<unsigned>(weak.index)%objects_per_chunk;
 if(chunk>=static_cast<unsigned>(a.num_chunks)||a.chunks>std::numeric_limits<std::uintptr_t>::max()-chunk*8u)return 0;
 std::uintptr_t data{};if(!read(a.chunks+chunk*8u,&data,8)||!data||data>std::numeric_limits<std::uintptr_t>::max()-within*sizeof(ObjectItem)-sizeof(ObjectItem))return 0;
 const auto item=data+within*sizeof(ObjectItem);ObjectItem first{},second{};
 if(!read(item,&first,sizeof(first))||!first.object||first.serial!=weak.serial||(first.flags&0x30000000u))return 0;
 std::atomic_thread_fence(std::memory_order_acquire);
 if(!read(item,&second,sizeof(second))||second.object!=first.object||second.serial!=first.serial||(second.flags&0x30000000u))return 0;
 return second.object;
}
}
