#pragma once
#include <stdint.h>
#if defined(QDCOOP_EXPORTS)
#define QD_API __declspec(dllexport)
#elif defined(_WIN32)
#define QD_API __declspec(dllimport)
#else
#define QD_API
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define QD_NATIVE_ABI 1u
#define QD_NATIVE_PROTOCOL 0x4e340001u
typedef struct QDNativeSession QDNativeSession;
typedef struct QDId { uint64_t high, low; } QDId;
typedef struct QDMemberRef { QDId session, player; uint64_t connection; } QDMemberRef;
typedef struct QDHostConfig {
    uint32_t struct_size, abi, protocol, reserved;
    QDId session, host;
} QDHostConfig;
typedef struct QDMemberView {
    QDMemberRef ref;
    uint64_t world, down_generation;
    uint32_t phase, down;
} QDMemberView;
typedef struct QDRoomView {
    uint32_t struct_size, closed;
    uint64_t world, revision;
    QDMemberView members[4];
} QDRoomView;
enum QDResult {
    QD_OK, QD_DUPLICATE, QD_INVALID, QD_FULL, QD_STALE, QD_NOT_READY,
    QD_BUSY, QD_CONFLICT, QD_BACKPRESSURE, QD_PENDING, QD_RECOVERY_REQUIRED,
    QD_CLOSED, QD_EXHAUSTED
};
// One owner thread per handle. Admission IDs must be authenticated by the
// transport adapter; these functions are NOT a packet parser or authentication.
QD_API uint32_t QDNativeAbi(void);
QD_API uint32_t QDNativeCreateHost(const QDHostConfig* config, QDNativeSession** output);
QD_API void QDNativeDestroy(QDNativeSession* session);
QD_API uint32_t QDNativeAdmit(QDNativeSession* session, QDId player, uint32_t protocol, QDMemberRef* output);
QD_API uint32_t QDNativeReady(QDNativeSession* session, QDMemberRef member, uint64_t world);
QD_API uint32_t QDNativeDisconnect(QDNativeSession* session, QDMemberRef member);
QD_API uint32_t QDNativeTravel(QDNativeSession* session, uint64_t world);
QD_API uint32_t QDNativeDown(QDNativeSession* session, QDMemberRef member, uint64_t world, uint64_t generation);
QD_API uint32_t QDNativeRoom(const QDNativeSession* session, QDRoomView* output);
#ifdef __cplusplus
}
#endif
