#pragma once
#include <algorithm>
#include <string>

template<class Reader> static std::string read_native_status(Reader read){
    constexpr unsigned max_report=65536;
    const auto requested=read(nullptr,0);
    if(!requested||requested>max_report)return {};
    std::string text(std::min(requested+256,max_report),'\0');
    const auto written=read(text.data(),static_cast<unsigned>(text.size()));
    if(!written||written>text.size())return {};
    text.resize(written-1);
    return text;
}
