#pragma once
#include "qdcoop/reward_journal.hpp"
#include "qdcoop/session.hpp"
#include "qdcoop/steam_room.hpp"
#include <bit>
#include <limits>

namespace qdcoop::rescue {
using enemy::reward::journal::SaveProof;
inline constexpr unsigned medicines=6, receipt_capacity=16, donor_capacity=64;
inline constexpr std::int32_t last_revision=std::numeric_limits<std::int32_t>::max();

struct Attempt {
    Id donor{},target{};
    std::uint64_t serial{},life{};
    std::uint32_t item{};
    std::uint64_t donor_account{},target_account{};
    bool operator==(const Attempt&) const = default;
    bool valid() const {
        const bool accounts=(!donor_account&&!target_account)||
            (steam_room::user_id(donor_account)&&steam_room::user_id(target_account)&&donor_account!=target_account);
        return donor.valid()&&target.valid()&&donor!=target&&serial&&life&&item>=1&&item<=medicines&&accounts;
    }
};
enum class Debit : std::uint32_t { none, offered, cancelled, intended, persisted, refunding };
enum class Receipt : std::uint32_t { none, prepared, applying, applied, rejected };
struct Inventory {
    std::int32_t revision{},quantity{},uses{};
    bool operator==(const Inventory&) const = default;
};
struct Outgoing {
    Attempt attempt;
    SaveProof proof;
    Debit phase{};
    bool operator==(const Outgoing&) const = default;
};
struct Incoming {
    Attempt attempt;
    SaveProof proof;
    Receipt phase{};
    bool operator==(const Incoming&) const = default;
};
struct Watermark {
    Id donor;
    std::uint64_t serial{};
    bool operator==(const Watermark&) const = default;
};
struct Ledger {
    Id identity;
    SaveProof anchor;
    std::uint64_t generation{},serial{};
    std::array<Inventory,medicines> inventory{};
    Outgoing outgoing;
    std::array<Incoming,receipt_capacity> incoming{};
    std::array<Watermark,donor_capacity> acknowledged{};
    bool operator==(const Ledger&) const = default;

    const Incoming* find(const Attempt& attempt) const {
        for(const auto& record:incoming)
            if(record.phase!=Receipt::none&&record.attempt.donor==attempt.donor&&
               record.attempt.serial==attempt.serial)return &record;
        return nullptr;
    }
    const Watermark* watermark(Id donor) const {
        for(const auto& record:acknowledged)if(record.donor==donor)return &record;
        return nullptr;
    }
    bool valid() const {
        if(!identity.valid()||!anchor.valid()||!generation)return false;
        for(const auto& value:inventory)
            if(value.revision<0||value.quantity<0||value.uses<0||value.uses>value.revision||
               (!value.revision&&value.quantity))return false;
        if(outgoing.phase==Debit::none){
            if(outgoing!=Outgoing{})return false;
        }else{
            const auto& attempt=outgoing.attempt;
            if(outgoing.phase>Debit::refunding||!attempt.valid()||attempt.donor!=identity||
               attempt.serial!=serial)return false;
            if(outgoing.phase>=Debit::intended&&!inventory[attempt.item-1].revision)return false;
            if(outgoing.phase<Debit::persisted?outgoing.proof!=SaveProof{}:!outgoing.proof.valid())return false;
        }
        for(const auto& entry:acknowledged)
            if(entry.donor.valid()?(entry.donor==identity||!entry.serial||watermark(entry.donor)!=&entry):
                                  entry.serial!=0)return false;
        for(const auto& record:incoming){
            if(record.phase==Receipt::none){
                if(record!=Incoming{})return false;
                continue;
            }
            if(record.phase>Receipt::rejected||!record.attempt.valid()||record.attempt.target!=identity||
               find(record.attempt)!=&record)return false;
            const auto previous=watermark(record.attempt.donor);
            if(previous&&record.attempt.serial<=previous->serial)return false;
            if(record.phase==Receipt::applied?!record.proof.valid():record.proof!=SaveProof{})return false;
        }
        return true;
    }

