#pragma once
#include "session.hpp"

namespace qdcoop {
struct TransactionId {
    Id session{};
    std::uint64_t sequence{};
    bool operator==(const TransactionId&) const = default;
};
// Trusted adapter evidence, not a claim accepted directly from a remote packet.
// A future persistence adapter must verify immutable archive bytes and flush its
// commit record before providing this receipt. A network ACK is not evidence.
struct DurableReceipt {
    Id digest{};
    std::uint64_t bytes{};
    bool valid() const { return digest.valid() && bytes != 0; }
    bool operator==(const DurableReceipt&) const = default;
};
enum class CheckpointPhase { idle, preparing, prepared, committed, aborted };
struct CheckpointRecord {
    TransactionId id{};
    std::uint64_t world{}, world_revision{}, membership_revision{};
    std::array<MemberRef, max_players> participants{};
    std::array<DurableReceipt, max_players> saves{};
    DurableReceipt commit{};
    std::size_t count{};
    CheckpointPhase phase{};
};
class Checkpoints {
public:
    Result begin(const Session& session, MemberRef requester, std::uint64_t world_revision);
    Result saved(const Session& session, TransactionId id, MemberRef member, DurableReceipt receipt);
    Result commit(const Session& session, TransactionId id, DurableReceipt journal);
    Result reconcile(const Session& session);
    Result abort(TransactionId id);
    const CheckpointRecord& pending() const { return pending_; }
    const CheckpointRecord& latest() const { return latest_; }
private:
    CheckpointRecord pending_{}, latest_{};
    std::uint64_t next_{1};
};

enum class RescuePhase { empty, reserved, intent_durable, debit_durable, completed, cancelled, recovery_required };
struct RescueRecord {
    TransactionId id{};
    MemberRef rescuer{}, target{};
    std::uint64_t world{}, down_generation{};
    std::uint32_t item{}; // 0 = bare hands (1 HP); 1..6 = existing medicine codes
    RescuePhase phase{};
    DurableReceipt intent{}, debit{}, applied{};
};
struct RescueResult { Result result; TransactionId id; };
// Reserves at completion of the E hold, after the gameplay adapter validates
// range, aim, uninterrupted charge, and the currently selected medicine.
class Rescues {
public:
    RescueResult reserve(const Session& session, MemberRef rescuer, MemberRef target,
                         std::uint64_t down_generation, std::uint32_t item);
    Result intent_durable(const Session& session, TransactionId id, DurableReceipt receipt);
    Result debit_durable(const Session& session, TransactionId id, DurableReceipt receipt);
    Result applied(Session& session, TransactionId id, DurableReceipt receipt);
    Result cancel(TransactionId id);
    void reconcile(const Session& session);
    const RescueRecord* find(TransactionId id) const;
    const std::array<RescueRecord, max_players>& records() const { return records_; }
private:
    RescueRecord* mutable_record(TransactionId id);
    bool valid_participants(const Session& session, const RescueRecord& record) const;
    std::array<RescueRecord, max_players> records_{};
    std::uint64_t next_{1};
};

// Only invoke after an accepted new DOWN transition, never from leave/join or
// a periodic poll. Restore completion, not timeout, releases the latch.
class WipeGate {
public:
    Result on_downed(const Session& session, const Checkpoints& checkpoints,
                     MemberRef changed, std::uint64_t generation);
    Result restored(TransactionId checkpoint);
    TransactionId checkpoint() const { return checkpoint_; }
private:
    bool pending_{};
    TransactionId checkpoint_{};
    struct Seen { MemberRef ref{}; std::uint64_t generation{}, world{}; };
    std::array<Seen, max_players> seen_{};
};
}
