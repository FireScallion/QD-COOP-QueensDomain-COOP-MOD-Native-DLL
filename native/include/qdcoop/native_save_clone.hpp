#pragma once
#include "qdcoop/object_lookup.hpp"
#include "qdcoop/reflection_view.hpp"
#include <array>
#include <string>
#include <string_view>

namespace qdcoop::native_save_clone {
using Address=std::uintptr_t;
using CopyValue=void(*)(const void*,void*,const void*,int);
struct Member {
    Address property{};
    unsigned offset{};
    CopyValue copy{};
};
struct Layout {
    Address type{};
    std::array<Member,92> members{};
};

template<class Reader>bool bind(Address image,Address pool,Address type,Reader read,Layout& out){
    struct Shape {std::string_view name;int size;Address copy;};
    constexpr Shape shapes[]{
        {"ArrayProperty",16,0x116fa40},{"BoolProperty",1,0x116fd80},{"FloatProperty",4,0x116f750},
        {"IntProperty",4,0x116f720},{"MapProperty",80,0x116fde0},{"NameProperty",8,0x116f870},
        {"ObjectProperty",8,0x116f870},{"SetProperty",80,0x1170110},{"StructProperty",8,0x118e4b0},
        {"StructProperty",32,0x118e4b0},{"StructProperty",48,0x118e4b0},{"TextProperty",24,0x119dad0}
    };
    constexpr std::array paths{"/Game/Blueprints/SaveSystem/FS_SaveSlot.FS_SaveSlot_C",
                               "/Script/QueensDomain.FS_SaveGame","/Script/Engine.SaveGame"};
    constexpr std::array sizes{3068,704,40};
    Layout result{};
    result.type=type;
    unsigned count{};
    bool delegate_skipped{};
    for(unsigned level=0;level<paths.size();++level){
        std::string path;
        int size{};
        Address field{};
        if(!type||!ue427::object_path(type,pool,read,path)||path!=paths[level]||
           !read(type+88,&size,4)||size!=sizes[level]||!read(type+80,&field,8))return false;
        if(level==2){
            if(field)return false;
            break;
        }
        int end=sizes[level+1];
        while(field){
            Address field_type{},vtable{},copy{},next{};
            ue427::NameKey key{},type_name{};
            int offset{},width{},dimension{};
            std::string kind;
            if(!read(field,&vtable,8)||!vtable||!read(vtable+23*8,&copy,8)||
               !read(field+8,&field_type,8)||!field_type||!read(field_type,&type_name,8)||
               !ue427::name_string(pool,type_name,read,kind)||!read(field+40,&key,8)||
               !read(field+76,&offset,4)||!read(field+60,&width,4)||!read(field+56,&dimension,4)||
               dimension!=1||width<=0||offset<end||offset>size-width||!read(field+32,&next,8))return false;
            end=offset+width;
            if(kind=="MulticastInlineDelegateProperty"){
                std::string name;
                if(delegate_skipped||level!=1||offset!=360||width!=16||copy!=image+0x116f950||
                   !ue427::name_string(pool,key,read,name)||name!="ItemChangedEvent")return false;
                delegate_skipped=true;
            }else{
                bool known{};
                for(const auto& shape:shapes)if(kind==shape.name&&width==shape.size&&copy==image+shape.copy){
                    known=true;
                    break;
                }
                if(!known||count==result.members.size())return false;
                result.members[count++]={field,static_cast<unsigned>(offset),reinterpret_cast<CopyValue>(copy)};
            }
            field=next;
        }
        if(!read(type+64,&type,8))return false;
    }
    if(count!=result.members.size()||!delegate_skipped)return false;
    out=result;
    return true;
}

inline bool copy(const Layout& layout,void* destination,const void* source){
    if(!layout.type||!destination||!source||destination==source||
       ue427::field<Address>(destination,16)!=layout.type||ue427::field<Address>(source,16)!=layout.type)return false;
    for(const auto& member:layout.members){
        member.copy(reinterpret_cast<const void*>(member.property),static_cast<unsigned char*>(destination)+member.offset,
                    static_cast<const unsigned char*>(source)+member.offset,1);
    }
    return true;
}
}
