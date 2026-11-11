#pragma once
#include "qdcoop/world_progress.hpp"

namespace qdcoop::world_progress {
#include "qdcoop/world_personal_keys.inc"

inline bool personal(const Guid& key){return std::ranges::binary_search(personal_guids,key);}
inline bool personal(std::string_view key){
    constexpr std::string_view prefixes[]{"QDCoopPersonal~","QDCOOPNativeRescueV1_","TotalAchievement","ReadNote_"};
    constexpr std::string_view exact[]{"ShopAmountSpent","ShopSessionSpend","TiniScytheHammerAP","CompletedChallenges"};
    constexpr std::string_view suffix="GuideGranted";
    return std::ranges::any_of(personal_names,[&](auto name){return name_equal(key,name);})||
           std::ranges::any_of(exact,[&](auto name){return name_equal(key,name);})||
           std::ranges::any_of(prefixes,[&](auto prefix){return name_equal(key.substr(0,prefix.size()),prefix);})||
           (key.size()>=suffix.size()&&name_equal(key.substr(key.size()-suffix.size()),suffix));
}
}
