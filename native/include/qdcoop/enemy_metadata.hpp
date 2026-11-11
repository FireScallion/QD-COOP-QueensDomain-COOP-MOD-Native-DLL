#pragma once
#include "weak_object_view.hpp"
#include "object_lookup.hpp"
#include <array>
#include <cstring>
#include <string_view>
namespace qdcoop::ue427 {
struct EnemyMetadata {
 struct Spec{std::string_view owner,property;};
 inline static constexpr std::array<Spec,42> specs{{
  {"/Game/Enemies/EnemyBase.EnemyBase_C",""},
  {"/Script/Engine.SkeletalMeshComponent",""},
  {"/Game/Enemies/EnemyBase.EnemyBase_C","HP"},
  {"/Game/Enemies/EnemyBase.EnemyBase_C","Mesh"},
  {"/Game/Enemies/EnemyBase.EnemyBase_C","bIsAttacking"},
  {"/Game/Enemies/EnemyBase.EnemyBase_C","bInRecoveryTime"},
  {"/Game/Enemies/EnemyBase.EnemyBase_C","bIsChasing"},
  {"/Game/Enemies/EnemyBase.EnemyBase_C","bHidden"},
  {"/Script/Engine.Actor.K2_GetActorLocation",""},
  {"/Script/Engine.Actor.K2_GetActorLocation","ReturnValue"},
  {"/Script/Engine.Actor.K2_GetActorRotation",""},
  {"/Script/Engine.Actor.K2_GetActorRotation","ReturnValue"},
  {"/Script/Engine.Actor.GetVelocity",""},
  {"/Script/Engine.Actor.GetVelocity","ReturnValue"},
  {"/Script/Engine.Actor.IsActorBeingDestroyed",""},
  {"/Script/Engine.Actor.IsActorBeingDestroyed","ReturnValue"},
  {"/Script/Engine.SceneComponent.K2_GetComponentToWorld",""},
  {"/Script/Engine.SceneComponent.K2_GetComponentToWorld","ReturnValue"},
  {"/Script/Engine.SceneComponent.IsVisible",""},
  {"/Script/Engine.SceneComponent.IsVisible","ReturnValue"},
  {"/Script/CoreUObject.Transform","Rotation"},
  {"/Script/CoreUObject.Transform","Translation"},
  {"/Script/CoreUObject.Transform","Scale3D"},
  {"/Script/Engine.Actor.K2_SetActorLocationAndRotation",""},
  {"/Script/Engine.Actor",""},
  {"/Script/Engine.Actor.K2_SetActorLocationAndRotation","NewLocation"},
  {"/Script/Engine.Actor.K2_SetActorLocationAndRotation","NewRotation"},
  {"/Script/Engine.Actor.K2_SetActorLocationAndRotation","bSweep"},
  {"/Script/Engine.Actor.K2_SetActorLocationAndRotation","SweepHitResult"},
  {"/Script/Engine.Actor.K2_SetActorLocationAndRotation","bTeleport"},
  {"/Script/Engine.Actor.K2_SetActorLocationAndRotation","ReturnValue"},
  {"/Script/Engine.SceneComponent.K2_SetWorldTransform",""},
  {"/Script/Engine.SceneComponent",""},
  {"/Script/Engine.SceneComponent.K2_SetWorldTransform","NewTransform"},
  {"/Script/Engine.SceneComponent.K2_SetWorldTransform","bSweep"},
  {"/Script/Engine.SceneComponent.K2_SetWorldTransform","SweepHitResult"},
  {"/Script/Engine.SceneComponent.K2_SetWorldTransform","bTeleport"},
  {"/Script/CoreUObject.Transform","Rotation"},
  {"/Script/CoreUObject.Transform","Translation"},
  {"/Script/CoreUObject.Transform","Scale3D"},
  {"/Script/Engine.Actor.SetActorHiddenInGame",""},
  {"/Script/Engine.SceneComponent.SetVisibility",""}
 }};
    std::array<WeakKey,42> owners{};
    std::array<std::uint64_t,42> arguments{};
    unsigned cursor{};
    unsigned found{};

    // Run on the verified game's thread. Assign is the engine weak assignment.
    template<class Reader,class Assign>
    bool step(std::uintptr_t array_address,std::uintptr_t pool,Reader read,
              Assign assign,unsigned budget=2048){
        ObjectArray array{};
        if(!read(array_address,&array,sizeof(array))||!object_array_shape(array))return false;

        const auto size=static_cast<unsigned>(array.num_elements);
        for(unsigned scanned=0;found<specs.size()&&scanned<budget&&cursor<size;++scanned,++cursor){
            std::uintptr_t chunk{};
            if(!read(array.chunks+(cursor/objects_per_chunk)*8,&chunk,8)||!chunk)continue;
            ObjectItem item{};
            const auto address=chunk+(cursor%objects_per_chunk)*sizeof(ObjectItem);
            if(!read(address,&item,sizeof(item))||!item.object||(item.flags&0x30000000u))continue;

            NameKey key{};
            std::string leaf;
            if(!read(item.object+24,&key,sizeof(key))||!name_string(pool,key,read,leaf))continue;
            bool wanted=false;
            for(const auto& spec:specs){
                if(spec.owner.substr(spec.owner.find_last_of('.')+1)==leaf){
                    wanted=true;
                    break;
                }
            }
            if(!wanted)continue;

            std::string path;
            if(!object_path(item.object,pool,read,path))continue;
            for(unsigned i=0;i<specs.size();++i){
                if(specs[i].owner==path)
                    assign(&owners[i],reinterpret_cast<const void*>(item.object));
            }
        }
        if(cursor>=size)cursor=0;

        found=0;
        for(unsigned i=0;i<specs.size();++i){
            const auto owner=resolve_weak(array_address,owners[i],read);
            arguments[i]=owner;
            if(owner&&!specs[i].property.empty())
                arguments[i]=find_inherited_property(owner,pool,specs[i].property,read);
            found+=arguments[i]!=0;
        }
        return found==specs.size();
    }
};
}
