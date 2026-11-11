#define QDCOOP_EXPORTS
#include "qdcoop/api.h"
#include "qdcoop/session.hpp"
#include <new>
#include <type_traits>

using qdcoop::Id;
using qdcoop::MemberRef;
struct QDNativeSession { qdcoop::Session room; };
static Id from(QDId id) { return {id.high, id.low}; }
static MemberRef from(QDMemberRef r) { return {from(r.session), from(r.player), r.connection}; }
static QDId to(Id id) { return {id.high, id.low}; }
static QDMemberRef to(MemberRef r) { return {to(r.session), to(r.player), r.connection}; }
static uint32_t result(qdcoop::Result r) { return static_cast<uint32_t>(r); }
static_assert(static_cast<uint32_t>(qdcoop::Result::exhausted) == QD_EXHAUSTED);
static_assert(std::is_standard_layout_v<QDRoomView>);
static_assert(sizeof(void*) == 8, "QD native requires Windows x64");
uint32_t QDNativeAbi(void) { return QD_NATIVE_ABI; }
uint32_t QDNativeCreateHost(const QDHostConfig* config, QDNativeSession** output) {
    if (!output) return QD_INVALID;
    *output = nullptr;
    if (!config || config->struct_size != sizeof(QDHostConfig) || config->abi != QD_NATIVE_ABI ||
        config->protocol != QD_NATIVE_PROTOCOL || config->reserved ||
        !from(config->session).valid() || !from(config->host).valid()) return QD_INVALID;
    *output = new (std::nothrow) QDNativeSession{qdcoop::Session(from(config->session), from(config->host))};
    return *output ? QD_OK : QD_BACKPRESSURE;
}
void QDNativeDestroy(QDNativeSession* session) { delete session; }
uint32_t QDNativeAdmit(QDNativeSession* session, QDId player, uint32_t protocol, QDMemberRef* output) {
    if (!output) return QD_INVALID;
    *output = {};
    if (!session) return QD_INVALID;
    const auto admission = session->room.admit_authenticated(from(player), protocol);
    *output = to(admission.ref); return result(admission.result);
}
uint32_t QDNativeReady(QDNativeSession* session, QDMemberRef member, uint64_t world) {
    if (!session) return QD_INVALID;
    return result(session->room.ready(from(member), world));
}
uint32_t QDNativeDisconnect(QDNativeSession* session, QDMemberRef member) {
    if (!session) return QD_INVALID;
    return result(session->room.disconnect(from(member)));
}
uint32_t QDNativeTravel(QDNativeSession* session, uint64_t world) {
    if (!session) return QD_INVALID;
    return result(session->room.travel(world));
}
uint32_t QDNativeDown(QDNativeSession* session, QDMemberRef member, uint64_t world, uint64_t generation) {
    if (!session) return QD_INVALID;
    return result(session->room.down(from(member), world, generation));
}
uint32_t QDNativeRoom(const QDNativeSession* session, QDRoomView* output) {
    if (!session || !output || output->struct_size != sizeof(QDRoomView)) return QD_INVALID;
    QDRoomView view{}; view.struct_size = sizeof(view); view.closed = session->room.closed();
    view.world = session->room.world(); view.revision = session->room.revision();
    for (std::size_t i = 0; i < qdcoop::max_players; ++i) {
        const auto& m = session->room.members()[i];
        view.members[i] = {to(m.ref), m.world, m.down_generation, static_cast<uint32_t>(m.phase), m.down};
    }
    *output = view; return QD_OK;
}
