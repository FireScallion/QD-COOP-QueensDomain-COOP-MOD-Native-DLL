#include "qdcoop/placed_item_claim.hpp"
#include "qdcoop/placed_item_deposit.hpp"
#include <cassert>
#include <cstdio>

int main(){
    namespace P=qdcoop::placed_item;
    namespace E=qdcoop::enemy;
    namespace WP=qdcoop::world_progress;
    constexpr E::Envelope scope{1,2,3,0},foreign{1,2,4,0};
    const P::Claim request{"test_drop",UINT64_MAX,42,P::ClaimOp::request};
    E::Packet packet;
    P::Claim decoded;
    assert(P::encode_claim(scope,request,packet)&&P::decode_claim(scope,packet,decoded)&&decoded==request);
    assert(!P::decode_claim(foreign,packet,decoded));
    auto grant=request;
    grant.op=P::ClaimOp::grant;
    assert(P::encode_claim(scope,grant,packet)&&P::decode_claim(scope,packet,decoded)&&decoded==grant);
    P::Claim alias{"legacy_owner",1,42,P::ClaimOp::alias};
    assert(P::encode_claim(scope,alias,packet)&&P::decode_claim(scope,packet,decoded)&&decoded==alias);
    alias.ticket=2;
    assert(!P::encode_claim(scope,alias,packet));
    auto invalid=request;
    invalid.ticket=0;
    assert(!P::encode_claim(scope,invalid,packet));
    invalid=request;
    invalid.generation=0;
    assert(!P::encode_claim(scope,invalid,packet));
    invalid=request;
    invalid.id="invalid~id";
    assert(!P::encode_claim(scope,invalid,packet));
    invalid=request;
    invalid.id.assign(81,'a');
    assert(!P::encode_claim(scope,invalid,packet));
    assert(P::encode_claim(scope,request,packet));
    E::animation::wire::put(packet,52,std::uint32_t{6});
    assert(!P::decode_claim(scope,packet,decoded));
    assert(P::encode_claim(scope,request,packet));
    packet.data[packet.size-1]=0;
    assert(!P::decode_claim(scope,packet,decoded));
    assert(P::encode_claim(scope,request,packet));
    ++packet.size;
    assert(!P::decode_claim(scope,packet,decoded));

    constexpr std::string_view raw="test_drop~owner~room~/Game/Items/Consumables/Herb/HealingHerb.HealingHerb~"
        "/Game/RuinsLevel/Maps/Ruins_Level_00.Ruins_Level_00~1~-2~3~0";
    const std::string key=std::string(P::drop_prefix)+std::string(raw);
    std::vector<WP::NamedState> names{
        {key,1},{"QDCoopWinner~test_drop~recipient",1},{"QDCoopNativeClaim~test_drop~18446744073709551615",1}
    };
    const auto sort=[&]{std::ranges::sort(names,WP::name_less,&WP::NamedState::key);};
    sort();
    auto records=P::transfers(names);
    assert(records.size()==1&&records[0].state==P::Custody::pending&&records[0].ticket==UINT64_MAX&&
        records[0].winner=="recipient"&&records[0].save_key==key);
    assert(P::encode(records[0].drop)==raw);
    assert(P::owner_matches("OldPlayer","NativePlayer","oldplayer")&&
           !P::owner_matches("OldPlayer","DifferentNative","DifferentOld"));
    std::uint32_t generation{};
    std::string_view deposit;
    P::Drop dropped;
    assert(P::encode_deposit(scope,42,raw,packet)&&P::decode_deposit(scope,packet,generation,deposit,dropped)&&
           generation==42&&deposit==raw&&dropped==records[0].drop);
    assert(!P::decode_deposit(foreign,packet,generation,deposit,dropped));
    E::animation::wire::put(packet,40,std::uint32_t{0});
    assert(!P::decode_deposit(scope,packet,generation,deposit,dropped));
    names.push_back({"QDCoopWinner~test_drop~other",1});
    sort();
    assert(P::transfers(names).empty());
    // Remove both current winner entries, then restore one valid winner.
    std::erase_if(names,[](const auto& state){return P::prefix(state.key,P::winner_prefix);});
    names.push_back({"QDCoopWinner~test_drop~recipient",1});
    names.push_back({"QDCoopNativeClaim~test_drop~0123",1});
    sort();
    assert(P::transfers(names).empty());
    std::erase_if(names,[](const auto& state){return state.key=="QDCoopNativeClaim~test_drop~0123";});
    names.push_back({key+"0",0});
    sort();
    assert(P::transfers(names).empty());
    for(auto& state:names)if(state.key==key+"0")state.value=2;
    assert(P::transfers(names).empty());
    std::erase_if(names,[&](const auto& state){return state.key==key+"0";});
    for(auto& state:names)if(state.key==key)state.value=0;
    sort();
    records=P::transfers(names);
    assert(records.size()==1&&records[0].state==P::Custody::ground&&records[0].ticket==UINT64_MAX&&records[0].winner=="recipient");
    for(auto& state:names)if(state.key==key)state.value=2;
    sort();
    assert(P::transfers(names).empty());
    std::uint64_t ticket{};
    for(const auto text:{"0","-1","01","18446744073709551616","3x"})
        assert(!P::decode_ticket(text,ticket));
    assert(P::decode_ticket("42",ticket)&&ticket==42);
    std::puts("PASS native placed item claim scope, bounded IDs, canonical tickets and ambiguous custody rejection");
}
