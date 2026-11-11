#pragma once
#include "name_pool_view.hpp"
#include <array>
#include <string_view>
namespace qdcoop::ue427 {
template<class Reader>bool object_path(std::uintptr_t object,std::uintptr_t pool,Reader read,std::string& out){
 out.clear();std::array<std::string,64> parts{};std::array<std::uintptr_t,64> visited{};unsigned count=0,total=0;
 while(object){if(count==parts.size()||object>std::numeric_limits<std::uintptr_t>::max()-40)return false;for(unsigned i=0;i<count;++i)if(visited[i]==object)return false;visited[count]=object;NameKey name{};if(!read(object+24,&name,8)||!name_string(pool,name,read,parts[count]))return false;total+=static_cast<unsigned>(parts[count].size()+1);if(total>8192)return false;++count;if(!read(object+32,&object,8))return false;}
 if(!count){return false;}while(count){if(!out.empty())out+='.';out+=parts[--count];}return true;
}
// Resolve the owning UStruct first; traversal must remain on the game thread.
template<class Reader>std::uintptr_t find_property(std::uintptr_t owner,std::uintptr_t pool,std::string_view wanted,Reader read){
 if(!owner||owner>std::numeric_limits<std::uintptr_t>::max()-88){return 0;}std::uintptr_t field{};if(!read(owner+80,&field,8))return 0;
 for(unsigned count=0;field&&count<4096;++count){if(field>std::numeric_limits<std::uintptr_t>::max()-48)return 0;NameKey key{};std::string name;if(!read(field+40,&key,8)||!name_string(pool,key,read,name))return 0;if(name==wanted)return field;std::uintptr_t next{};if(!read(field+32,&next,8)||next==field)return 0;field=next;}return 0;
}
template<class Reader>std::uintptr_t find_inherited_property(std::uintptr_t owner,std::uintptr_t pool,std::string_view wanted,Reader read){
 std::array<std::uintptr_t,256> visited{};for(unsigned count=0;owner&&count<visited.size();++count){if(owner>std::numeric_limits<std::uintptr_t>::max()-88)return 0;for(unsigned i=0;i<count;++i)if(visited[i]==owner)return 0;visited[count]=owner;if(auto field=find_property(owner,pool,wanted,read))return field;if(!read(owner+64,&owner,8))return 0;}return 0;
}}
