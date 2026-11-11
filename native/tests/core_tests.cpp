#include "qdcoop/channels.hpp"
#include "qdcoop/transactions.hpp"
#include "qdcoop/steam_room.hpp"
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

using namespace qdcoop;
static unsigned checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)
static Id player(unsigned value) { return {10, value}; }
static DurableReceipt receipt(unsigned value) { return {{20, value}, 1024}; }
struct Room {
    Session session{{1, 1}, player(1)};
    std::array<MemberRef, 4> refs{};
    explicit Room(unsigned count = 4) {
        refs[0] = session.host(); CHECK(session.ready(refs[0], 1) == Result::ok);
        for (unsigned i = 1; i < count; ++i) {
            const auto a = session.admit_authenticated(player(i + 1), protocol_version);
            CHECK(a.result == Result::ok); refs[i] = a.ref;
            CHECK(session.ready(refs[i], 1) == Result::ok);
        }
    }
};
static void checkpoint(Room& room, Checkpoints& cp) {
    CHECK(cp.begin(room.session, room.refs[0], 42) == Result::ok);
    const auto id = cp.pending().id;
    for (std::size_t i = 0; i < cp.pending().count; ++i)
        CHECK(cp.saved(room.session, id, cp.pending().participants[i], receipt(static_cast<unsigned>(i + 1))) == Result::ok);
    CHECK(cp.commit(room.session, id, receipt(90)) == Result::ok);
}
static void membership() {
    for (unsigned count = 2; count <= 4; ++count) { Room room(count); CHECK(room.session.active_count() == count); }
    Room room;
    CHECK(room.session.admit_authenticated(player(5), protocol_version).result == Result::full);
    const auto revision = room.session.revision();
    const auto duplicate = room.session.admit_authenticated(player(2), protocol_version);
    CHECK(duplicate.result == Result::duplicate); CHECK(duplicate.ref == room.refs[1]);
    CHECK(room.session.revision() == revision);
    CHECK(room.session.admit_authenticated(player(9), 11802).result == Result::invalid);
    CHECK(room.session.disconnect(room.refs[1]) == Result::ok);
    CHECK(room.session.find(room.refs[2]) != nullptr); CHECK(room.session.find(room.refs[3]) != nullptr);
    const auto reconnect = room.session.admit_authenticated(player(2), protocol_version);
    CHECK(reconnect.result == Result::ok); CHECK(reconnect.ref.player == room.refs[1].player);
    CHECK(reconnect.ref.connection != room.refs[1].connection);
    CHECK(room.session.ready(room.refs[1], 1) == Result::stale);
    CHECK(room.session.down(reconnect.ref, 1, 1) == Result::not_ready);
    CHECK(room.session.ready(reconnect.ref, 1) == Result::ok);
    CHECK(room.session.down(reconnect.ref, 1, 3) == Result::ok);
    CHECK(room.session.disconnect(reconnect.ref) == Result::ok);
    const auto down_reconnect = room.session.admit_authenticated(player(2), protocol_version);
    CHECK(down_reconnect.result == Result::ok);
    CHECK(room.session.find(down_reconnect.ref)->down);
    CHECK(room.session.find(down_reconnect.ref)->down_generation == 3);
    CHECK(room.session.ready(down_reconnect.ref, 1) == Result::ok);
    CHECK(room.session.find(down_reconnect.ref)->down);
    CHECK(room.session.travel(2) == Result::ok);
    CHECK(room.session.find(down_reconnect.ref)->down); // travel is not a revival
    CHECK(room.session.ready(room.refs[2], 1) == Result::stale);
    CHECK(room.session.ready(room.refs[2], 2) == Result::ok);
    CHECK(room.session.active_count() == 1); // slow guests do not block others
    CHECK(room.session.down(room.refs[2], 1, 1) == Result::stale);
    CHECK(room.session.disconnect(room.refs[0]) == Result::ok);
    CHECK(room.session.closed()); CHECK(room.session.active_count() == 0);
    CHECK(room.session.admit_authenticated(player(7), protocol_version).result == Result::closed);
    Room churn(2);
    CHECK(churn.session.down(churn.refs[1], 1, 7) == Result::ok);
    CHECK(churn.session.disconnect(churn.refs[1]) == Result::ok);
    for (unsigned i = 3; i <= max_session_identities; ++i) {
        const auto a = churn.session.admit_authenticated(player(i), protocol_version);
        CHECK(a.result == Result::ok); CHECK(churn.session.disconnect(a.ref) == Result::ok);
    }
    CHECK(churn.session.admit_authenticated(player(100), protocol_version).result == Result::backpressure);
    const auto retained = churn.session.admit_authenticated(player(2), protocol_version);
    CHECK(retained.result == Result::ok); CHECK(churn.session.find(retained.ref)->down_generation == 7);
    std::puts("PASS membership, duplicate/full admission, reconnect, travel, host departure");
}
static void reliable() {
    Room room;
    PeerChannel one(room.refs[1]), two(room.refs[2]);
    std::array<std::uint8_t, 2> bytes{1, 2};
    for (unsigned i = 0; i < event_capacity; ++i) CHECK(one.enqueue(bytes, 100).result == Result::ok);
    CHECK(one.enqueue(bytes, 100).result == Result::backpressure);
    CHECK(two.enqueue(bytes, 100).result == Result::ok);
    bytes[0] = 99; CHECK(one.due(100, 10)->payload.bytes[0] == 1); // owned copy
    CHECK(one.receipt_ack(room.refs[2], 1) == Result::stale);
    CHECK(one.pending() == 64); CHECK(two.pending() == 1);
    CHECK(one.oldest_age(900) == 800);
    CHECK(one.sent(1, 100) == Result::ok);
    CHECK(one.due(105, 10)->sequence == 2); CHECK(one.due(110, 10)->sequence == 1);
    CHECK(one.receipt_ack(room.refs[1], 1) == Result::ok);
    CHECK(one.receipt_ack(room.refs[1], 1) == Result::duplicate);
    CHECK(one.enqueue(bytes, 101).result == Result::ok);
    OrderedInbox inbox(room.refs[1]); Payload p{}; p.size = 1; p.bytes[0] = 42;
    CHECK(inbox.receive(room.refs[1], 2, p) == Result::ok); CHECK(inbox.next() == nullptr);
    CHECK(inbox.receive(room.refs[1], 65, p) == Result::backpressure);
    CHECK(inbox.receive(room.refs[1], 1, p) == Result::ok);
    CHECK(inbox.receive(room.refs[1], 1, p) == Result::duplicate);
    Payload corrupt = p; corrupt.bytes[0] = 43;
    CHECK(inbox.receive(room.refs[1], 1, corrupt) == Result::conflict);
    CHECK(inbox.commit_applied(2) == Result::stale);
    CHECK(inbox.next()->sequence == 1); // failed apply can retry unchanged
    CHECK(inbox.commit_applied(1) == Result::ok); CHECK(inbox.next()->sequence == 2);
    CHECK(inbox.commit_applied(2) == Result::ok);
    CHECK(inbox.receive(room.refs[1], 1, p) == Result::duplicate);
    LatestSnapshot latest(room.refs[1], 1);
    CHECK(latest.publish(room.refs[1], 1, 8, p) == Result::ok);
    CHECK(latest.publish(room.refs[1], 1, 7, corrupt) == Result::stale);
    CHECK(latest.publish(room.refs[1], 2, 9, corrupt) == Result::stale);
    CHECK(latest.value().bytes[0] == 42);
    std::puts("PASS independent queues, backpressure, ordered apply, duplicates, stale snapshots");
}
static void fault_simulation() {
    Room room;
    struct Link {
        MemberRef ref; PeerChannel sender; OrderedInbox receiver;
        unsigned produced{}, applied{};
        explicit Link(MemberRef r) : ref(r), sender(r), receiver(r) {}
    };
    struct Flight { unsigned link; std::uint64_t at, seq; Payload payload; };
    for (unsigned seed = 1; seed <= 5; ++seed) {
        std::mt19937 random(seed);
        std::array<Link, 3> links{Link(room.refs[1]), Link(room.refs[2]), Link(room.refs[3])};
        std::vector<Flight> flights;
        for (std::uint64_t tick = 0; tick < 20000; ++tick) {
            for (unsigned i = 0; i < links.size(); ++i) {
                auto& link = links[i];
                if (link.produced < 200) {
                    const std::array<std::uint8_t, 1> data{static_cast<std::uint8_t>(link.produced)};
                    const auto r = link.sender.enqueue(data, tick);
                    if (r.result == Result::ok) ++link.produced;
                    else CHECK(r.result == Result::backpressure);
                }
                if (const auto* event = link.sender.due(tick, 13)) {
                    if (random() % 100 >= 30) flights.push_back({i, tick + random() % 20, event->sequence, event->payload});
                    CHECK(link.sender.sent(event->sequence, tick) == Result::ok);
                }
            }
            for (std::size_t i = 0; i < flights.size();) {
                if (flights[i].at > tick) { ++i; continue; }
                const auto f = flights[i]; auto& link = links[f.link];
                const auto r = link.receiver.receive(link.ref, f.seq, f.payload);
                CHECK(r == Result::ok || r == Result::duplicate || r == Result::backpressure);
                // ACK only after the receiver has accepted/retained the event.
                if ((r == Result::ok || r == Result::duplicate) && random() % 100 >= 30) {
                    const auto ack = link.sender.receipt_ack(link.ref, f.seq);
                    CHECK(ack == Result::ok || ack == Result::duplicate);
                }
                flights[i] = flights.back(); flights.pop_back();
            }
            for (auto& link : links) while (const auto* event = link.receiver.next()) {
                CHECK(event->payload.bytes[0] == static_cast<std::uint8_t>(link.applied));
                ++link.applied; CHECK(link.receiver.commit_applied(event->sequence) == Result::ok);
            }
            bool done = true;
            for (const auto& link : links) done = done && link.applied == 200 && link.sender.pending() == 0;
            if (done) break;
        }
        for (const auto& link : links) { CHECK(link.produced == 200); CHECK(link.applied == 200); CHECK(link.sender.pending() == 0); }
    }
    std::puts("PASS 3 peers x 200 reliable events x 5 seeds, 30% packet/ACK loss and reordering");
}
static void checkpoints() {
    Room room; Checkpoints cp;
    CHECK(cp.begin(room.session, room.refs[2], 0) == Result::invalid);
    CHECK(cp.begin(room.session, room.refs[2], 42) == Result::ok);
    CHECK(cp.pending().world_revision == 42);
    const auto id = cp.pending().id; CHECK(cp.pending().count == 4);
    CHECK(cp.commit(room.session, id, receipt(9)) == Result::not_ready);
    CHECK(cp.saved(room.session, id, room.refs[0], {}) == Result::invalid);
    for (unsigned i = 0; i < 3; ++i) CHECK(cp.saved(room.session, id, room.refs[i], receipt(i + 1)) == Result::ok);
    CHECK(cp.commit(room.session, id, receipt(9)) == Result::not_ready); // host/2 guests insufficient
    CHECK(cp.saved(room.session, id, room.refs[1], receipt(2)) == Result::duplicate);
    CHECK(cp.saved(room.session, id, room.refs[1], receipt(8)) == Result::conflict);
    CHECK(cp.saved(room.session, id, room.refs[3], receipt(4)) == Result::ok);
    CHECK(cp.commit(room.session, id, {}) == Result::invalid);
    CHECK(cp.latest().phase == CheckpointPhase::idle);
    CHECK(cp.commit(room.session, id, receipt(9)) == Result::ok);
    CHECK(cp.commit(room.session, id, receipt(9)) == Result::duplicate);
    CHECK(cp.latest().id == id);
    CHECK(cp.begin(room.session, room.refs[1], 43) == Result::ok);
    const auto interrupted = cp.pending().id;
    CHECK(cp.saved(room.session, interrupted, room.refs[0], receipt(1)) == Result::ok);
    CHECK(room.session.disconnect(room.refs[3]) == Result::ok);
    CHECK(cp.saved(room.session, interrupted, room.refs[1], receipt(2)) == Result::stale);
    CHECK(cp.pending().phase == CheckpointPhase::aborted); CHECK(cp.latest().id == id);
    const auto late = room.session.admit_authenticated(player(5), protocol_version);
    CHECK(cp.begin(room.session, room.refs[0], 44) == Result::not_ready);
    CHECK(room.session.ready(late.ref, 1) == Result::ok);
    CHECK(cp.begin(room.session, room.refs[0], 44) == Result::ok);
    CHECK(cp.saved(room.session, interrupted, room.refs[0], receipt(1)) == Result::stale);
    CHECK(room.session.travel(2) == Result::ok); CHECK(cp.reconcile(room.session) == Result::stale);
    CHECK(cp.latest().id == id);
    std::puts("PASS four-party durable checkpoint, save failure, disconnect, late join, world invalidation");
}
static void rescues() {
    Room room; Rescues rescues;
    CHECK(room.session.down(room.refs[3], 1, 1) == Result::ok);
    const auto first = rescues.reserve(room.session, room.refs[0], room.refs[3], 1, 2);
    CHECK(first.result == Result::ok);
    CHECK(rescues.reserve(room.session, room.refs[1], room.refs[3], 1, 2).result == Result::busy);
    CHECK(rescues.reserve(room.session, room.refs[0], room.refs[3], 1, 2).result == Result::duplicate);
    CHECK(rescues.debit_durable(room.session, first.id, receipt(1)) == Result::not_ready);
    CHECK(rescues.applied(room.session, first.id, receipt(2)) == Result::not_ready);
    CHECK(rescues.intent_durable(room.session, first.id, receipt(3)) == Result::ok);
    CHECK(rescues.debit_durable(room.session, first.id, receipt(4)) == Result::ok);
    CHECK(rescues.debit_durable(room.session, first.id, receipt(4)) == Result::duplicate);
    CHECK(rescues.debit_durable(room.session, first.id, receipt(5)) == Result::conflict);
    CHECK(rescues.applied(room.session, first.id, receipt(6)) == Result::ok);
    CHECK(!room.session.find(room.refs[3])->down);
    CHECK(rescues.applied(room.session, first.id, receipt(6)) == Result::duplicate);
    CHECK(room.session.down(room.refs[3], 1, 1) == Result::stale); // old death cannot re-down
    CHECK(room.session.down(room.refs[3], 1, 2) == Result::ok);
    const auto second = rescues.reserve(room.session, room.refs[1], room.refs[3], 2, 0);
    CHECK(second.result == Result::ok); CHECK(rescues.find(second.id)->item == 0);
    CHECK(rescues.cancel(second.id) == Result::ok); // no intent, no debit could be sent
    const auto third = rescues.reserve(room.session, room.refs[1], room.refs[3], 2, 1);
    CHECK(third.result == Result::ok);
    CHECK(rescues.intent_durable(room.session, third.id, receipt(7)) == Result::ok);
    CHECK(room.session.disconnect(room.refs[1]) == Result::ok);
    rescues.reconcile(room.session);
    CHECK(rescues.find(third.id)->phase == RescuePhase::recovery_required);
    CHECK(rescues.cancel(third.id) == Result::recovery_required);
    CHECK(rescues.debit_durable(room.session, third.id, receipt(8)) == Result::recovery_required);
    CHECK(rescues.reserve(room.session, room.refs[2], room.refs[3], 2, 1).result == Result::busy);
    const auto reconnect = room.session.admit_authenticated(player(2), protocol_version);
    CHECK(room.session.ready(reconnect.ref, 1) == Result::ok);
    CHECK(rescues.reserve(room.session, reconnect.ref, room.refs[3], 2, 1).result == Result::busy);
    CHECK(room.session.find(room.refs[3])->down);
    std::puts("PASS concurrent rescue arbitration, debit/apply receipts, generation checks, recovery lock");
}
static void wipes() {
    Room room; Checkpoints cp; WipeGate gate; checkpoint(room, cp);
    for (unsigned i = 0; i < 4; ++i) {
        CHECK(room.session.down(room.refs[i], 1, 1) == Result::ok);
        CHECK(gate.on_downed(room.session, cp, room.refs[i], 1) == (i == 3 ? Result::ok : Result::not_ready));
    }
    CHECK(gate.on_downed(room.session, cp, room.refs[3], 1) == Result::busy);
    CHECK(gate.restored(cp.latest().id) == Result::ok);
    CHECK(gate.on_downed(room.session, cp, room.refs[3], 1) == Result::duplicate);
    CHECK(gate.on_downed(room.session, cp, room.refs[0], 1) == Result::duplicate);
    Room partial(2); Checkpoints cp2; WipeGate gate2; checkpoint(partial, cp2);
    CHECK(partial.session.down(partial.refs[0], 1, 1) == Result::ok);
    CHECK(gate2.on_downed(partial.session, cp2, partial.refs[0], 1) == Result::not_ready);
    CHECK(partial.session.disconnect(partial.refs[1]) == Result::ok);
    CHECK(gate2.on_downed(partial.session, cp2, partial.refs[0], 1) == Result::duplicate);
    Room late(2); Checkpoints cp3; WipeGate gate3; checkpoint(late, cp3);
    auto guest = late.session.admit_authenticated(player(3), protocol_version);
    CHECK(late.session.ready(guest.ref, 1) == Result::ok);
    CHECK(late.session.down(late.refs[0], 1, 1) == Result::ok);
    CHECK(late.session.down(late.refs[1], 1, 1) == Result::ok);
    CHECK(late.session.down(guest.ref, 1, 1) == Result::ok);
    CHECK(gate3.on_downed(late.session, cp3, guest.ref, 1) == Result::not_ready);
    std::puts("PASS wipe latch, membership-only replay protection, missing character archive guard");
}
static void steam_room_model(){
    namespace SR=qdcoop::steam_room;
    constexpr SR::SteamId user=0x0110000100000000ull,lobby=0x0184000000001001ull,nonce=5678;
    const std::array<SR::SteamId,4> members{user+3,user+1,user+4,user+2};
    SR::Roster roster{};
    CHECK(SR::make_roster(user+3,members,4,roster));
    SR::Peers peers{};
    CHECK(SR::map_peers(lobby,nonce,user+3,roster,peers));
    CHECK(peers.mask==7&&peers.find(user+1)==0&&peers.find(user+2)==1&&peers.find(user+4)==2);
    CHECK(peers.find(user+3)==-1&&peers.find(user+5)==-1);
    for(const auto guest:members)if(guest!=roster.owner){
        CHECK(SR::map_peers(lobby,nonce,guest,roster,peers));
        CHECK(peers.mask==1&&peers.find(roster.owner)==0&&peers.find(guest)==-1);
    }
    CHECK(!SR::map_peers(lobby,nonce,user+5,roster,peers));
    CHECK(!SR::map_peers(lobby,0,user+3,roster,peers));
    CHECK(!SR::make_roster(user+3,members,5,roster));
    auto invalid=members;invalid[0]=invalid[1];
    CHECK(!SR::make_roster(user+1,invalid,4,roster));
    CHECK(!SR::make_roster(user+5,members,4,roster));
    CHECK(!SR::map_peers(user+1,nonce,user+3,roster,peers));
    auto packet=SR::control(lobby,nonce,SR::Control::hello);
    SR::Control kind{};
    CHECK(SR::decode_control(packet,lobby,nonce,kind)&&kind==SR::Control::hello);
    CHECK(!SR::decode_control(packet,lobby+1,nonce,kind));
    CHECK(!SR::decode_control(packet,lobby,nonce+1,kind));
    CHECK(!SR::match_datagram(packet,lobby,nonce));
    enemy::animation::wire::put(packet,4,protocol_version+1);
    CHECK(!SR::decode_control(packet,lobby,nonce,kind));
    packet=SR::control(lobby,nonce,SR::Control::ack);
    CHECK(SR::decode_control(packet,lobby,nonce,kind)&&kind==SR::Control::ack);
    SR::Handshake handshake{};
    for(unsigned peer=0;peer<3;++peer)handshake.observe(peer,SR::Control::hello);
    handshake.observe(1,SR::Control::ack);
    handshake.observe(0,SR::Control::ack);
    CHECK(handshake.connected()==3);
    handshake.observe(1,SR::Control::ack);
    handshake.observe(3,SR::Control::ack);
    CHECK(handshake.connected()==3);
    handshake.observe(2,SR::Control::ack);
    CHECK(handshake.connected()==7);
    enemy::animation::Frame frame{};
    frame.id=frame.generation=1;
    frame.sequence=1;
    frame.profile=31;
    CHECK(enemy::animation::encode({lobby,nonce,1,1},frame,packet));
    CHECK(SR::match_datagram(packet,lobby,nonce)&&!SR::match_datagram(packet,lobby,nonce+1));
    --packet.size;
    CHECK(!SR::match_datagram(packet,lobby,nonce));
    std::puts("PASS Steam room identity, four-person slot mapping and scoped control wire");
}
int main() {
    membership(); reliable(); fault_simulation(); checkpoints(); rescues(); wipes(); steam_room_model();
    std::printf("PASS all %u checks\n", checks);
}
