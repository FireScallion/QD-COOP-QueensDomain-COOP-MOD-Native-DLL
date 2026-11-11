#pragma once
#include "qdcoop/rescue_ledger.hpp"
#include "qdcoop/enemy_enrollment.hpp"

namespace qdcoop::rescue {
inline constexpr unsigned wire_kind=15,wire_size=112;
enum class Op : std::uint32_t { offer=1, ready, debit, cancel, applied, rejected, ack };
struct Message {
    Attempt attempt;
    Op op{};
};
struct BindingView {
    Id identity;
    std::uint32_t phase{},outgoing{},bind_check{};
    std::uint64_t generation{},debits{},revives{},refunds{},errors{},ready_sent{};
};
inline bool from_donor(Op op){return op==Op::offer||op==Op::debit||op==Op::cancel||op==Op::ack;}
inline bool encode_message(const enemy::Envelope& room,const Message& message,enemy::Packet& packet){
    const auto& a=message.attempt;
    if(!a.valid()||message.op<Op::offer||message.op>Op::ack)return false;
    enemy::enrollment::write_header(room,wire_kind,wire_size,packet);
    using enemy::animation::wire::put;
    put(packet,40,a.donor.high);put(packet,48,a.donor.low);
    put(packet,56,a.target.high);put(packet,64,a.target.low);
    put(packet,72,a.serial);put(packet,80,a.life);
    put(packet,88,a.item);put(packet,92,static_cast<std::uint32_t>(message.op));
    put(packet,96,a.donor_account);put(packet,104,a.target_account);
    return true;
}
inline bool decode_message(const enemy::Envelope& room,const enemy::Packet& packet,Message& message){
    if(!enemy::enrollment::check_header(room,packet,wire_kind,wire_size))return false;
    using enemy::animation::wire::get;
    const Message value{{{get<std::uint64_t>(packet,40),get<std::uint64_t>(packet,48)},
                         {get<std::uint64_t>(packet,56),get<std::uint64_t>(packet,64)},
                         get<std::uint64_t>(packet,72),get<std::uint64_t>(packet,80),get<std::uint32_t>(packet,88),
                         get<std::uint64_t>(packet,96),get<std::uint64_t>(packet,104)},
                        static_cast<Op>(get<std::uint32_t>(packet,92))};
    if(!value.attempt.valid()||value.op<Op::offer||value.op>Op::ack)return false;
    message=value;
    return true;
}
}
