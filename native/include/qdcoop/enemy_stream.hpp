#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <span>

namespace qdcoop::enemy {
constexpr unsigned capacity=128, history=24, packet_capacity=2048;
struct State {
    std::uint32_t id{}, generation{};
    std::uint64_t sequence{};
    double time{};
    // HP, attack/recovery/chase/hidden/visible, location, rotation, velocity,
    // component position, quaternion XYZW, scale. Matches preview2 capture.
    std::array<float,25> v{};
};
struct Packet { std::uint32_t size{}; std::array<std::uint8_t,packet_capacity> data{}; };
template<class T, unsigned N> class Spsc {
    static_assert(N>1);
    alignas(64) std::atomic<unsigned> write_{};
    alignas(64) std::atomic<unsigned> read_{};
    std::array<T,N> slots_{};
public:
    bool push(const T& value) {
        const auto w=write_.load(std::memory_order_relaxed), next=(w+1)%N;
        if(next==read_.load(std::memory_order_acquire))return false;
        slots_[w]=value;write_.store(next,std::memory_order_release);return true;
    }
    bool pop(T& value) {
        const auto r=read_.load(std::memory_order_relaxed);
        if(r==write_.load(std::memory_order_acquire))return false;
        value=slots_[r];read_.store((r+1)%N,std::memory_order_release);return true;
    }
    // Both queue owners must be stopped before discarding an epoch.
    void discard_quiescent() {
        read_.store(write_.load(std::memory_order_relaxed),std::memory_order_relaxed);
    }
};
struct Envelope { std::uint64_t room_high{},room_low{},world{}; std::uint32_t kind{}; };
bool encode(const Envelope&,const State&,Packet&);
bool decode(const Envelope&,const Packet&,State&);
bool text_packet(const Envelope&,std::span<const std::uint8_t>,Packet&);
bool text_payload(const Envelope&,const Packet&,std::span<const std::uint8_t>&);
bool finite(const State&);
float position(float a,float b,float va,float vb,double span,double u);
struct Sample { State state{}; unsigned kind{}; double time{}; };
struct Clock {
    bool initialized{};
    double latest{},arrived{},offset{},interval{.1},jitter{},delay{.16};
    void push(double source,double now);
};
class Timeline {
    std::array<State,history> frames_{};
    unsigned begin_{},count_{};
    double arrived_{},interval_{},jitter_{},delay_{.16},play_{},sample_at_{};
    bool playing_{};
public:
    std::uint64_t accepted{},stale{},teleports{};
    bool push(const State&,double now);
    bool sample(const Clock&,double now,Sample&);
    const State* latest() const {return count_?&frames_[(begin_+count_-1)%history]:nullptr;}
};
}
