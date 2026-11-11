#include "qdcoop/enemy_damage.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace E=qdcoop::enemy;
namespace H=E::damage;
static void check(bool condition,const char* message){
    if(condition)return;
    std::fprintf(stderr,"FAIL %s\n",message);
    std::exit(1);
}
int main(){
    H::Value source{4.25f,{0,42,0x80000000u,0xffffffffu},
        {"Damage.Tags.Blockable","Damage.Tags.DamageTypes.Slashing"},
        {{"Damage.Data.Response.DamageMultiplier",-1.25f},{"Damage.Data.StaggerDamage",7.5f}}};
    H::Bytes bytes;
    H::Value decoded;
    check(H::encode(source,bytes)&&H::decode(bytes,decoded)&&decoded==source,"typed damage round trip");
    check(!H::decode(std::span(bytes).first(bytes.size()-1),decoded)&&decoded==source,"truncated damage keeps result");
    auto invalid=source;
    invalid.tags.insert(invalid.tags.begin()+1,"Damage.Tags.bLOCKABLE");
    check(!H::encode(invalid,bytes),"FName case-insensitive duplicates rejected");
    invalid=source;
    invalid.data[0].value=std::numeric_limits<float>::infinity();
    check(!H::encode(invalid,bytes),"infinite data rejected");
    invalid=source;
    invalid.amount=0;
    check(!H::encode(invalid,bytes),"zero damage rejected");
    source.amount=.0001f;
    for(unsigned i=0;i<20;++i){
        const auto name="Damage.Tags.Fixture"+std::to_string(i)+std::string(135,'X');
        source.tags.push_back(name);
    }
    std::ranges::sort(source.tags,H::tag_less);
    check(H::encode(source,bytes)&&H::count(static_cast<unsigned>(bytes.size()))>=3,"large payload fragmentation");
    const E::Envelope scope{11,22,33};
    const H::Key key{7,3,9,2};
    const H::Header header{key,{10,-20,30},H::digest(bytes),static_cast<unsigned>(bytes.size()),true};
    H::Assembly assembly{header,{},0};
    E::Packet packet{};
    H::Part part{};
    for(unsigned i=H::count(header.bytes);i-->0;){
        const auto offset=i*H::part_size;
        const auto payload=std::span(bytes).subspan(offset,std::min(H::part_size,header.bytes-offset));
        check(H::encode(scope,{header,i,payload},packet)&&packet.size<=1200,"bounded transport packet");
        check(!H::decode({11,22,34},packet,part),"stale epoch rejected");
        check(H::decode(scope,packet,part)&&assembly.push(part)&&assembly.push(part),"reordered and duplicated fragment");
        packet.data[H::hit_header]^=1;
        check(H::decode(scope,packet,part)&&!assembly.push(part),"conflicting duplicate rejected");
    }
    check(assembly.complete()&&assembly.value(decoded)&&decoded==source,"assembled damage verified");
    assembly.bytes[0]^=1;
    check(!assembly.value(decoded),"body digest mismatch rejected");
    H::Key ack{};
    check(H::encode_ack(scope,key,packet)&&H::decode_ack(scope,packet,ack)&&ack==key,"ack carries source and target generations");
    check(!H::decode_ack({11,23,33},packet,ack),"cross-room ack rejected");
    H::ReplayWindow replay{};
    replay.accept(9);
    replay.accept(7);
    replay.accept(9);
    check(replay.known(7)&&!replay.known(8)&&replay.known(9),"out-of-order replay bookkeeping");
    replay.accept(73);
    check(replay.known(9)&&!replay.known(10)&&replay.known(73)&&!replay.known(74),"too-old serial cannot replay");
    std::puts("Native enemy damage protocol verified");
}
