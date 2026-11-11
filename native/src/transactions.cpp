#include "qdcoop/transactions.hpp"
#include <limits>

namespace qdcoop {
static bool checkpoint_busy(CheckpointPhase p) {
    return p == CheckpointPhase::preparing || p == CheckpointPhase::prepared;
}
Result Checkpoints::begin(const Session& session, MemberRef requester, std::uint64_t world_revision) {
    if (session.closed()) return Result::closed;
    if (!world_revision) return Result::invalid;
    if (checkpoint_busy(pending_.phase)) return Result::busy;
    const auto* source = session.find(requester);
    if (!source || source->phase != MemberPhase::active || source->down) return Result::not_ready;
    for (const auto& m : session.members()) if (m.phase == MemberPhase::loading) return Result::not_ready;
    if (next_ == std::numeric_limits<std::uint64_t>::max()) return Result::exhausted;
    pending_ = {}; pending_.id = {session.id(), next_++}; pending_.world = session.world();
    pending_.world_revision = world_revision;
    pending_.membership_revision = session.revision(); pending_.phase = CheckpointPhase::preparing;
    for (const auto& m : session.members()) if (m.phase == MemberPhase::active)
        pending_.participants[pending_.count++] = m.ref;
    return Result::ok;
}
Result Checkpoints::reconcile(const Session& session) {
    if (!checkpoint_busy(pending_.phase)) return Result::ok;
    if (session.closed() || session.id() != pending_.id.session || session.world() != pending_.world ||
        session.revision() != pending_.membership_revision) {
        pending_.phase = CheckpointPhase::aborted; return Result::stale;
    }
    return Result::ok;
}
Result Checkpoints::saved(const Session& session, TransactionId id, MemberRef member, DurableReceipt receipt) {
    if (id != pending_.id) return Result::stale;
    if (reconcile(session) != Result::ok || !checkpoint_busy(pending_.phase)) return Result::stale;
    if (!receipt.valid()) return Result::invalid;
    for (std::size_t i = 0; i < pending_.count; ++i) if (member == pending_.participants[i]) {
        auto& old = pending_.saves[i];
        if (old.valid()) return old == receipt ? Result::duplicate : Result::conflict;
        old = receipt;
        bool all = true;
        for (std::size_t j = 0; j < pending_.count; ++j) all = all && pending_.saves[j].valid();
        if (all) pending_.phase = CheckpointPhase::prepared;
        return Result::ok;
    }
    return Result::stale;
}
Result Checkpoints::commit(const Session& session, TransactionId id, DurableReceipt journal) {
    if (id != pending_.id) return Result::stale;
    if (pending_.phase == CheckpointPhase::committed)
        return journal == pending_.commit ? Result::duplicate : Result::conflict;
    if (reconcile(session) != Result::ok) return Result::stale;
    if (pending_.phase != CheckpointPhase::prepared) return Result::not_ready;
    if (!journal.valid()) return Result::invalid;
    pending_.commit = journal; pending_.phase = CheckpointPhase::committed; latest_ = pending_;
    return Result::ok;
}
Result Checkpoints::abort(TransactionId id) {
    if (id != pending_.id) return Result::stale;
    if (pending_.phase == CheckpointPhase::aborted) return Result::duplicate;
    if (!checkpoint_busy(pending_.phase)) return Result::conflict;
    pending_.phase = CheckpointPhase::aborted; return Result::ok;
}
static bool rescue_busy(RescuePhase p) {
    return p == RescuePhase::reserved || p == RescuePhase::intent_durable ||
           p == RescuePhase::debit_durable || p == RescuePhase::recovery_required;
}
RescueRecord* Rescues::mutable_record(TransactionId id) {
    if (!id.sequence || !id.session.valid()) return nullptr;
    for (auto& r : records_) if (r.phase != RescuePhase::empty && r.id == id) return &r;
    return nullptr;
}
const RescueRecord* Rescues::find(TransactionId id) const {
    if (!id.sequence || !id.session.valid()) return nullptr;
    for (const auto& r : records_) if (r.phase != RescuePhase::empty && r.id == id) return &r;
    return nullptr;
}
bool Rescues::valid_participants(const Session& session, const RescueRecord& r) const {
    const auto* donor = session.find(r.rescuer); const auto* target = session.find(r.target);
    return session.world() == r.world && donor && target && donor->phase == MemberPhase::active &&
        target->phase == MemberPhase::active && !donor->down && target->down && target->down_generation == r.down_generation;
}
RescueResult Rescues::reserve(const Session& session, MemberRef rescuer, MemberRef target,
                             std::uint64_t generation, std::uint32_t item) {
    if (session.closed()) return {Result::closed, {}};
    if (rescuer == target || !generation || item > 6) return {Result::invalid, {}};
    reconcile(session);
    RescueRecord candidate{};
    candidate.rescuer = rescuer; candidate.target = target; candidate.world = session.world();
    candidate.down_generation = generation; candidate.item = item;
    if (!valid_participants(session, candidate)) return {Result::stale, {}};
    RescueRecord* available = nullptr;
    for (auto& r : records_) {
        if (rescue_busy(r.phase)) {
            // Recovery locks stable identities across reconnect and travel.
            if (r.target.player == target.player || r.rescuer.player == rescuer.player) {
                if (r.rescuer == rescuer && r.target == target && r.world == candidate.world &&
                    r.down_generation == generation && r.item == item)
                    return {r.phase == RescuePhase::recovery_required ? Result::recovery_required : Result::duplicate, r.id};
                return {Result::busy, {}};
            }
        } else if (!available) available = &r;
    }
    if (!available) return {Result::backpressure, {}};
    if (next_ == std::numeric_limits<std::uint64_t>::max()) return {Result::exhausted, {}};
    candidate.id = {session.id(), next_++}; candidate.phase = RescuePhase::reserved; *available = candidate;
    return {Result::ok, candidate.id};
}
Result Rescues::intent_durable(const Session& session, TransactionId id, DurableReceipt receipt) {
    reconcile(session);
    auto* r = mutable_record(id);
    if (!r) return Result::stale;
    if (r->intent.valid()) return r->intent == receipt ? Result::duplicate : Result::conflict;
    if (r->phase != RescuePhase::reserved) return Result::stale;
    if (!receipt.valid()) return Result::invalid;
    r->intent = receipt; r->phase = RescuePhase::intent_durable; return Result::ok;
}
Result Rescues::debit_durable(const Session& session, TransactionId id, DurableReceipt receipt) {
    reconcile(session);
    auto* r = mutable_record(id);
    if (!r) return Result::stale;
    if (r->debit.valid()) return r->debit == receipt ? Result::duplicate : Result::conflict;
    if (r->phase == RescuePhase::recovery_required) return Result::recovery_required;
    if (r->phase != RescuePhase::intent_durable) return Result::not_ready;
    if (!receipt.valid()) return Result::invalid;
    r->debit = receipt; r->phase = RescuePhase::debit_durable; return Result::ok;
}
Result Rescues::applied(Session& session, TransactionId id, DurableReceipt receipt) {
    reconcile(session);
    auto* r = mutable_record(id);
    if (!r) return Result::stale;
    if (r->phase == RescuePhase::completed) return r->applied == receipt ? Result::duplicate : Result::conflict;
    if (r->phase == RescuePhase::recovery_required) return Result::recovery_required;
    if (r->phase != RescuePhase::debit_durable) return Result::not_ready;
    if (!receipt.valid()) return Result::invalid;
    // Adapter must have applied idempotently and persisted its native receipt.
    const auto status = session.revive(r->target, r->world, r->down_generation);
    if (status != Result::ok) return status;
    r->applied = receipt; r->phase = RescuePhase::completed; return Result::ok;
}
Result Rescues::cancel(TransactionId id) {
    auto* r = mutable_record(id);
    if (!r) return Result::stale;
    if (r->phase == RescuePhase::cancelled) return Result::duplicate;
    // After durable intent, a debit command may already be in flight. Neither
    // disconnect nor timeout proves it did not execute. Keep a recovery lock.
    if (r->phase == RescuePhase::intent_durable || r->phase == RescuePhase::debit_durable ||
        r->phase == RescuePhase::recovery_required) {
        r->phase = RescuePhase::recovery_required; return Result::recovery_required;
    }
    if (r->phase != RescuePhase::reserved) return Result::conflict;
    r->phase = RescuePhase::cancelled; return Result::ok;
}
void Rescues::reconcile(const Session& session) {
    for (auto& r : records_) if (rescue_busy(r.phase) && !valid_participants(session, r)) {
        if (r.phase == RescuePhase::reserved) r.phase = RescuePhase::cancelled;
        else r.phase = RescuePhase::recovery_required;
    }
}
Result WipeGate::on_downed(const Session& session, const Checkpoints& checkpoints,
                           MemberRef changed, std::uint64_t generation) {
    const auto* m = session.find(changed);
    if (!m || !m->down || m->down_generation != generation) return Result::stale;
    if (pending_) return Result::busy;
    const auto slot = static_cast<std::size_t>(m - session.members().data());
    auto& seen = seen_[slot];
    if (seen.ref == changed && seen.generation >= generation && seen.world == session.world()) return Result::duplicate;
    // Remember even non-wipe transitions: later member removal cannot turn a
    // replay of this DOWN into a fresh wipe decision.
    seen = {changed, generation, session.world()};
    if (!session.all_active_down()) return Result::not_ready;
    const auto& latest = checkpoints.latest();
    if (latest.phase != CheckpointPhase::committed || latest.id.session != session.id()) return Result::not_ready;
    // Without that player's immutable character archive there is no complete
    // room restore. New arrivals must first participate in a new checkpoint.
    for (const auto& current : session.members()) if (current.phase == MemberPhase::active) {
        bool saved = false;
        for (std::size_t i = 0; i < latest.count; ++i)
            saved = saved || latest.participants[i].player == current.ref.player;
        if (!saved) return Result::not_ready;
    }
    checkpoint_ = latest.id; pending_ = true; return Result::ok;
}
Result WipeGate::restored(TransactionId checkpoint) {
    if (!pending_ || checkpoint != checkpoint_) return Result::stale;
    pending_ = false; return Result::ok;
}
}
