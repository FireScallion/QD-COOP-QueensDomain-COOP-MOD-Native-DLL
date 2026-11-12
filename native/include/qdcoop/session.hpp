#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace qdcoop {
constexpr std::size_t max_players = 4;
constexpr std::size_t max_session_identities = 64;
constexpr std::uint32_t protocol_version = 0x4e340003;
enum class Result : std::uint32_t {
    ok, duplicate, invalid, full, stale, not_ready, busy, conflict,
    backpressure, pending, recovery_required, closed, exhausted
};
struct Id {
    std::uint64_t high{}, low{};
    constexpr bool valid() const { return high != 0 || low != 0; }
    bool operator==(const Id&) const = default;
};
// The adapter assigns stable IDs after authenticating Steam/LAN identities.
// These values are never accepted as proof of identity from a packet.
struct MemberRef {
    Id session{}, player{};
    std::uint64_t connection{};
    bool operator==(const MemberRef&) const = default;
};
enum class MemberPhase : std::uint32_t { empty, loading, active };
struct Member {
    MemberRef ref{};
    MemberPhase phase{};
    std::uint64_t world{}, down_generation{};
    bool down{};
};
struct Admission { Result result{Result::invalid}; MemberRef ref{}; };

// Single owner thread. No locks, waits, IO, UE pointers, or dynamic allocations.
// Network IO must deliver owned values to this owner through a separate queue.
class Session {
public:
    Session(Id session, Id host);
    Admission admit_authenticated(Id player, std::uint32_t protocol);
    Result ready(MemberRef ref, std::uint64_t world);
    Result disconnect(MemberRef ref);
    Result travel(std::uint64_t next_world);
    Result down(MemberRef ref, std::uint64_t world, std::uint64_t generation);
    Result revive(MemberRef ref, std::uint64_t world, std::uint64_t generation);
    const Member* find(MemberRef ref) const;
    const std::array<Member, max_players>& members() const { return members_; }
    MemberRef host() const { return members_[0].ref; }
    Id id() const { return id_; }
    std::uint64_t world() const { return world_; }
    std::uint64_t revision() const { return revision_; }
    bool closed() const { return closed_; }
    std::size_t active_count() const;
    bool all_active_down() const;
private:
    Member* mutable_member(MemberRef ref);
    struct LifeRecord { Id player{}; std::uint64_t generation{}; bool down{}; };
    LifeRecord* life(Id player);
    Id id_{};
    std::uint64_t world_{1}, revision_{1}, next_connection_{2};
    bool closed_{};
    std::array<Member, max_players> members_{};
    // Disconnected identities retain life state. Churn cannot reset a downed
    // player; capacity exhaustion rejects new identities, never evicts history.
    std::array<LifeRecord, max_session_identities> lives_{};
};
} // namespace qdcoop
