#include "qdcoop/projectile_stream.hpp"
#include "qdcoop/player_pose.hpp"
#include <cassert>
#include <cstdio>
#include <limits>

int main(){
    namespace P=qdcoop::projectile;
    namespace E=qdcoop::enemy;
    const E::Envelope scope{1,2,3,0},other_world{1,2,4,0};
    constexpr auto mesh=P::assets::id(P::assets::meshes,
        "/Game/Items/Weapons/Daggers/Default/Default_Dagger_Low.Default_Dagger_Low",P::assets::player_meshes);
    constexpr auto trail=P::assets::id(P::assets::trails,
        "/Game/Items/Weapons/Daggers/Default/Dagger_Projectile_Trail.Dagger_Projectile_Trail",P::assets::player_trails);
    static_assert(mesh&&trail);
    P::Frame active{1,7,P::Profile::stock_player,P::Phase::flying,10,
        {{1,2,3},{-1,1,1},{0,0,0,1}},{{4,5,6},{1,1,1},{0,0,0,1}},{34000,20,30},true,0,mesh,trail};
    E::Packet packet;
    P::Frame decoded;
    assert(P::encode(scope,active,packet)&&P::decode(scope,packet,decoded)&&decoded==active);
    assert(!P::decode(other_world,packet,decoded));
    E::animation::wire::put(packet,64,std::numeric_limits<float>::infinity());
    assert(!P::decode(scope,packet,decoded));
    auto bad=active;
    bad.mesh_asset=UINT16_MAX;
    assert(!P::encode(scope,bad,packet));
    bad=active;
    bad.id=P::capacity+1;
    assert(!P::encode(scope,bad,packet));
    bad=active;
    bad.slot=qdcoop::max_players;
    assert(!P::encode(scope,bad,packet));
    auto guest=active;
    guest.slot=qdcoop::player::unassigned;
    assert(P::encode(scope,guest,packet)&&P::decode(scope,packet,decoded));
    unsigned relay{};
    assert(P::accept_origin(decoded,true,1,7,relay)&&decoded.slot==2&&relay==5);
    assert(!P::accept_origin(decoded,true,0,7,relay));
    constexpr auto enemy_trail=P::assets::id(P::assets::trails,
        "/Game/Enemies/Boner_00/Skeletons24/Black/Boner_Gun_ProjectileTrail.Boner_Gun_ProjectileTrail",
        P::assets::player_trails);
    static_assert(enemy_trail>P::assets::player_trails);
    auto enemy=active;
    enemy.trail_asset=enemy_trail;
    assert(!P::encode(scope,enemy,packet));
    enemy.profile=P::Profile::stock_enemy;
    assert(P::encode(scope,enemy,packet)&&P::decode(scope,packet,decoded)&&decoded==enemy);
    assert(P::accept_origin(decoded,false,0,1,relay)&&!relay);
    assert(!P::accept_origin(decoded,false,1,1,relay)&&!P::accept_origin(decoded,true,0,7,relay));
    enemy.slot=qdcoop::player::unassigned;
    assert(!P::encode(scope,enemy,packet)&&!P::accept_origin(enemy,true,0,7,relay));
    auto effect=enemy;
    effect.profile=P::Profile::stock_enemy_effect;
    effect.slot=0;
    assert(!P::encode(scope,effect,packet));
    effect.mesh_asset=0;
    effect.mesh=effect.trail;
    effect.velocity={};
    assert(P::encode(scope,effect,packet)&&P::decode(scope,packet,decoded)&&decoded==effect);
    assert(P::accept_origin(decoded,false,0,1,relay)&&!P::accept_origin(decoded,true,0,7,relay));
    effect.profile=P::Profile::stock_player_effect;
    effect.slot=qdcoop::player::unassigned;
    assert(!P::encode(scope,effect,packet)); // Enemy-only asset.
    effect.trail_asset=trail;
    assert(P::encode(scope,effect,packet)&&P::decode(scope,packet,decoded)&&decoded==effect);
    assert(P::accept_origin(decoded,true,1,7,relay)&&decoded.slot==2&&relay==5);
    constexpr auto opacity=static_cast<std::uint16_t>(1+std::find(P::assets::variables.begin(),P::assets::variables.end(),
        std::string_view("User.Opacity"))-P::assets::variables.begin());
    static_assert(opacity<=P::assets::player_variables);
    auto tinted=effect;
    assert(P::set_scalar(tinted,opacity,.25f)&&P::set_scalar(tinted,opacity,.75f));
    assert(!P::set_scalar(tinted,opacity,std::numeric_limits<float>::quiet_NaN()));
    assert(!P::set_scalar(tinted,static_cast<std::uint16_t>(P::assets::player_variables+1),1.f));
    assert(P::encode(scope,tinted,packet)&&packet.size==P::bytes+P::parameter_header+4&&
           P::decode(scope,packet,decoded)&&decoded==tinted);
    E::animation::wire::put(packet,P::bytes,std::uint16_t{});
    assert(!P::decode(scope,packet,decoded));
    ++packet.size;
    assert(!P::decode(scope,packet,decoded));
    using Type=P::ParameterType;
    const auto word=[](float value){return std::bit_cast<std::uint32_t>(value);};
    assert(P::set_parameter(tinted,{1,Type::boolean,{1}}));
    assert(P::set_parameter(tinted,{2,Type::integer,{std::bit_cast<std::uint32_t>(INT32_MIN)}}));
    assert(P::set_parameter(tinted,{3,Type::vector,{word(1300.f),word(-500.f),word(.25f)}}));
    assert(P::set_parameter(tinted,{4,Type::color,{word(.25f),word(.5f),word(.75f),word(1.f)}}));
    assert(!P::set_parameter(tinted,{5,Type::boolean,{2}}));
    assert(!P::set_parameter(tinted,{5,Type::vector,{word(1.f),word(2.f),word(3.f),1}}));
    assert(P::encode(scope,tinted,packet)&&P::decode(scope,packet,decoded)&&decoded==tinted);
    E::animation::wire::put(packet,P::bytes+2,static_cast<std::uint8_t>(255));
    assert(!P::decode(scope,packet,decoded));
    constexpr auto cascade=P::assets::id(P::assets::particles,
        "/Game/LD45/FX/BloodSplosion.BloodSplosion",P::assets::player_particles);
    static_assert(cascade&&P::assets::player_particles==0);
    effect.profile=P::Profile::stock_enemy_particle;
    effect.trail_asset=cascade;
    assert(!P::encode(scope,effect,packet)&&!P::accept_origin(effect,true,1,7,relay));
    effect.slot=0;
    assert(P::encode(scope,effect,packet)&&P::decode(scope,packet,decoded)&&decoded==effect);
    assert(P::accept_origin(decoded,false,0,1,relay)&&!P::accept_origin(decoded,true,1,7,relay));
    effect.trail_asset=static_cast<std::uint16_t>(P::assets::particles.size()+1);
    assert(!P::encode(scope,effect,packet));
    effect.trail_asset=cascade;
    P::Track effect_track;
    assert(effect_track.push(effect,1.));
    auto draining=effect;
    draining.trail_active=false;
    draining.trail_hidden=true;
    ++draining.sequence;
    assert(P::encode(scope,draining,packet)&&P::decode(scope,packet,decoded)&&decoded==draining);
    assert(effect_track.push(decoded,1.05)&&effect_track.visible(1.1)&&!effect_track.push(effect,1.1));
    assert(effect_track.last.trail_hidden&&!effect_track.last.trail_active);
    E::animation::wire::put(packet,116,std::uint32_t{4});
    assert(!P::decode(scope,packet,decoded));
    effect.sequence=draining.sequence+1;
    assert(effect_track.push(effect,1.15)&&effect_track.last.trail_active);
    draining.trail_asset=0;
    assert(!P::encode(scope,draining,packet));
    bad=active;
    bad.phase=P::Phase::retired;
    assert(!P::encode(scope,bad,packet));
    bad=active;
    bad.mesh.rotation[3]=2;
    assert(!P::encode(scope,bad,packet));
    bad=active;
    bad.trail_active=false;
    assert(!P::encode(scope,bad,packet));
    bad.trail={};
    bad.trail_hidden=true;
    assert(!P::encode(scope,bad,packet));
    P::Track track;
    assert(track.push(active,1.)&&track.visible(1.3)&&!track.visible(1.36));
    assert(!track.push(active,1.4));
    const P::Frame terminal{1,7,P::Profile::stock_player,P::Phase::retired,11};
    assert(P::encode(scope,terminal,packet)&&P::decode(scope,packet,decoded)&&decoded==terminal);
    assert(track.push(terminal,1.4)&&!track.visible(1.4));
    auto retry=terminal;
    ++retry.sequence;
    assert(track.push(retry,1.45)&&!track.visible(1.45)&&!track.push(terminal,1.46));
    active.sequence=13;
    assert(!track.push(active,1.5));
    active.generation=8;
    active.sequence=1;
    assert(track.push(active,1.5)&&track.visible(1.5)&&!track.push(terminal,1.6));
    std::puts("PASS native projectile scope, finite transforms, duplicate retirement, generation reuse and lease expiry");
}