    bool offer(const Attempt& attempt){
        if(outgoing.phase!=Debit::none||!attempt.valid()||attempt.donor!=identity||
           serial==UINT64_MAX||attempt.serial!=serial+1)return false;
        serial=attempt.serial;
        outgoing={attempt,{},Debit::offered};
        return true;
    }
    bool cancel(const Attempt& attempt){
        if(outgoing.phase!=Debit::offered||outgoing.attempt!=attempt)return false;
        outgoing.phase=Debit::cancelled;
        return true;
    }
    bool intend(const Attempt& attempt,int quantity){
        if(outgoing.phase!=Debit::offered||outgoing.attempt!=attempt||quantity<=0)return false;
        auto& item=inventory[attempt.item-1];
        if(item.revision>=last_revision-1)return false;
        item={item.revision+1,quantity-1,item.uses+1};
        outgoing.phase=Debit::intended;
        return true;
    }
    bool debit_saved(const Attempt& attempt,const SaveProof& proof){
        if(outgoing.phase!=Debit::intended||outgoing.attempt!=attempt||!proof.valid())return false;
        outgoing.proof=proof;
        outgoing.phase=Debit::persisted;
        return true;
    }
    bool settle(const Attempt& attempt,Receipt result,int quantity){
        if(outgoing.attempt!=attempt)return false;
        if(result==Receipt::rejected&&(outgoing.phase==Debit::offered||outgoing.phase==Debit::cancelled)){
            outgoing={};
            return true;
        }
        if(outgoing.phase!=Debit::persisted)return false;
        if(result==Receipt::applied){outgoing={};return true;}
        if(result!=Receipt::rejected||quantity<0||quantity==last_revision)return false;
        auto& item=inventory[attempt.item-1];
        if(item.revision==last_revision)return false;
        item={item.revision+1,quantity+1,item.uses};
        outgoing.phase=Debit::refunding;
        return true;
    }
    bool refund_saved(const Attempt& attempt){
        if(outgoing.phase!=Debit::refunding||outgoing.attempt!=attempt)return false;
        outgoing={};
        return true;
    }
    bool prepare(const Attempt& attempt){
        if(!attempt.valid()||attempt.target!=identity||find(attempt))return false;
        const auto previous=watermark(attempt.donor);
        if(previous&&attempt.serial<=previous->serial)return false;
        for(const auto& record:incoming)
            if(record.phase!=Receipt::none&&(record.attempt.donor==attempt.donor||
               ((record.phase==Receipt::prepared||record.phase==Receipt::applying)&&
                record.attempt.life==attempt.life)))return false;
        for(auto& record:incoming)if(record.phase==Receipt::none){
            record={attempt,{},Receipt::prepared};
            return true;
        }
        return false;
    }
    bool receive(const Attempt& attempt,Receipt next,const SaveProof& proof={}){
        for(auto& record:incoming){
            if(record.attempt!=attempt)continue;
            if(next==Receipt::applied){
                if(record.phase!=Receipt::applying||!proof.valid())return false;
            }else if(next==Receipt::applying){
                if(record.phase!=Receipt::prepared||proof!=SaveProof{})return false;
            }else if(next==Receipt::rejected){
                if((record.phase!=Receipt::prepared&&record.phase!=Receipt::applying)||proof!=SaveProof{})return false;
            }else return false;
            record.phase=next;
            record.proof=proof;
            return true;
        }
        return false;
    }
    bool acknowledge(const Attempt& attempt){
        for(auto& record:incoming){
            if(record.attempt!=attempt||(record.phase!=Receipt::applied&&record.phase!=Receipt::rejected))continue;
            Watermark* mark=nullptr;
            for(auto& entry:acknowledged){
                if(entry.donor==attempt.donor){mark=&entry;break;}
                if(!entry.donor.valid()&&!mark)mark=&entry;
            }
            if(!mark)return false;
            *mark={attempt.donor,attempt.serial};
            record={};
            return true;
        }
        return false;
    }
};

// Fixed little-endian bytes. No object layout, handles, or text from packets.
static_assert(std::endian::native==std::endian::little);
inline constexpr unsigned blob_size=80+medicines*12+(1+receipt_capacity)*112+donor_capacity*24+4;
using Blob=std::array<std::uint8_t,blob_size>;
inline std::uint32_t ledger_crc(const Blob& blob){
    std::uint32_t value=~0u;
    for(unsigned i=0;i<blob.size()-4;++i){
        value^=blob[i];
        for(unsigned bit=0;bit<8;++bit)value=(value>>1)^(0xedb88320u&(0u-(value&1u)));
    }
    return ~value;
}
template<class Io,class Record>void ledger_record(Io& io,Record& record){
    auto& attempt=record.attempt;
    io(attempt.donor.high);io(attempt.donor.low);io(attempt.target.high);io(attempt.target.low);
    io(attempt.serial);io(attempt.life);io(attempt.item);io(record.phase);
    io(attempt.donor_account);io(attempt.target_account);
    io(record.proof.bytes);io(record.proof.sha256);
}
template<class Io,class Value>void ledger_fields(Io& io,Value& value){
    io(value.generation);io(value.identity.high);io(value.identity.low);
    io(value.anchor.bytes);io(value.anchor.sha256);io(value.serial);
    for(auto& item:value.inventory){io(item.revision);io(item.quantity);io(item.uses);}
    ledger_record(io,value.outgoing);
    for(auto& record:value.incoming)ledger_record(io,record);
    for(auto& record:value.acknowledged){io(record.donor.high);io(record.donor.low);io(record.serial);}
}
inline bool encode_ledger(const Ledger& value,Blob& blob){
    if(!value.valid())return false;
    unsigned offset{};
    const auto put=[&](const auto& field){
        std::memcpy(blob.data()+offset,&field,sizeof(field));
        offset+=sizeof(field);
    };
    put(std::uint32_t{0x4d524451});put(std::uint32_t{1});
    ledger_fields(put,value);
    put(ledger_crc(blob));
    return offset==blob.size();
}
inline bool decode_ledger(const Blob& blob,Ledger& value){
    unsigned offset{};
    const auto get=[&](auto& field){
        std::memcpy(&field,blob.data()+offset,sizeof(field));
        offset+=sizeof(field);
    };
    std::uint32_t magic{},version{},crc{};
    get(magic);get(version);
    if(magic!=0x4d524451||version!=1)return false;
    Ledger next{};
    ledger_fields(get,next);get(crc);
    if(offset!=blob.size()||crc!=ledger_crc(blob)||!next.valid())return false;
    value=next;
    return true;
}
}
