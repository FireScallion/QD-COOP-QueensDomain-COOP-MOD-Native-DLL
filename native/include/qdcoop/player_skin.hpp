#pragma once
#include <array>
#include <span>
#include <string_view>

namespace qdcoop::player {
inline constexpr std::array<std::wstring_view,6> skin_names{
    L"",L"prince",L"arjaot",L"ippibi",L"ujji",L"imba"
};
constexpr bool valid_skin(unsigned skin){return skin>0&&skin<skin_names.size();}

inline bool parse_skin(std::span<wchar_t* const> arguments,unsigned& skin){
    constexpr std::wstring_view prefix=L"-QDCoopSkin=";
    unsigned chosen=0;
    for(const std::wstring_view arg:arguments){
        if(!arg.starts_with(prefix))continue;
        if(chosen)return false;
        for(unsigned id=1;id<skin_names.size();++id)
            if(arg.substr(prefix.size())==skin_names[id])chosen=id;
        if(!chosen)return false;
    }
    skin=chosen?chosen:1;
    return true;
}
}
