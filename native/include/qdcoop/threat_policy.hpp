#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace qdcoop::threat {
inline constexpr unsigned participants=4,none=participants;
inline constexpr double damage_window=3.,memory_time=1.,minimum_hold=1.5,attack_hold=1.2;
inline constexpr float combat_range=2000.f,front_cos=.5f;
inline float range(float awareness,bool engaged){return engaged?std::max(awareness,combat_range):awareness;}

struct Candidate {
    float distance{};
    bool alive{},in_range{},front{},los{};
};
struct Threat {
    struct Hit {double at;float amount;};
    std::array<std::vector<Hit>,participants> hits;
    std::array<double,participants> seen{-1e9,-1e9,-1e9,-1e9};
    unsigned target{none};
    double since{},lock_since{};
    bool locking{};

    void record(unsigned slot,float effective,double time){
        if(slot>=participants||!std::isfinite(effective)||effective<=0)return;
        auto& queue=hits[slot];
        if(queue.size()==128){
            queue[1].amount+=queue[0].amount;
            queue.erase(queue.begin());
        }
        queue.push_back({time,effective});
    }

    float recent(unsigned slot,double time){
        auto& queue=hits[slot];
        std::erase_if(queue,[&](const auto& hit){return time-hit.at>=damage_window;});
        float damage{};
        for(const auto& hit:queue)
            damage+=hit.amount*static_cast<float>(std::max(0.,1.-(time-hit.at)/damage_window));
        return damage;
    }

    bool choose(const std::array<Candidate,participants>& candidates,double time,bool attack_locked,float max_health,
                bool fear_decay=false){
        const auto previous=target;
        const auto scale=std::max(1.f,max_health*.15f);
        std::array<float,participants> damage{},scores{};
        std::array<bool,participants> eligible{};
        unsigned best=none,pull=none;
        for(unsigned slot=0;slot<participants;++slot){
            const auto& peer=candidates[slot];
            damage[slot]=recent(slot,time);
            if(!peer.alive||!peer.in_range||!std::isfinite(peer.distance))continue;
            const bool visible=peer.front&&peer.los;
            const bool sight=peer.los&&(visible||damage[slot]>0||slot==previous);
            if(sight)seen[slot]=time;
            const bool memory=slot==previous&&time-seen[slot]<memory_time;
            if(!sight&&!memory)continue;
            eligible[slot]=true;
            scores[slot]=1.2f*damage[slot]/(damage[slot]+scale)+.8f/(1.f+std::max(0.f,peer.distance)/500.f)+
                         (visible?.35f:0.f);
            if(best==none||scores[slot]>scores[best])best=slot;
        }
        if(previous!=none)for(unsigned slot=0;slot<participants;++slot){
            if(slot==previous||!eligible[slot]||!candidates[slot].los||damage[slot]<=0||damage[slot]<1.5f*damage[previous])continue;
            if(pull==none||damage[slot]>damage[pull])pull=slot;
        }
        if(pull!=none)best=pull;
        if(attack_locked&&!locking)lock_since=time;
        locking=attack_locked;
        if(previous!=none){
            const auto& old=candidates[previous];
            if(old.alive&&best==none&&fear_decay)best=previous;
            else if(old.alive&&old.in_range&&attack_locked&&time-lock_since<attack_hold)best=previous;
            else if(pull==none&&eligible[previous]&&best!=none&&best!=previous&&
                    (time-since<minimum_hold||scores[best]<=scores[previous]*1.25f+.12f))best=previous;
        }
        if(best==previous)return false;
        target=best;
        since=time;
        return true;
    }
};
}
