#include "qdcoop/world_progress_personal.hpp"
#include "qdcoop/persistent_travel.hpp"
#include "qdcoop/elevator_intent.hpp"
#include "qdcoop/cutscene_wire.hpp"
#include "qdcoop/boss_phase.hpp"
#include "qdcoop/boss_interact.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace W=qdcoop::world_progress;
namespace E=qdcoop::enemy;
namespace T=qdcoop::persistent_travel;
namespace Lift=qdcoop::elevator_intent;
namespace Cine=qdcoop::cutscene;
namespace Boss=qdcoop::boss_phase;
namespace BI=qdcoop::boss_interact;
static void check(bool condition,const char* label){
    if(condition)return;
    std::fprintf(stderr,"FAIL %s\n",label);
    std::exit(1);
}
int main(){
    W::Snapshot source;
    for(int i=-100;i<100;++i)source.actors.push_back({{i,43,-7,99},i*3});
    source.names={{"Alpha",-172},{"Rich key \xe8\xbf\x9b\xe5\xba\xa6",0},{"zLast",INT32_MIN}};
    source.mechanisms={{{4,3,2,1},W::MechanismKind::door,10000,true},{{1,2,3,4},W::MechanismKind::lever,0,false}};
    source.streaming.known=true;
    source.streaming.stacks[0]={{"/Game/RuinsLevel/Maps/LevelLists/GreenRuins.GreenRuins",7},
                                {"/Game/RuinsLevel/Maps/LevelLists/AutumnIsland.AutumnIsland",-3}};
    source.streaming.stacks[1]={{"/Game/RuinsLevel/Maps/LevelLists/GreenRuins_NearLibrary.GreenRuins_NearLibrary",5}};
    source.arrival={0x00000005deadbeefULL,{5552.f,-1842.5f,961.f},-76.5f};
    source.music="/Game/Music/1-Stage/1-1_GreenRuin-GreenRuins_MusicCue.1-1_GreenRuin-GreenRuins_MusicCue";
    source.items={{{-4,3,2,1},"/Game/Items/KeyItems/BookKeys/Alchemy/BookKey_00_Alchemy_Item.BookKey_00_Alchemy_Item"},
                  {{3,2,1,0},""}};
    source.barriers={
        {W::BarrierKind::necro,"/Game/RuinsLevel/Maps/300_LIB.300_LIB.PersistentLevel.Necro_Boss_Door","",
         W::identity_transform,1,true,true},
        {W::BarrierKind::bridge,"/Game/RuinsLevel/Maps/500_AUT.500_AUT.PersistentLevel.CastleBridge_Gate_Rear","BlockerBox",
         {0,0,0,1,47.f,80.f,-100.f,1.f,2.f,3.f},3,false,false}
    };
    W::Bytes bytes;
    W::Snapshot parsed;
    check(W::encode(source,bytes)&&W::decode(bytes,parsed)&&parsed==source,"signed snapshots and UTF-8 names");
    check(!W::decode(std::span(bytes).first(bytes.size()-1),parsed)&&parsed==source,"truncation cannot replace snapshot");
    auto duplicate=source;
    duplicate.items[1].key=duplicate.items[0].key;
    check(!W::encode(duplicate,bytes),"actor item keys must be unique and ordered");
    duplicate=source;
    duplicate.items[0].path="/Game/Items/BookKey.Other";
    check(!W::encode(duplicate,bytes),"actor items require canonical asset object paths");
    auto oversized=bytes;
    oversized[W::snapshot_prefix-7]=0xff;
    check(!W::decode(oversized,parsed)&&parsed==source,"oversized actor item count cannot replace snapshot");
    duplicate=source;
    duplicate.barriers[1].component="LeftDoor";
    check(!W::encode(duplicate,bytes),"bridge blocker does not target the animated door mesh");
    duplicate=source;
    duplicate.barriers[0].actor="/Game/RuinsLevel/Maps/300_LIB.300_LIB.Default__Actor";
    check(!W::encode(duplicate,bytes),"barrier actor requires a persistent level path");
    duplicate=source;
    duplicate.barriers[1].transform[3]=0.f;
    check(!W::encode(duplicate,bytes),"barrier transform requires a normalized rotation");
    duplicate=source;
    duplicate.names.insert(duplicate.names.begin()+1,{"aLPHA",1});
    check(!W::encode(duplicate,bytes),"case-insensitive duplicate key");
    duplicate=source;
    duplicate.mechanisms[1].activations=1;
    check(!W::encode(duplicate,bytes),"lever cannot carry a door activation count");
    duplicate=source;
    duplicate.streaming.known=false;
    check(!W::encode(duplicate,bytes),"unavailable stream snapshot cannot carry stacks");
    duplicate.streaming={};
    check(!W::encode(duplicate,bytes),"arrival requires a settled stream snapshot");
    duplicate=source;
    duplicate.arrival.position[2]=std::numeric_limits<float>::infinity();
    check(!W::encode(duplicate,bytes),"arrival rejects nonfinite coordinates");
    duplicate=source;
    duplicate.arrival.serial=0;
    check(!W::encode(duplicate,bytes),"arrival without a serial cannot carry a pose");
    duplicate=source;
    duplicate.music="/Game/RuinsLevel/Maps/LevelLists/GreenRuins.GreenRuins";
    check(!W::encode(duplicate,bytes),"music is restricted to the stock music package");
    duplicate=source;
    duplicate.streaming.stacks[0][0].path="/Game//GreenRuins.GreenRuins";
    check(!W::encode(duplicate,bytes),"stream paths reject empty package segments");
    duplicate.streaming.stacks[0][0].path="/Game/GreenRuins.AnotherObject";
    check(!W::encode(duplicate,bytes),"stream paths require exact asset object names");
    duplicate=source;
    duplicate.streaming.stacks[0][0].priority=100001;
    check(!W::encode(duplicate,bytes),"stream priorities remain bounded");
    check(std::ranges::is_sorted(W::personal_guids)&&W::personal(W::personal_guids[1000])&&
          W::personal("QDCOOPNATIVERESCUEV1_Identity1")&&W::personal("IntroGuideGranted")&&
          W::personal("Ippibi_State")&&!W::personal("UnlockedSouthGate"),"personal filters");

    const E::Envelope scope{111,222,333};
    const W::Revision revision{5,W::digest(bytes)};
    std::vector<E::Packet> packets;
    const auto total=static_cast<unsigned>(bytes.size());
    for(unsigned part=0;part<W::count(total);++part){
        const auto offset=part*W::part_bytes;
        E::Packet packet{};
        check(W::encode(scope,{revision,part,total,std::span(bytes).subspan(offset,std::min(W::part_bytes,total-offset))},packet),
              "snapshot part encode");
        packets.push_back(packet);
    }
    check(packets.size()>3,"multiple bounded datagrams");
    W::Inbox inbox;
    W::Part part{};
    check(!W::decode({111,222,444},packets.front(),part),"old world is fenced");
    for(unsigned i=static_cast<unsigned>(packets.size());i-->1;){
        check(W::decode(scope,packets[i],part)&&inbox.push(part)&&inbox.push(part),"reordered duplicate delivery");
    }
    check(!inbox.complete()&&!inbox.snapshot(parsed),"incomplete data cannot apply");
    check(W::decode(scope,packets.front(),part)&&inbox.push(part)&&inbox.snapshot(parsed)&&parsed==source,
          "late fragment completes snapshot");
    auto corrupt=packets.front();
    corrupt.data[W::header_bytes+3]^=0x02;
    check(W::decode(scope,corrupt,part)&&!inbox.push(part),"conflicting duplicate is rejected");
    E::Packet ack{};
    W::Revision decoded{};
    check(W::encode_ack(scope,revision,ack)&&W::decode_ack(scope,ack,decoded)&&decoded==revision&&
          !W::decode_ack({111,222,444},ack,decoded),"scoped applied acknowledgement");
    const T::Passport passport{4,987654,T::Map::intro,T::do_intro};
    T::Passport received;
    check(T::identify("/Game/RuinsLevel/Maps/Intro_Level_00.Intro_Level_00")==passport.map&&
          T::identify("/Game/RuinsLevel/Maps/Intro_Level_01.Intro_Level_01")==T::Map::none&&
          T::identify("/Game/RuinsLevel/Maps/100_GR.100_GR")==T::Map::none&&
          T::identify("/Game/LD45/Maps/title_screen.title_screen")==T::Map::none,
          "only cooked persistent gameplay worlds have a passport");
    check(T::encode(scope,passport,ack)&&T::decode({111,222,444},ack,received)&&received==passport&&
          !T::decode({111,223,333},ack,received)&&T::epoch(333,passport.nonce)!=T::epoch(333,0)&&
          T::canonical_epoch(T::Map::ruins)==2682127273519773619ULL,
          "room-scoped passport crosses a gameplay epoch and fences old data");
    check(!T::encode(scope,{2,0,T::Map::ruins},ack)&&!T::encode(scope,{2,3,T::Map::intro,4},ack)&&
          !T::encode(scope,{1,0,T::Map::none},ack),"world changes require a nonce and a playable map");
    const Lift::Message request{7,{-9,10,12,1},6,true,Lift::Result::request};
    Lift::Message arrived;
    check(Lift::encode(scope,request,ack)&&Lift::decode(scope,ack,arrived)&&arrived==request&&
          !Lift::decode({111,222,444},ack,arrived),"elevator request validates scoped identity and signed GUID");
    auto bad_lift=ack;
    bad_lift.data[68]=2;
    check(!Lift::decode(scope,bad_lift,arrived)&&arrived==request,"elevator rejects invalid direction");
    auto refused=request;
    refused.result=Lift::Result::refused;
    check(Lift::encode(scope,refused,ack)&&Lift::decode(scope,ack,arrived)&&arrived==refused,
          "elevator response includes the original request identity");
    auto unbound=request;
    unbound.generation=0;
    check(!Lift::encode(scope,unbound,ack),"elevator request requires active member generation");
    const Cine::Message play{12,0,Cine::Manager::ending1,Cine::Phase::play,1500};
    Cine::Message cue;
    check(Cine::encode(scope,play,ack)&&Cine::decode(scope,ack,cue)&&cue==play&&
          !Cine::decode({111,222,444},ack,cue),"cutscene cue is scoped to the current room and world");
    auto malformed_cue=ack;
    malformed_cue.data[68]=1;
    check(!Cine::decode(scope,malformed_cue,cue)&&cue==play,"cutscene rejects reserved data without replacing state");
    const Cine::Message commit{12,21,Cine::Manager::ending1,Cine::Phase::commit,0};
    check(Cine::encode(scope,commit,ack)&&Cine::decode(scope,ack,cue)&&cue==commit&&
          !Cine::encode(scope,{12,0,Cine::Manager::ending1,Cine::Phase::commit,0},ack)&&
          !Cine::encode(scope,{12,0,Cine::Manager::ending1,Cine::Phase::skip,1},ack)&&
          !Cine::encode(scope,{12,0,Cine::Manager::ending1,Cine::Phase::play,Cine::max_position_ms+1},ack),
          "cutscene commit requires a revision and only play carries bounded position");
    const Boss::State defeat{2,4,9,Boss::Profile::shrine,Boss::dead|Boss::suspended,3,{}};
    Boss::State boss;
    check(Boss::encode(scope,defeat,ack)&&Boss::decode(scope,ack,boss)&&boss==defeat&&
          !Boss::decode({111,222,444},ack,boss),"Boss state is generation-scoped to its room and world");
    auto invalid_boss=defeat;
    invalid_boss.phase[0]=1;
    check(!Boss::encode(scope,invalid_boss,ack),"Boss state rejects fields absent from the original class");
    invalid_boss.profile=Boss::Profile::queen;
    invalid_boss.phase[1]=2;
    check(!Boss::encode(scope,invalid_boss,ack),"Boss state rejects invalid Boolean fields");
    const BI::Message boss_request{20,2,4,6,BI::Result::request};
    BI::Message boss_response;
    check(BI::encode(scope,boss_request,ack)&&BI::decode(scope,ack,boss_response)&&boss_request==boss_response&&
          !BI::decode({111,222,444},ack,boss_response),"Boss interaction binds enemy and member generation to room scope");
    auto corrupt_request=ack;
    corrupt_request.data[60]=3;
    check(!BI::decode(scope,corrupt_request,boss_response)&&boss_response==boss_request&&
          !BI::encode(scope,{20,2,4,0,BI::Result::request},ack),"Boss interaction rejects corrupt results and missing member generation");
    std::puts("PASS signed typed progress snapshots, case-fold and personal filters, scoped ACK, loss, reorder and conflicts");
}
