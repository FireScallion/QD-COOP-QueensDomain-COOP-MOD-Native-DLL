#include "qdcoop/session.hpp"
#include <limits>

namespace qdcoop {
Session::Session(Id session, Id host_id) : id_(session) {
    closed_ = !session.valid() || !host_id.valid();
    if (!closed_) {
        members_[0] = {{session, host_id, 1}, MemberPhase::loading, world_, 0, false};
        lives_[0].player = host_id;
    }
}
Session::LifeRecord* Session::life(Id player) {
    for (auto& entry : lives_) if (entry.player == player) return &entry;
    return nullptr;
}
Member* Session::mutable_member(MemberRef ref) {
    if (closed_ || ref.session != id_) return nullptr;
    for (auto& m : members_) if (m.phase != MemberPhase::empty && m.ref == ref) return &m;
    return nullptr;
}
const Member* Session::find(MemberRef ref) const {
    if (closed_ || ref.session != id_) return nullptr;
    for (const auto& m : members_) if (m.phase != MemberPhase::empty && m.ref == ref) return &m;
    return nullptr;
}
Admission Session::admit_authenticated(Id player, std::uint32_t protocol) {
    if (closed_) return {Result::closed, {}};
    if (!player.valid() || protocol != protocol_version) return {Result::invalid, {}};
    for (const auto& m : members_)
        if (m.phase != MemberPhase::empty && m.ref.player == player) return {Result::duplicate, m.ref};
    for (std::size_t i = 1; i < members_.size(); ++i) {
        if (members_[i].phase != MemberPhase::empty) continue;
        if (next_connection_ == std::numeric_limits<std::uint64_t>::max()) return {Result::exhausted, {}};
        auto* record = life(player);
        if (!record) for (auto& entry : lives_) if (!entry.player.valid()) {
            entry.player = player; record = &entry; break;
        }
        if (!record) return {Result::backpressure, {}};
        members_[i] = {{id_, player, next_connection_++}, MemberPhase::loading, world_, record->generation, record->down};
        ++revision_;
        return {Result::ok, members_[i].ref};
    }
    return {Result::full, {}};
}
Result Session::ready(MemberRef ref, std::uint64_t world) {
    auto* m = mutable_member(ref);
    if (!m || world != world_) return Result::stale;
    if (m->phase == MemberPhase::active) return Result::duplicate;
    m->phase = MemberPhase::active; m->world = world_; ++revision_;
    return Result::ok;
}
Result Session::disconnect(MemberRef ref) {
    auto* m = mutable_member(ref);
    if (!m) return Result::stale;
    if (m == &members_[0]) { closed_ = true; for (auto& member : members_) member.phase = MemberPhase::empty; }
    else *m = {};
    ++revision_;
    return Result::ok;
}
Result Session::travel(std::uint64_t next_world) {
    if (closed_) return Result::closed;
    if (next_world <= world_) return Result::stale;
    world_ = next_world; ++revision_;
    for (auto& m : members_) if (m.phase != MemberPhase::empty) {
        m.phase = MemberPhase::loading; m.world = world_;
    }
    return Result::ok;
}
Result Session::down(MemberRef ref, std::uint64_t world, std::uint64_t generation) {
    auto* m = mutable_member(ref);
    if (!m || world != world_) return Result::stale;
    if (m->phase != MemberPhase::active) return Result::not_ready;
    if (!generation || generation < m->down_generation) return Result::stale;
    if (generation == m->down_generation) return m->down ? Result::duplicate : Result::stale;
    if (m->down) return Result::conflict;
    m->down = true; m->down_generation = generation;
    if (auto* record = life(m->ref.player)) { record->generation = generation; record->down = true; }
    return Result::ok;
}
Result Session::revive(MemberRef ref, std::uint64_t world, std::uint64_t generation) {
    auto* m = mutable_member(ref);
    if (!m || world != world_ || !generation || generation != m->down_generation) return Result::stale;
    if (m->phase != MemberPhase::active) return Result::not_ready;
    if (!m->down) return Result::duplicate;
    m->down = false;
    if (auto* record = life(m->ref.player)) record->down = false;
    return Result::ok;
}
std::size_t Session::active_count() const {
    std::size_t count = 0;
    for (const auto& m : members_) if (m.phase == MemberPhase::active) ++count;
    return count;
}
bool Session::all_active_down() const {
    if (closed_) return false;
    bool any = false;
    for (const auto& m : members_) if (m.phase == MemberPhase::active) {
        any = true; if (!m.down) return false;
    }
    return any;
}
}
