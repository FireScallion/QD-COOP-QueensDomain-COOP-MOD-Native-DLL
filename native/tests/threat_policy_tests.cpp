#include "qdcoop/threat_policy.hpp"
#include <cstdio>
#include <cstdlib>

namespace T=qdcoop::threat;
static void check(bool value,const char* message){
    if(value)return;
    std::fprintf(stderr,"FAIL %s\n",message);
    std::exit(1);
}
int main(){
    T::Threat state;
    const T::Candidate near{120.f,true,true,true,true};
    std::array<T::Candidate,T::participants> candidates{near,near,near,near};
    check(state.choose(candidates,0,false,60.f)&&state.target==0,"equal candidates retain deterministic slot order");
    state.record(3,30.f,.1);
    candidates[3].front=false;
    candidates[3].los=false;
    check(!state.choose(candidates,.2,false,60.f)&&state.target==0,"damage does not grant wall vision");
    candidates[3].los=true;
    check(!state.choose(candidates,.3,true,60.f)&&state.target==0,"native attack keeps original target");
    check(state.choose(candidates,1.6,true,60.f)&&state.target==3,"continuous lock expires and highest effective damage pulls");
    candidates[0].in_range=candidates[1].in_range=candidates[2].in_range=false;
    candidates[3].los=false;
    check(!state.choose(candidates,2.2,false,60.f)&&state.target==3,"recent visible target is remembered");
    check(state.choose(candidates,2.7,false,60.f)&&state.target==T::none,"lost target expires at stock memory limit");
    check(state.recent(3,3.2)==0&&T::range(375.f,false)==375.f&&T::range(375.f,true)==2000.f,
          "stock damage decay and short-sighted combat range");
    for(unsigned hit=0;hit<1000;++hit)state.record(2,.5f,4.);
    check(state.hits[2].size()==128&&state.recent(2,4.)==500.f,"flood bound aggregates effective damage");
    T::Threat flee;
    std::array<T::Candidate,T::participants> hiding{};
    hiding[1]=near;
    check(flee.choose(hiding,0,false,60.f,true)&&flee.target==1,"frightened lizard selects a visible player");
    hiding[1].los=false;
    hiding[1].in_range=false;
    check(!flee.choose(hiding,2.,false,60.f,true)&&flee.target==1&&flee.since==0.,
          "stock fear holds a living target across occlusion and range loss");
    check(flee.choose(hiding,3.,false,60.f,false)&&flee.target==T::none,"stock fear decay releases the target");
    hiding[1]=near;
    check(flee.choose(hiding,4.,false,60.f,true)&&flee.target==1,"frightened lizard can reacquire");
    hiding[1].alive=false;
    check(flee.choose(hiding,5.,false,60.f,true)&&flee.target==T::none,"lost player does not extend native fear");
    std::puts("Four-slot stock threat policy verified");
}
