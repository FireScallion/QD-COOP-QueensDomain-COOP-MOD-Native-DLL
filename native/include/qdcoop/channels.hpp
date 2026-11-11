#pragma once
#include "session.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <span>

namespace qdcoop {
constexpr std::size_t event_capacity = 64, payload_capacity = 512;
struct Payload {
    std::uint32_t size{};
    std::array<std::uint8_t, payload_capacity> bytes{};
    bool operator==(const Payload&) const = default;
};
struct ReliableEvent {
    std::uint64_t sequence{}, enqueued_ms{}, last_sent_ms{};
    std::uint32_t attempts{};
    Payload payload{};
};
struct EnqueueResult { Result result; std::uint64_t sequence; };

// One instance per connection. It owns its payloads and never silently evicts
// critical events. A full queue requires the producer to retain/retry intent.
class PeerChannel {
public:
    explicit PeerChannel(MemberRef peer) : peer_(peer) {}
    EnqueueResult enqueue(std::span<const std::uint8_t> bytes, std::uint64_t now) {
        if (bytes.empty() || bytes.size() > payload_capacity) return {Result::invalid, 0};
        if (next_ == std::numeric_limits<std::uint64_t>::max()) return {Result::exhausted, 0};
        for (auto& event : events_) if (!event.sequence) {
            event = {}; event.sequence = next_++; event.enqueued_ms = now;
            event.payload.size = static_cast<std::uint32_t>(bytes.size());
            std::copy(bytes.begin(), bytes.end(), event.payload.bytes.begin());
            return {Result::ok, event.sequence};
        }
        return {Result::backpressure, 0};
    }
    const ReliableEvent* due(std::uint64_t now, std::uint64_t retry_ms) const {
        const ReliableEvent* best = nullptr;
        for (const auto& event : events_) if (event.sequence &&
            (event.attempts == 0 || (now >= event.last_sent_ms && now - event.last_sent_ms >= retry_ms)) &&
            (!best || event.sequence < best->sequence)) best = &event;
        return best;
    }
    Result sent(std::uint64_t sequence, std::uint64_t now) {
        for (auto& event : events_) if (event.sequence == sequence && sequence) {
            event.last_sent_ms = now;
            if (event.attempts != std::numeric_limits<std::uint32_t>::max()) ++event.attempts;
            return Result::ok;
        }
        return Result::stale;
    }
    // Receipt ACK releases transport storage only. Gameplay completion uses its
    // own transaction acknowledgements and durable receipts.
    Result receipt_ack(MemberRef source, std::uint64_t sequence) {
        if (source != peer_ || !sequence) return Result::stale;
        for (auto& event : events_) if (event.sequence == sequence) { event = {}; return Result::ok; }
        return sequence < next_ ? Result::duplicate : Result::stale;
    }
    std::size_t pending() const {
        std::size_t n = 0; for (const auto& event : events_) if (event.sequence) ++n; return n;
    }
    std::uint64_t oldest_age(std::uint64_t now) const {
        std::uint64_t age = 0;
        for (const auto& event : events_) if (event.sequence && now >= event.enqueued_ms)
            age = std::max(age, now - event.enqueued_ms);
        return age;
    }
private:
    MemberRef peer_{};
    std::uint64_t next_{1};
    std::array<ReliableEvent, event_capacity> events_{};
};

// Ordered bounded receiver: unlike a sliding bitset, a very late critical event
// is never silently classified as applied just because later packets arrived.
// Caller retries apply() after failure; only commit_applied advances the cursor.
class OrderedInbox {
public:
    explicit OrderedInbox(MemberRef peer) : peer_(peer) {}
    Result receive(MemberRef source, std::uint64_t sequence, const Payload& payload) {
        if (source != peer_ || !sequence) return Result::stale;
        if (!payload.size || payload.size > payload_capacity) return Result::invalid;
        if (sequence < next_) return Result::duplicate;
        if (sequence - next_ >= event_capacity) return Result::backpressure;
        auto& slot = slots_[sequence % event_capacity];
        if (slot.sequence) return slot.sequence == sequence && slot.payload.size == payload.size &&
            std::equal(payload.bytes.begin(), payload.bytes.begin() + payload.size, slot.payload.bytes.begin())
                ? Result::duplicate : Result::conflict;
        slot.sequence = sequence; slot.payload = payload;
        return Result::ok;
    }
    const ReliableEvent* next() const {
        const auto& slot = slots_[next_ % event_capacity];
        return slot.sequence == next_ ? &slot : nullptr;
    }
    Result commit_applied(std::uint64_t sequence) {
        if (!next() || sequence != next_) return Result::stale;
        if (next_ == std::numeric_limits<std::uint64_t>::max()) return Result::exhausted;
        slots_[next_ % event_capacity] = {}; ++next_; return Result::ok;
    }
private:
    MemberRef peer_{};
    std::uint64_t next_{1};
    std::array<ReliableEvent, event_capacity> slots_{};
};

class LatestSnapshot {
public:
    LatestSnapshot(MemberRef source, std::uint64_t world) : source_(source), world_(world) {}
    Result publish(MemberRef source, std::uint64_t world, std::uint64_t sequence, const Payload& payload) {
        if (source != source_ || world != world_ || sequence <= sequence_) return Result::stale;
        if (!payload.size || payload.size > payload_capacity) return Result::invalid;
        payload_ = payload; sequence_ = sequence; return Result::ok;
    }
    const Payload& value() const { return payload_; }
    std::uint64_t sequence() const { return sequence_; }
private:
    MemberRef source_{};
    std::uint64_t world_{}, sequence_{};
    Payload payload_{};
};
}
