#pragma once
#include "qdcoop/placed_item_record.hpp"

namespace qdcoop::placed_item {
namespace E=enemy;
inline constexpr unsigned claim_kind=30,claim_header=56;
enum class ClaimOp : std::uint32_t { request=1, received, refused, grant, alias };
struct Claim {
    std::string id;
    std::uint64_t ticket{};
    std::uint32_t generation{};
    ClaimOp op{};
    bool operator==(const Claim&) const=default;
};

inline bool valid(const Claim& claim){
    return claim.ticket&&claim.generation&&valid_id(claim.id)&&
        claim.op>=ClaimOp::request&&claim.op<=ClaimOp::alias&&(claim.op!=ClaimOp::alias||claim.ticket==1);
}

inline bool encode_claim(const E::Envelope& scope,const Claim& claim,E::Packet& packet){
    if(!scope.world||!valid(claim))return false;
    const auto size=static_cast<unsigned>(claim.id.size());
    E::enrollment::write_header(scope,claim_kind,claim_header+size,packet);
    using E::animation::wire::put;
    put(packet,40,claim.ticket);
    put(packet,48,claim.generation);
    put(packet,52,static_cast<std::uint32_t>(claim.op));
    std::memcpy(packet.data.data()+claim_header,claim.id.data(),size);
    return true;
}

inline bool decode_claim(const E::Envelope& scope,const E::Packet& packet,Claim& out){
    if(!scope.world||packet.size<=claim_header||packet.size>claim_header+max_item_id||
       !E::enrollment::check_header(scope,packet,claim_kind,packet.size))return false;
    using E::animation::wire::get;
    Claim claim{std::string(reinterpret_cast<const char*>(packet.data.data()+claim_header),packet.size-claim_header),
        get<std::uint64_t>(packet,40),get<std::uint32_t>(packet,48),static_cast<ClaimOp>(get<std::uint32_t>(packet,52))};
    if(!valid(claim))return false;
    out=std::move(claim);
    return true;
}
}
