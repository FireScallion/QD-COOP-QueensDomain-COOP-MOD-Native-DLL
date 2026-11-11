#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "native_status_text.hpp"
#include <string>
#include "qdcoop/enemy_metadata.hpp"
#include "qdcoop/enemy_discovery.hpp"
#include "qdcoop/enemy_enrollment.hpp"
#include "qdcoop/steam_room.hpp"
#include "qdcoop/lan_room.hpp"
#include "qdcoop/native_world.hpp"
#include "qdcoop/player_pose.hpp"
#include "qdcoop/player_gear_pose.hpp"
#include "qdcoop/rescue_wire.hpp"
#include "qdcoop/checkpoint_store_windows.hpp"
#include "qdcoop/world_progress_map.hpp"
#include "qdcoop/world_progress_personal.hpp"
#include <optional>
#include "qdcoop/reflection_view.hpp"
#include "qdcoop/native_save_clone.hpp"

namespace U=qdcoop::ue427;
namespace E=qdcoop::enemy;
namespace R=E::enrollment;
namespace SR=qdcoop::steam_room;
namespace Lan=qdcoop::lan_room;
namespace P=qdcoop::player;
using Address=std::uintptr_t;
using EngineTick=void(*)(void*,float,bool);
extern "C" int QDInstallNativeTick(EngineTick);

struct Core {
    int (*initialize)(const std::uint64_t*,unsigned){};
    int (*start)(unsigned,unsigned,std::uint64_t,std::uint64_t,unsigned,unsigned,unsigned){};
    int (*start_steam)(unsigned,std::uint64_t,unsigned,unsigned){};
    int (*steam_room)(SR::Status*,unsigned){};
    int (*start_lan)(unsigned,const wchar_t*,unsigned,unsigned){};
    int (*lan_room)(Lan::Status*,unsigned){};
    int (*recover)(){};
    int (*start_recovery)(){};
    int (*change_recovery_roster)(unsigned){};
    int (*configure_enum)(void*){};
    int (*enumerate)(void*,E::NativeCandidate*,unsigned){};
    int (*enroll)(std::uint32_t,std::uint32_t,const U::WeakKey*){};
    int (*read_offer)(std::uint32_t,R::Offer*){};
    int (*register_replica)(const R::Offer*,void*,void*){};
    int (*confirm_replica)(std::uint32_t,std::uint32_t){};
    int (*configure_destruction)(void*){};
    int (*retire)(std::uint32_t,std::uint32_t){};
    qdcoop::native_world::Phase (*pump_world)(void*,float,unsigned){};
    int (*inspect_binding)(std::uint32_t,E::NativeBindingView*){};
    int (*inspect_player)(unsigned,P::Binding*){};
    int (*inspect_rescue)(P::RescueView*){};
    int (*inspect_medic)(qdcoop::rescue::BindingView*){};
    int (*inspect_checkpoint)(qdcoop::checkpoint::BindingView*){};
    int (*inspect_equipment)(unsigned,P::EquipmentBinding*){};
    int (*inspect_gear_pose)(unsigned,unsigned,P::GearPoseBinding*){};
    int (*set_skin)(unsigned){};
    int (*bind_display)(std::uint32_t,std::uint32_t,void*,void*,void*){};
    int (*configure_death)(const std::uint64_t*,unsigned){};
    int (*observe_kill)(){};
    int (*configure_ownership)(const std::uint64_t*,unsigned){};
    int (*take_ownership)(std::uint32_t,std::uint32_t,const Address*,unsigned){};
    int (*release_ownership)(std::uint32_t,std::uint32_t){};
    int (*scan_animation)(unsigned){};
    unsigned (*bind_catalog_animation)(std::uint32_t,std::uint32_t){};
    unsigned (*status)(char*,unsigned){};
    int (*stop)(){};
    void (*tick)(float){};
};
static Core core;
static Address image;
static DWORD game_thread;
static bool host,initialized,started,quitting,passed,automatic_lab,reopen_lab,steam_smoke,recovery_lab;
static bool player_lab,player_body_lab,player_gear_lab,player_bone_lab;
static bool player_rescue_lab,player_medic_lab,player_medic_replay;
static bool player_medic_interrupt,player_medic_refund;
static bool checkpoint_probe,checkpoint_lab,checkpoint_refresh,checkpoint_wipe,checkpoint_wipe_missing;
static bool world_map_probe,world_progress_lab;
static bool world_actor_probe,world_chest_probe,world_chest_lab;
static bool world_mechanism_probe,world_mechanism_lab;
static bool world_shrine_gate_lab;
static bool world_castle_gate_lab;
static bool world_barrier_lab;
static bool world_arena_lab,world_cutscene_lab,world_boss_lab,world_boss_interact_lab;
static bool world_pause_probe,world_pause_lab;
static bool world_photo_probe,world_photo_lab;
static bool world_damage_probe,world_damage_stock,world_damage_lab,world_enemy_damage_lab,world_threat_lab,world_threat_memory_lab;
static U::WeakKey threat_forward_method;
static constexpr std::string_view threat_memory_test_paths[]{
    "/Game/Enemies/EnemyBase.EnemyBase_C.GetChaseTarget",
    "/Game/Enemies/EnemyBase.EnemyBase_C.GetLastSeenPlayerLoc",
    "/Script/Engine.KismetSystemLibrary.LineTraceSingle",
    "/Script/Engine.Default__KismetSystemLibrary"
};
static std::array<U::WeakKey,std::size(threat_memory_test_paths)> threat_memory_test_methods{};
static bool world_hitbox_lab,world_host_melee_lab;
static bool world_travel_lab;
static bool world_stream_probe,world_stream_replay,world_stream_lab;
static bool world_arrival_lab,world_arrival_cancel_lab;
static bool world_music_lab;
static bool world_story_lab;
static bool world_library_lab;
static bool world_elevator_lab;
static bool world_elevator_guest_lab;
static bool guest_safe_save_lab;
static bool persistent_save_probe,persistent_reload_lab,persistent_cross_lab;
static constexpr std::string_view travel_test_paths[]{
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.DoTeleporterWingStone",
    "/Game/UI/Menus/Teleporter/TeleporterUI.TeleporterUI_C.GoToDestination",
    "/Game/UI/Menus/SaveMenu.SaveMenu_C.BndEvt__SavedPopup_ShopButton_1_K2Node_ComponentBoundEvent_2_CommonButtonBaseClicked__DelegateSignature",
    "/Script/UMG.WidgetBlueprintLibrary.Create",
    "/Script/UMG.Default__WidgetBlueprintLibrary"
};
static std::array<U::WeakKey,std::size(travel_test_paths)> travel_test_methods{};
static constexpr std::string_view hitbox_test_paths[]{
    "/Script/Engine.PrimitiveComponent.GetCollisionEnabled",
    "/Script/Engine.PrimitiveComponent.GetCollisionResponseToChannel",
    "/Script/Engine.Actor.IsActorTickEnabled",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.InpActEvt_Attack_K2Node_InputActionEvent_4",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.InpActEvt_Attack_K2Node_InputActionEvent_5",
    "/Script/Engine.KismetStringLibrary.Conv_StringToName",
    "/Script/Engine.Default__KismetStringLibrary",
    "/Script/Engine.Controller.SetControlRotation",
    "/Script/Engine.PrimitiveComponent.GetCollisionObjectType",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.DoHitboxTrace",
    "/Script/Engine.SceneComponent.K2_GetComponentLocation"
};
static std::array<U::WeakKey,std::size(hitbox_test_paths)> hitbox_test_methods{};
static constexpr std::string_view damage_probe_paths[]{
    "/Game/Enemies/EnemyBase.EnemyBase_C.TryApplyDamage",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.TryApplyDamage",
    "/Script/FBAttributes.FB_DamageAttributeSet",
    "/Script/FBAttributes.FB_DamageAttributeUtilities.AddTagsToDamage",
    "/Script/GameplayTags.GameplayTagContainer",
    "/Script/GameplayTags.GameplayTag",
    "/Script/Engine.GameplayStatics.GetAllActorsOfClass",
    "/Game/Enemies/EnemyBase.EnemyBase_C"
};
static std::array<U::WeakKey,std::size(damage_probe_paths)> damage_probe_methods{};
static constexpr std::string_view photo_probe_paths[]{
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.EnterPhotoMode",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.TogglePhotoMode"
};
static std::array<U::WeakKey,std::size(photo_probe_paths)> photo_probe_methods{};
enum PauseProbe {pause_open,pause_gm,pause_menu_time,pause_enemy_time,pause_paused,pause_clock,pause_dilation,
                 pause_unpause,pause_set_menu,pause_set_enemy,pause_viewport,pause_visible,pause_remove,pause_find_widgets,
                 pause_global_set,pause_widget_lib,pause_menu_class,pause_methods};
static constexpr std::string_view pause_probe_paths[]{
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.OpenPauseMenu",
    "/Script/Engine.GameplayStatics.GetGameMode",
    "/Game/LD45/Blueprints/Base_LD45_GM.Base_LD45_GM_C.IsPausedTimeDilationEnabled",
    "/Game/LD45/Blueprints/Base_LD45_GM.Base_LD45_GM_C.IsEnemyPauseTDLEnabled",
    "/Script/Engine.GameplayStatics.IsGamePaused",
    "/Script/Engine.KismetSystemLibrary.GetGameTimeInSeconds",
    "/Script/Engine.GameplayStatics.GetGlobalTimeDilation",
    "/Script/Engine.GameplayStatics.SetGamePaused",
    "/Game/LD45/Blueprints/Base_LD45_GM.Base_LD45_GM_C.SetPauseTimeDilation",
    "/Game/LD45/Blueprints/Base_LD45_GM.Base_LD45_GM_C.SetPausedEnemyTimeDilation",
    "/Script/UMG.UserWidget.IsInViewport",
    "/Script/UMG.Widget.IsVisible",
    "/Script/UMG.Widget.RemoveFromParent",
    "/Script/UMG.WidgetBlueprintLibrary.GetAllWidgetsOfClass",
    "/Script/Engine.GameplayStatics.SetGlobalTimeDilation",
    "/Script/UMG.Default__WidgetBlueprintLibrary",
    "/Game/UI/Menus/PauseMenu.PauseMenu_C"
};
static std::array<U::WeakKey,pause_methods> pause_probe_methods{};
static constexpr std::string_view mechanism_paths[]{
    "/Game/Blueprints/Door_Base.Door_Base_C",
    "/Game/LD45/Blueprints/Objects/Gate/Lever.Lever_C"
};
static std::array<U::WeakKey,std::size(mechanism_paths)> mechanism_types{};
static constexpr std::string_view mechanism_timeline_paths[]{
    "/Script/Engine.Actor.K2_GetComponentsByClass",
    "/Script/Engine.TimelineComponent",
    "/Script/Engine.SceneComponent",
    "/Script/Engine.SceneComponent.K2_GetComponentLocation",
    "/Script/Engine.SceneComponent.K2_GetComponentRotation"
};
static std::array<U::WeakKey,std::size(mechanism_timeline_paths)> mechanism_timeline{};
static constexpr std::string_view world_actor_paths[]{
    "/Game/Blueprints/SaveSystem/SavableActor.SavableActor_C",
    "/Script/Engine.GameplayStatics.GetAllActorsOfClass",
    "/Script/Engine.KismetArrayLibrary.Array_Clear",
    "/Script/Engine.PrimitiveComponent.GetCollisionEnabled",
    "/Script/Engine.TimelineComponent.GetPlaybackPosition",
    "/Script/Engine.TimelineComponent.IsPlaying"
};
static std::array<U::WeakKey,std::size(world_actor_paths)> world_actor_methods{};
static constexpr std::string_view world_map_paths[]{
    "/Script/Engine.BlueprintMapLibrary.Map_Add",
    "/Script/Engine.BlueprintMapLibrary.Map_Find",
    "/Script/Engine.BlueprintMapLibrary.Map_Remove",
    "/Script/Engine.BlueprintMapLibrary.Map_Length",
    "/Script/Engine.BlueprintMapLibrary.Map_Clear",
    "/Script/Engine.Default__KismetStringLibrary",
    "/Script/Engine.KismetStringLibrary.Conv_StringToName"
};
static std::array<U::WeakKey,std::size(world_map_paths)> world_map_methods{};
enum CheckpointProbe {cp_get_index,cp_get_slot,cp_utils,cp_load,cp_save,cp_save_blueprint,cp_status_save,cp_open,
                      cp_stock_save,cp_health,cp_pit,cp_probe_count};
static constexpr std::string_view cp_probe_paths[]{
    "/Game/Blueprints/FS_GameInstance.FS_GameInstance_C.GetActiveSaveSlotIndex",
    "/Script/QueensDomain.BlueprintUtils.GetSaveSlotName",
    "/Script/QueensDomain.Default__BlueprintUtils",
    "/Script/Engine.GameplayStatics.LoadGameFromSlot",
    "/Script/Engine.GameplayStatics.SaveGameToSlot",
    "/Game/Blueprints/FS_GameInstance.FS_GameInstance_C.Save Game to Disk",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.Player_WriteStatusToSave",
    "/Script/Engine.GameplayStatics.OpenLevel",
    "/Game/Blueprints/FS_GameInstance.FS_GameInstance_C.GI_SaveGameToDisk",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.Set Health Points",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.ApplyPitDamage"
};
static std::array<U::WeakKey,cp_probe_count> cp_probe_methods{};
static bool presentation_lab;
enum PresentationMethod { probe_hud,probe_power,probe_mouse_pitch,probe_mouse_yaw,probe_pitch,probe_yaw,
                          probe_cam_mgr,probe_cam_position,probe_capsule,probe_dynamic,probe_hud_get,probe_control,
                          probe_scalar_get,probe_name,probe_strings,probe_count };
static constexpr std::string_view presentation_paths[]{
    "/Game/Player/Assets/Input/FS_HUD.FS_HUD_C.CanInteractHighlight",
    "/Game/UI/HUD/Widgets/StatusCluster.StatusCluster_C.UpdatePowerMeter",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.InpAxisEvt_LookUpMouse_K2Node_InputAxisEvent_2",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.InpAxisEvt_TurnMouse_K2Node_InputAxisEvent_3",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.HandlePitchInput",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.HandleYawInput",
    "/Script/Engine.GameplayStatics.GetPlayerCameraManager",
    "/Script/Engine.PlayerCameraManager.GetCameraLocation",
    "/Script/Engine.CapsuleComponent.GetScaledCapsuleHalfHeight",
    "/Script/UMG.Image.GetDynamicMaterial",
    "/Script/Engine.PlayerController.GetHUD",
    "/Script/Engine.Pawn.GetControlRotation",
    "/Script/Engine.MaterialInstanceDynamic.K2_GetScalarParameterValue",
    "/Script/Engine.KismetStringLibrary.Conv_StringToName",
    "/Script/Engine.Default__KismetStringLibrary"
};
static std::array<U::WeakKey,probe_count> presentation_methods{};
static_assert(std::size(presentation_paths)==presentation_methods.size());
static U::WeakKey rescue_inventory_count;
enum RescueLabMethod {rescue_health,rescue_pit,rescue_controller,rescue_control,rescue_method_count};
static constexpr std::string_view rescue_lab_paths[]{
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.Set Health Points",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.ApplyPitDamage",
    "/Script/Engine.Pawn.GetController",
    "/Script/Engine.Controller.SetControlRotation"
};
static std::array<U::WeakKey,rescue_method_count> rescue_lab_methods{};
enum BoneLabMethod {bone_show,bone_hide,bone_name,bone_strings,bone_socket,bone_method_count};
static constexpr std::string_view bone_lab_paths[]{
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.ShowQuickDashSword",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.HideQuickDashSwrod",
    "/Script/Engine.KismetStringLibrary.Conv_StringToName",
    "/Script/Engine.Default__KismetStringLibrary",
    "/Script/Engine.SceneComponent.GetSocketTransform"
};
static std::array<U::WeakKey,bone_method_count> bone_lab_methods{};
static_assert(std::size(bone_lab_paths)==bone_lab_methods.size());
static U::WeakKey local_pawn_method;
enum PlayerLabMethod {
    body_get_instance,body_play,body_stop,body_stopped,body_position,body_kismet,
    body_make_path,body_ref,body_load,body_method_count
};
static constexpr std::string_view player_body_paths[]{
    "/Script/Engine.SkeletalMeshComponent.GetAnimInstance",
    "/Script/Engine.AnimInstance.Montage_Play",
    "/Script/Engine.AnimInstance.Montage_Stop",
    "/Script/Engine.AnimInstance.Montage_GetIsStopped",
    "/Script/Engine.AnimInstance.Montage_GetPosition",
    "/Script/Engine.Default__KismetSystemLibrary",
    "/Script/Engine.KismetSystemLibrary.MakeSoftObjectPath",
    "/Script/Engine.KismetSystemLibrary.Conv_SoftObjPathToSoftObjRef",
    "/Script/Engine.KismetSystemLibrary.LoadAsset_Blocking"
};
static std::array<U::WeakKey,body_method_count> body_methods{};
static_assert(std::size(player_body_paths)==body_methods.size());
enum GearLabMethod {
    gear_equip,gear_equip_weapon,gear_unequip,gear_refresh,gear_register,gear_select,gear_inventory,
    gear_save_functions,gear_soft_ref,gear_get_material,gear_get_parent,gear_get_collision,gear_method_count
};
static constexpr std::string_view gear_lab_paths[]{
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.EquipItem",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.EquipItemWeapon",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.UnequipItem",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.EquipFirstEquippedItem",
    "RegisterWeaponClass",
    "SetSelectedWeaponByItem",
    "/Game/Blueprints/SaveSystem/SaveFunctions.SaveFunctions_C.AddItemToInventory",
    "/Game/Blueprints/SaveSystem/SaveFunctions.Default__SaveFunctions_C",
    "/Script/Engine.KismetSystemLibrary.Conv_ObjectToSoftObjectReference",
    "/Script/Engine.PrimitiveComponent.GetMaterial",
    "/Script/Engine.SceneComponent.GetAttachParent",
    "/Script/Engine.PrimitiveComponent.GetCollisionEnabled"
};
static std::array<U::WeakKey,gear_method_count> gear_lab_methods{};
static_assert(std::size(gear_lab_paths)==gear_lab_methods.size());
static SR::Status steam_opened{};
static bool steam_invalid_rejected,steam_closing,steam_rooms_distinct;
static bool steam_join_probe,steam_join_checked;
static unsigned steam_rooms_opened,steam_rooms_left,steam_closed_join_response;
static SR::SteamId steam_first_lobby{},steam_first_nonce{};
static unsigned role_index,client_count;
static wchar_t lan_ipv4[16]{};
static bool world_lan_lab;
static unsigned fixture_lan_epoch=1;
static constexpr const char* roles[]{"host","client","client2","client3"};
static constexpr const wchar_t* role_dirs[]{L"host",L"client",L"client2",L"client3"};
static constexpr unsigned first_port=49861;
static constexpr std::uint64_t lab_room=3819373611u,lab_nonce=1729814417u;
static ULONGLONG deadline,active_since,world_ready_since;
static unsigned phase,cursor,wrong_thread,active_ticks;
static U::EnemyMetadata metadata;
static U::WeakKey title,quit,library,player,enemy,gameplay,fixture_class,display_class,display;
enum Method {spawn_begin,spawn_finish,actor_tick,actor_collision,component_tick,kill_actor,get_owner,set_mesh,all_actors,destroy_actor,get_components,method_count};
static std::array<U::WeakKey,method_count> methods{};
static std::array<U::WeakKey,3> title_functions{};
static wchar_t report_path[32768]{},core_path[32768]{},run_root[32768]{};
static wchar_t ready_path[32768]{},verified_path[32768]{};
static std::atomic<bool> peer_ready{},peer_finished{};
static bool ready_published,alive_verified,death_published,display_hidden,death_hp_zero,death_actor_expired;
static std::atomic<bool> peers_alive_verified{},peers_montage_stopped{},peers_reuse_verified{};
static std::atomic<ULONGLONG> death_publish_seen{},retirement_notice_seen{};
static std::wstring death_marker,death_verified_marker,stop_verified_marker,retirement_notice_marker,
                    reuse_verified_marker,final_clean_marker;
static std::array<float,3> display_position{};
static Address original_kill_exec;
static bool kill_observer_installed,kill_slot_restored;
static bool native_enrolled,native_natural,offer_confirmed,stale_offer_rejected;
static bool death_verified,first_clean,reuse_verified,second_clean,early_reuse_rejected,old_generation_rejected,
            natural_reuse_alive,reuse_retire_requested,retirement_notice_written,reuse_enrolled;
static R::Offer offer,first_offer;
static U::WeakKey offer_class,offer_mesh;
static U::WeakKey reuse_candidate,first_world_enemy;
static std::string reuse_path;
static float reuse_hp,reuse_start_x;
static ULONGLONG offer_seen_at;
static unsigned native_candidates,enum_probe_count;
static double enum_max_us;
static std::string enemy_path;
static float hp_before,hp_after;
static std::array<float,3> position_before{},position_after{};
static constexpr float host_hp=50.f,client_hp=67.5f,final_hp=45.5f;
static bool first_snapshot_seen,second_marker_sent;
static std::array<double,16384> core_us{};
static unsigned core_sample_count;
static double counter_us;
static constexpr const char* title_names[]{
    "ConfirmLanguage",
    "BndEvt__TitleScreenUI_LoadGameButton_K2Node_ComponentBoundEvent_0_CommonButtonBaseClicked__DelegateSignature",
    "Load Slot"
};

static bool read_local(Address address,void* out,std::size_t size){
    std::memcpy(out,reinterpret_cast<const void*>(address),size);
    return true;
}
template<class T>static T at(Address address,unsigned offset){
    return U::field<T>(reinterpret_cast<const void*>(address),offset);
}
static std::string name(Address object){
    std::string result;
    if(object)U::name_string(image+0x4A27F00,at<U::NameKey>(object,24),read_local,result);
    return result;
}
static void assign(U::WeakKey* key,const void* object){
    reinterpret_cast<void(*)(U::WeakKey*,const void*)>(image+0x11BB360)(key,object);
}
static Address resolve(U::WeakKey key){
    return U::resolve_weak(image+0x4A64260,key,read_local);
}
static void remember(U::WeakKey& key,Address object){
    assign(&key,reinterpret_cast<const void*>(object));
}
static void event(Address object,Address function,void* parameters){
    using Event=void(*)(void*,void*,void*);
    const auto table=at<Address>(object,0);
    at<Event>(table,0x220)(reinterpret_cast<void*>(object),reinterpret_cast<void*>(function),parameters);
}
template<class Visit>static void scan(Visit visit){
    const auto array=at<U::ObjectArray>(image+0x4A64260,0);
    if(!U::object_array_shape(array))return;
    const auto size=static_cast<unsigned>(array.num_elements);
    for(unsigned n=0;n<4096&&cursor<size;++n,++cursor){
        const auto chunk=at<Address>(array.chunks,(cursor/U::objects_per_chunk)*8);
        if(!chunk)continue;
        const auto item=at<U::ObjectItem>(chunk,(cursor%U::objects_per_chunk)*sizeof(U::ObjectItem));
        if(item.object&&!(item.flags&0x30000000u))visit(item.object);
    }
    if(cursor>=size)cursor=0;
}
static bool parameter(Address owner,const char* wanted,int offset,int size){
    const auto property=U::find_property(owner,image+0x4A27F00,wanted,read_local);
    return property&&at<int>(property,76)==offset&&at<int>(property,60)==size&&
        (at<std::uint64_t>(property,64)&0x80u);
}
static bool valid_quit(Address fn){
    std::string path;
    return fn&&U::object_path(fn,image+0x4A27F00,read_local,path)&&
        path=="/Script/Engine.KismetSystemLibrary.QuitGame"&&at<std::uint16_t>(fn,182)==18&&
        parameter(fn,"WorldContextObject",0,8)&&parameter(fn,"SpecificPlayer",8,8)&&
        parameter(fn,"QuitPreference",16,1)&&parameter(fn,"bIgnorePlatformRestrictions",17,1);
}
static void title_step(){
    scan([](Address object){
        const auto leaf=name(object);
        const auto type=at<Address>(object,16);
        const auto outer=at<Address>(object,32);
        if(leaf=="Default__KismetSystemLibrary"&&name(type)=="KismetSystemLibrary")
            remember(library,object);
        if(leaf=="QuitGame"&&name(type)=="Function"&&valid_quit(object))
            remember(quit,object);
        if(!(at<unsigned>(object,8)&0x10u)&&name(type)=="TitleScreenUI_C")
            remember(title,object);
        const auto context=resolve(title);
        if(context&&outer==at<Address>(context,16)&&name(type)=="Function"){
            for(unsigned i=0;i<title_functions.size();++i)
                if(leaf==title_names[i])remember(title_functions[i],object);
        }
    });
    if(steam_smoke)return;
    const auto context=resolve(title);
    const auto confirm=resolve(title_functions[0]);
    const auto button_fn=resolve(title_functions[1]);
    const auto load=resolve(title_functions[2]);
    if(!context||!confirm||!button_fn||!load||!resolve(library)||!resolve(quit))return;
    const auto type=at<Address>(context,16);
    const auto button_property=U::find_inherited_property(type,image+0x4A27F00,"LoadGameButton",read_local);
    if(!button_property)return;
    const auto button_offset=at<int>(button_property,76);
    if(at<int>(button_property,60)!=8||button_offset<0||button_offset+8>at<int>(type,88)||
       at<std::uint16_t>(confirm,182)!=0||at<std::uint16_t>(button_fn,182)!=8||
       !parameter(button_fn,"Button",0,8)||at<std::uint16_t>(load,182)!=12||
       !parameter(load,"Slot",0,4)||!parameter(load,"Backup",4,1)||
       !parameter(load,"BackupSlot",8,4)){
        phase=10;
        return;
    }
    event(context,confirm,nullptr);
    const auto current=resolve(title);
    if(!current){phase=11;return;}
    auto button=at<Address>(current,static_cast<unsigned>(button_offset));
    if(!button){phase=12;return;}
    event(current,button_fn,&button);
    if(resolve(title)!=current){phase=13;return;}
    alignas(16) std::array<unsigned char,16> parameters{};
    const int slot=1;
    std::memcpy(parameters.data(),&slot,sizeof(slot));
    event(current,load,parameters.data());
    cursor=0;
    phase=1;
}

static std::array<float,3> location(Address actor){
    alignas(16) std::array<unsigned char,256> parameters{};
    event(actor,metadata.arguments[8],parameters.data());
    std::array<float,3> result{};
    const auto offset=at<int>(metadata.arguments[9],76);
    std::memcpy(result.data(),parameters.data()+offset,sizeof(result));
    return result;
}
static std::array<float,3> component_location(Address mesh){
    alignas(16) std::array<unsigned char,256> parameters{};
    event(mesh,metadata.arguments[16],parameters.data());
    const auto result_offset=at<int>(metadata.arguments[17],76);
    const auto translation_offset=at<int>(metadata.arguments[21],76);
    std::array<float,3> result{};
    std::memcpy(result.data(),parameters.data()+result_offset+translation_offset,sizeof(result));
    return result;
}
static constexpr const char* method_paths[]{
    "/Script/Engine.GameplayStatics.BeginDeferredActorSpawnFromClass",
    "/Script/Engine.GameplayStatics.FinishSpawningActor",
    "/Script/Engine.Actor.SetActorTickEnabled",
    "/Script/Engine.Actor.SetActorEnableCollision",
    "/Script/Engine.ActorComponent.SetComponentTickEnabled",
    "/Game/Enemies/EnemyBase.EnemyBase_C.Kill",
    "/Script/Engine.ActorComponent.GetOwner",
    "/Script/Engine.SkinnedMeshComponent.SetSkeletalMesh",
    "/Script/Engine.GameplayStatics.GetAllActorsOfClass",
    "/Script/Engine.Actor.K2_DestroyActor",
    "/Script/Engine.Actor.K2_GetComponentsByClass"
};
static constexpr const char* fixture_path="/Game/Enemies/Lizer_00/Default_Lizer_01.Default_Lizer_01_C";
using Parameters=std::array<unsigned char,256>;

static bool exact_path(Address object,const char* expected){
    std::string path;
    return U::object_path(object,image+0x4A27F00,read_local,path)&&path==expected;
}
static int parameter_offset(Address function,const char* wanted,unsigned size){
    const auto property=U::find_property(function,image+0x4A27F00,wanted,read_local);
    const auto bytes=at<std::uint16_t>(function,182);
    if(!property||bytes>256||at<int>(property,60)!=static_cast<int>(size)||
       !(at<std::uint64_t>(property,64)&0x80u))return -1;
    const auto offset=at<int>(property,76);
    if(offset<0||static_cast<unsigned>(offset)+size>bytes)return -1;
    return offset;
}
template<class T>static bool put(Parameters& data,Address function,const char* wanted,const T& value){
    const auto offset=parameter_offset(function,wanted,sizeof(T));
    if(offset<0)return false;
    std::memcpy(data.data()+offset,&value,sizeof(T));
    return true;
}
static Address spawn_actor(Address context,Address type,const std::array<float,12>& transform,Address default_mode=0){
    const auto begin=resolve(methods[spawn_begin]),finish_spawn=resolve(methods[spawn_finish]);
    const auto cdo=resolve(gameplay);
    if(!begin||!finish_spawn||!cdo||!type)return 0;
    alignas(16) Parameters begin_args{},finish_args{};
    const auto begin_return=parameter_offset(begin,"ReturnValue",sizeof(Address));
    const auto finish_actor=parameter_offset(finish_spawn,"Actor",sizeof(Address));
    const auto finish_return=parameter_offset(finish_spawn,"ReturnValue",sizeof(Address));
    if(begin_return<0||finish_actor<0||finish_return<0||
       !put(begin_args,begin,"WorldContextObject",context)||!put(begin_args,begin,"ActorClass",type)||
       !put(begin_args,begin,"SpawnTransform",transform)||
       !put(begin_args,begin,"CollisionHandlingOverride",std::uint8_t{1})||
       !put(begin_args,begin,"Owner",Address{})||!put(finish_args,finish_spawn,"SpawnTransform",transform)){
        phase=25;
        return 0;
    }
    event(cdo,begin,begin_args.data());
    Address actor{};
    std::memcpy(&actor,begin_args.data()+begin_return,sizeof(actor));
    if(!actor){phase=26;return 0;}
    if(default_mode)U::set_bool(reinterpret_cast<void*>(default_mode),reinterpret_cast<void*>(actor),true,0);
    std::memcpy(finish_args.data()+finish_actor,&actor,sizeof(actor));
    event(cdo,finish_spawn,finish_args.data());
    Address completed{};
    std::memcpy(&completed,finish_args.data()+finish_return,sizeof(completed));
    if(completed!=actor){phase=27;return 0;}

    return actor;
}

static Address field_of(Address type,const char* field,unsigned size){
    const auto property=U::find_inherited_property(type,image+0x4A27F00,field,read_local);
    if(!property||at<int>(property,60)!=static_cast<int>(size))return 0;
    const auto offset=at<int>(property,76);
    const auto owner_size=at<int>(type,88);
    return offset>=0&&owner_size>=static_cast<int>(size)&&offset<=owner_size-static_cast<int>(size)?property:0;
}
static Address object_field(Address object,const char* field){
    const auto property=field_of(at<Address>(object,16),field,8);
    return property?at<Address>(object,static_cast<unsigned>(at<int>(property,76))):0;
}
static std::array<float,12> transform_at(std::array<float,3> position){
    return {0,0,0,1,position[0],position[1],position[2],0,1,1,1,0};
}
static bool quiet_natural_enemy(Address actor){
    const auto disable_tick=resolve(methods[actor_tick]),disable_collision=resolve(methods[actor_collision]);
    const auto disable_component=resolve(methods[component_tick]);
    if(!actor||!disable_tick||!disable_collision||!disable_component||
       at<std::uint16_t>(disable_tick,182)!=1||at<std::uint16_t>(disable_collision,182)!=1||
       at<std::uint16_t>(disable_component,182)!=1)return false;
    unsigned char disabled{};
    event(actor,disable_tick,&disabled);
    event(actor,disable_collision,&disabled);
    if(const auto movement=object_field(actor,"CharacterMovement"))event(movement,disable_component,&disabled);
    return true;
}
static bool configure_fixture_death(){
    const auto type=resolve(fixture_class);
    const auto mode=field_of(type,"DefaultEnemyMode",1);
    const auto dilation=field_of(metadata.arguments[24],"CustomTimeDilation",4);
    const std::array<std::uint64_t,6> args{
        resolve(methods[kill_actor]),resolve(methods[actor_tick]),resolve(methods[component_tick]),dilation,type,mode
    };
    return std::all_of(args.begin(),args.end(),[](auto value){return value!=0;})&&
        core.configure_death(args.data(),static_cast<unsigned>(args.size()));
}
static bool bind_fixture_display(Address context,Address actor,Address source_mesh){
    const auto visual_type=resolve(display_class);
    const auto owner=resolve(methods[get_owner]),set_asset=resolve(methods[set_mesh]);
    if(!visual_type||!owner||!set_asset||parameter_offset(owner,"ReturnValue",8)!=0)return false;
    const auto asset=object_field(source_mesh,"SkeletalMesh");
    alignas(16) Parameters mesh_args{};
    if(!asset||!put(mesh_args,set_asset,"NewMesh",asset)||!put(mesh_args,set_asset,"bReinitPose",std::uint8_t{1}))return false;
    const auto visual=spawn_actor(context,visual_type,transform_at(location(actor)));
    if(!visual)return false;
    const auto pose=object_field(visual,"SkeletalMeshComponent");
    if(!pose)return false;
    event(pose,set_asset,mesh_args.data());
    unsigned char disabled{};
    event(visual,resolve(methods[actor_collision]),&disabled);
    if(!core.bind_display(offer.id,offer.generation,reinterpret_cast<void*>(visual),reinterpret_cast<void*>(pose),reinterpret_cast<void*>(owner)))
        return false;
    remember(display,visual);
    return true;
}

#include "ownership_fixture.inc"
#include "animation_fixture.inc"
#include "enrollment_fixture.inc"

static void world_step(){
    if(resolve(player)&&!world_ready_since)world_ready_since=GetTickCount64();
    if(host&&!recovery_lab&&!player_lab&&!peer_ready&&world_ready_since)return;
    const auto fixture=resolve(player);
    const auto selection=player_gear_lab&&fixture?object_field(fixture,"WeaponSelection"):0;
    scan([&](Address object){
        const auto leaf=name(object);
        const auto type=at<Address>(object,16);
        const auto outer=at<Address>(object,32);
        if(!resolve(player)&&!(at<unsigned>(object,8)&0x10u)&&
           name(type)=="FS_Full_PlayerChar_C"&&name(outer)=="PersistentLevel")remember(player,object);
        if(leaf=="Default__GameplayStatics"&&exact_path(object,"/Script/Engine.Default__GameplayStatics"))
            remember(gameplay,object);
        if(leaf=="Default_Lizer_01_C"&&exact_path(object,fixture_path))remember(fixture_class,object);
        if(leaf=="SkeletalMeshActor"&&exact_path(object,"/Script/Engine.SkeletalMeshActor"))remember(display_class,object);
        if(!host||reopen_lab||player_lab)discover_ownership(object,leaf);
        if(!automatic_lab&&!player_lab)discover_animation(object,leaf);
        if(player_lab&&leaf=="GetPlayerPawn"&&exact_path(object,"/Script/Engine.GameplayStatics.GetPlayerPawn"))
            remember(local_pawn_method,object);
        if(player_rescue_lab)for(unsigned i=0;i<rescue_lab_methods.size();++i){
            const auto path=rescue_lab_paths[i];
            if(leaf==path.substr(path.find_last_of('.')+1)&&exact_path(object,path.data()))
                remember(rescue_lab_methods[i],object);
        }
        if(checkpoint_lab||guest_safe_save_lab||(checkpoint_probe&&host))for(unsigned i=0;i<cp_probe_methods.size();++i){
            const auto path=cp_probe_paths[i];
            if(leaf==path.substr(path.find_last_of('.')+1)&&exact_path(object,path.data()))
                remember(cp_probe_methods[i],object);
        }
        if(world_progress_lab||world_story_lab||world_castle_gate_lab||world_arena_lab||world_cutscene_lab||(world_map_probe&&host))for(unsigned i=0;i<world_map_methods.size();++i){
            const auto path=world_map_paths[i];
            if(leaf==path.substr(path.find_last_of('.')+1)&&exact_path(object,path.data()))
                remember(world_map_methods[i],object);
        }
        if(world_chest_lab||world_mechanism_lab||((world_actor_probe||world_mechanism_probe)&&host))
        for(unsigned i=0;i<world_actor_methods.size();++i){
            const auto path=world_actor_paths[i];
            if(leaf==path.substr(path.find_last_of('.')+1)&&exact_path(object,path.data()))
                remember(world_actor_methods[i],object);
        }
        if(world_mechanism_lab||(world_mechanism_probe&&host))for(unsigned i=0;i<mechanism_types.size();++i){
            const auto path=mechanism_paths[i];
            if(leaf==path.substr(path.find_last_of('.')+1)&&exact_path(object,path.data()))
                remember(mechanism_types[i],object);
        }
        if(world_mechanism_lab)for(unsigned i=0;i<mechanism_timeline.size();++i){
            const auto path=mechanism_timeline_paths[i];
            if(leaf==path.substr(path.find_last_of('.')+1)&&exact_path(object,path.data()))
                remember(mechanism_timeline[i],object);
        }
        if(world_pause_lab||world_photo_lab||((world_pause_probe||world_photo_probe)&&host))
        for(unsigned i=0;i<pause_probe_methods.size();++i){
            const auto path=pause_probe_paths[i];
            if(leaf==path.substr(path.find_last_of('.')+1)&&exact_path(object,path.data()))remember(pause_probe_methods[i],object);
        }
        if(world_photo_lab||(world_photo_probe&&host))for(unsigned i=0;i<photo_probe_methods.size();++i){
            const auto path=photo_probe_paths[i];
            if(leaf==path.substr(path.find_last_of('.')+1)&&exact_path(object,path.data()))remember(photo_probe_methods[i],object);
        }
        if(world_hitbox_lab||automatic_lab||world_enemy_damage_lab)for(unsigned i=0;i<(world_hitbox_lab?hitbox_test_methods.size():2);++i){
            const auto path=hitbox_test_paths[i];
            if(leaf==path.substr(path.find_last_of('.')+1)&&exact_path(object,path.data()))remember(hitbox_test_methods[i],object);
        }
        if(world_threat_lab&&leaf=="GetAIForwardVector"&&
           exact_path(object,"/Game/Enemies/EnemyBase.EnemyBase_C.GetAIForwardVector"))remember(threat_forward_method,object);
        if(world_threat_memory_lab)for(unsigned i=0;i<threat_memory_test_methods.size();++i){
            const auto path=threat_memory_test_paths[i];
            if(leaf==path.substr(path.find_last_of('.')+1)&&exact_path(object,path.data()))remember(threat_memory_test_methods[i],object);
        }
        if(world_damage_lab||world_enemy_damage_lab||(world_damage_probe&&host))for(unsigned i=0;i<damage_probe_methods.size();++i){
            const auto path=damage_probe_paths[i];
            if(leaf==path.substr(path.find_last_of('.')+1)&&exact_path(object,path.data()))remember(damage_probe_methods[i],object);
        }
        if(presentation_lab){
            for(unsigned i=0;i<presentation_methods.size();++i){
                const auto path=presentation_paths[i];
                if(leaf==path.substr(path.find_last_of('.')+1)&&exact_path(object,path.data()))
                    remember(presentation_methods[i],object);
            }
        }
        if((player_medic_lab||player_medic_replay)&&leaf=="ReadInventoryItemCount"&&
           exact_path(object,"/Game/Blueprints/FS_GameInstance.FS_GameInstance_C.ReadInventoryItemCount"))
            remember(rescue_inventory_count,object);
        if(player_bone_lab)for(unsigned i=0;i<bone_lab_methods.size();++i){
            const auto path=bone_lab_paths[i];
            if(leaf==path.substr(path.find_last_of('.')+1)&&exact_path(object,path.data()))
                remember(bone_lab_methods[i],object);
        }
        if(player_body_lab||player_gear_lab||player_rescue_lab||world_music_lab||world_story_lab||
           world_progress_lab||world_library_lab||world_elevator_lab||world_elevator_guest_lab||world_castle_gate_lab||world_arena_lab)
        for(unsigned i=0;i<body_methods.size();++i){
            const auto path=player_body_paths[i];
            if(leaf==path.substr(path.find_last_of('.')+1)&&exact_path(object,path.data()))
                remember(body_methods[i],object);
        }
        if(player_gear_lab)for(unsigned i=0;i<gear_lab_methods.size();++i){
            const auto path=gear_lab_paths[i];
            if(leaf!=path.substr(path.find_last_of('.')+1))continue;
            if(i==gear_register||i==gear_select){
                if(!selection||name(type)!="Function")continue;
                for(auto parent=at<Address>(selection,16);parent;parent=at<Address>(parent,64)){
                    if(parent!=outer)continue;
                    remember(gear_lab_methods[i],object);
                    break;
                }
            }else if(exact_path(object,path.data()))remember(gear_lab_methods[i],object);
        }
        for(unsigned i=0;i<methods.size();++i){
            const std::string_view path=method_paths[i];
            if(leaf==path.substr(path.find_last_of('.')+1)&&name(type)=="Function"&&exact_path(object,method_paths[i]))
                remember(methods[i],object);
        }
    });
    const auto context=resolve(player);
    if(!context||!world_ready_since||GetTickCount64()-world_ready_since<1600||
       (host&&!recovery_lab&&!player_lab&&!peer_ready))return;
    if(recovery_lab){
        started=core.start_recovery()!=0;
        phase=started?2:81;
        return;
    }
    if(player_lab){
        if(!resolve(gameplay)||!resolve(local_pawn_method)||!resolve(ownership_keys[1])||
           !resolve(methods[actor_tick])||!resolve(methods[actor_collision])||!resolve(methods[component_tick]))return;
        if((player_body_lab||player_gear_lab||player_rescue_lab)&&
           std::any_of(body_methods.begin(),body_methods.end(),[](auto key){return !resolve(key);}))
            return;
        if(player_rescue_lab&&std::any_of(rescue_lab_methods.begin(),rescue_lab_methods.end(),[](auto key){return !resolve(key);}))
            return;
        if((player_medic_lab||player_medic_replay)&&!resolve(rescue_inventory_count))return;
        if((checkpoint_lab||guest_safe_save_lab||(checkpoint_probe&&host))&&
           std::ranges::any_of(cp_probe_methods,[](auto key){return !resolve(key);}))return;
        if((world_progress_lab||world_story_lab||world_castle_gate_lab||world_arena_lab||world_cutscene_lab||(world_map_probe&&host))&&
           std::ranges::any_of(world_map_methods,[](auto key){return !resolve(key);}))return;
        if((world_chest_lab||world_mechanism_lab||((world_actor_probe||world_mechanism_probe)&&host))&&
           std::ranges::any_of(world_actor_methods,[](auto key){return !resolve(key);}))return;
        if((world_mechanism_lab||(world_mechanism_probe&&host))&&
           std::ranges::any_of(mechanism_types,[](auto key){return !resolve(key);}))return;
        if(world_mechanism_lab&&std::ranges::any_of(mechanism_timeline,[](auto key){return !resolve(key);}))return;
        if((world_pause_lab||world_photo_lab||((world_pause_probe||world_photo_probe)&&host))&&
           std::ranges::any_of(pause_probe_methods,[](auto key){return !resolve(key);}))return;
        if((world_photo_lab||(world_photo_probe&&host))&&
           std::ranges::any_of(photo_probe_methods,[](auto key){return !resolve(key);}))return;
        if((world_damage_lab||world_enemy_damage_lab||(world_damage_probe&&host))&&
           std::ranges::any_of(damage_probe_methods,[](auto key){return !resolve(key);}))return;
        if(world_hitbox_lab&&std::ranges::any_of(hitbox_test_methods,[](auto key){return !resolve(key);}))return;
        if(world_enemy_damage_lab&&(!resolve(hitbox_test_methods[0])||!resolve(hitbox_test_methods[1])))return;
        if(world_threat_lab&&!resolve(threat_forward_method))return;
        if(world_threat_memory_lab&&std::ranges::any_of(threat_memory_test_methods,[](auto key){return !resolve(key);}))return;
        if(presentation_lab&&std::ranges::any_of(presentation_methods,[](auto key){return !resolve(key);}))
            return;
        if(player_gear_lab&&std::any_of(gear_lab_methods.begin(),gear_lab_methods.end(),[](auto key){return !resolve(key);}))
            return;
        if(player_bone_lab&&std::any_of(bone_lab_methods.begin(),bone_lab_methods.end(),[](auto key){return !resolve(key);}))
            return;
    }else if(automatic_lab){
        if((host||reopen_lab)&&(!resolve(methods[actor_tick])||!resolve(methods[actor_collision])||
                               !resolve(methods[component_tick])||!resolve(methods[kill_actor])))return;
        if((!host||reopen_lab)&&(!resolve(ownership_keys[0])||!resolve(ownership_keys[1])))return;
        if(!resolve(hitbox_test_methods[0])||!resolve(hitbox_test_methods[1]))return;
    }else{
        if(!animation_inputs_ready()||!resolve(fixture_class)||!resolve(methods[kill_actor])||
           !resolve(methods[actor_tick])||!resolve(methods[component_tick])||(host&&!resolve(methods[all_actors]))||
           (!host&&(!resolve(display_class)||!resolve(methods[get_owner])||
                    !resolve(methods[set_mesh])||!resolve(methods[destroy_actor])||!ownership_inputs_ready())))return;
        if(!scan_fixture_catalog())return;
        if(!configure_fixture_death()){phase=28;return;}
        if(host&&!core.configure_enum(reinterpret_cast<void*>(resolve(methods[all_actors])))){phase=43;return;}
        if(!host&&!core.configure_destruction(reinterpret_cast<void*>(resolve(methods[destroy_actor])))){phase=50;return;}
    }
    const auto local=first_port+role_index;
    std::array<unsigned,3> peers{};
    if(host){
        for(unsigned i=0;i<client_count;++i)peers[i]=first_port+i+1;
    }else{
        peers[0]=first_port;
    }
    const auto opened=world_lan_lab?core.start_lan(host?1:0,host?nullptr:lan_ipv4,first_port,host?client_count+1:0):
        core.start(host?1:0,local,lab_room,lab_nonce,peers[0],peers[1],peers[2]);
    if(!opened){
        phase=16;
        return;
    }
    started=true;
    phase=2;
}

static void write_file(const wchar_t* path,const char* text,unsigned size){
    auto file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return;
    DWORD written{};
    WriteFile(file,text,size,&written,nullptr);
    CloseHandle(file);
}
static double core_percentile(unsigned percent){
    if(!core_sample_count)return 0;
    return core_us[(core_sample_count-1)*percent/100];
}
static void write_automatic_report();
static void write_recovery_report();
static void write_player_report();
static void restore_rescue_fixture_selection();
static void write_steam_report(){
    SR::Status state{};
    if(!core.steam_room(&state,sizeof(state)))return;
    char json[512]{};
    const auto bytes=std::snprintf(json,sizeof(json),
        "{\"phase\":%u,\"failure\":%u,\"opened_members\":%u,\"opened_limit\":%u,\"expected\":%u,"
        "\"lobby_seen\":%s,\"nonce_seen\":%s,\"left\":%u,\"invalid_rejected\":%s,\"module_loaded\":%s,"
        "\"initialized_here\":%u,\"init_result\":%d,\"rooms_opened\":%u,\"rooms_left\":%u,\"rooms_distinct\":%s,"
        "\"closed_join_response\":%u}",
        state.phase,state.failure,steam_opened.members,steam_opened.limit,state.expected,
        steam_opened.lobby?"true":"false",steam_opened.nonce?"true":"false",state.left,
        steam_invalid_rejected?"true":"false",GetModuleHandleW(L"steam_api64.dll")?"true":"false",
        state.initialized_here,state.init_result,steam_rooms_opened,steam_rooms_left,steam_rooms_distinct?"true":"false",
        steam_closed_join_response);
    const auto path=std::wstring(run_root)+L"/host/native-steam.json";
    if(bytes>0&&bytes<static_cast<int>(sizeof(json)))write_file(path.c_str(),json,static_cast<unsigned>(bytes));
}
static void finish(){
    // Keep the receiving socket open until the host has stopped sending.
    if(!host&&passed){
        if(!peer_finished&&GetTickCount64()<deadline)return;
    }
    const auto context=resolve(player)?resolve(player):resolve(title);
    const auto fn=resolve(quit),cdo=resolve(library);
    if(!context||!fn||!cdo)return;
    if(initialized){
        if(player_rescue_lab)restore_rescue_fixture_selection();
        if(started&&!core.stop())return;
        if(automatic_lab)write_automatic_report();
        if(steam_smoke)write_steam_report();
        if(recovery_lab)write_recovery_report();
        if(player_lab)write_player_report();
        const auto status=read_native_status(core.status);
        if(!status.empty())write_file(core_path,status.data(),static_cast<unsigned>(status.size()));
        if(host&&kill_observer_installed){
            const auto kill=resolve(methods[kill_actor]);
            kill_slot_restored=kill&&at<Address>(kill,216)==original_kill_exec;
        }
    }
    std::sort(core_us.begin(),core_us.begin()+core_sample_count);
    char text[3072]{};
    const auto bytes=std::snprintf(text,sizeof(text),
        "{\"role\":\"%s\",\"client_count\":%u,\"port\":%u,\"passed\":%s,\"phase\":%u,\"game_thread\":%lu,\"wrong_thread\":%u,"
        "\"ue4ss_loaded\":%s,\"metadata_found\":%u,\"active_ticks\":%u,\"hp_before\":%.2f,\"hp_after\":%.2f,"
        "\"position_before\":[%.3f,%.3f,%.3f],\"position_after\":[%.3f,%.3f,%.3f],\"actor\":\"%s\",\"first_snapshot_seen\":%s,\"second_marker_sent\":%s,\"core_samples\":%u,\"core_us_p50\":%.3f,\"core_us_p95\":%.3f,\"core_us_p99\":%.3f,\"death_published\":%s,\"display_hidden\":%s,\"death_hp_zero\":%s,\"death_actor_expired\":%s,\"display_position\":[%.3f,%.3f,%.3f],\"kill_observer_installed\":%s,\"kill_slot_restored\":%s,\"ownership_fixture_step\":%u,\"ownership_before\":%u,\"ownership_held\":%u,\"ownership_after\":%u,\"dilation_before\":%.2f,\"dilation_held\":%.2f,\"dilation_after\":%.2f,\"ownership_audio\":%u,\"ownership_audio_active_before\":%u,\"ownership_audio_paused_held\":%u,\"ownership_restored\":%s,\"stale_release_rejected\":%s,\"fixture_controller\":%s,\"fixture_brain\":%s,\"fixture_uro\":%s,\"fixture_los\":%s,"
        "\"animation_methods\":%u,\"animation_fixture_step\":%u,\"animation_started\":%s,\"animation_stopped\":%s,"
        "\"animation_first\":%s,\"animation_second\":%s,\"animation_advanced\":%s,"
        "\"aim_observed\":%.3f,\"montage_first_position\":%.4f,\"montage_last_position\":%.4f,\"montage_rate\":%.3f,"
        "\"catalog_profile\":%u,\"catalog_scan_slices\":%u,\"catalog_scan_max_us\":%.3f,"
        "\"native_natural\":%s,\"native_enrolled\":%s,\"offer_confirmed\":%s,"
        "\"stale_offer_rejected\":%s,\"native_candidates\":%u,\"enum_max_us\":%.3f,"
        "\"offer_class\":%u,\"offer_mesh\":%u,"
        "\"first_clean\":%s,\"reuse_enrolled\":%s,\"reuse_verified\":%s,\"second_clean\":%s,"
        "\"early_reuse_rejected\":%s,\"old_generation_rejected\":%s,\"natural_reuse_alive\":%s,"
        "\"reuse_actor\":\"%s\",\"reuse_hp\":%.2f,\"last_generation\":%u}",
        roles[role_index],client_count,first_port+role_index,passed?"true":"false",phase,static_cast<unsigned long>(game_thread),wrong_thread,
        GetModuleHandleW(L"UE4SS.dll")?"true":"false",metadata.found,active_ticks,hp_before,hp_after,
        position_before[0],position_before[1],position_before[2],position_after[0],position_after[1],
        position_after[2],enemy_path.c_str(),first_snapshot_seen?"true":"false",
        second_marker_sent?"true":"false",core_sample_count,core_percentile(50),core_percentile(95),core_percentile(99),
        death_published?"true":"false",display_hidden?"true":"false",death_hp_zero?"true":"false",
        death_actor_expired?"true":"false",display_position[0],display_position[1],display_position[2],
        kill_observer_installed?"true":"false",kill_slot_restored?"true":"false",
        ownership_fixture_step,ownership_before,ownership_held,ownership_after,
        dilation_before,dilation_held,dilation_after,ownership_audio,ownership_audio_active_before,ownership_audio_paused_held,ownership_restored?"true":"false",
        stale_release_rejected?"true":"false",fixture_controller?"true":"false",fixture_brain?"true":"false",
        fixture_uro?"true":"false",fixture_los?"true":"false",
        animation_methods_found,animation_fixture_step,animation_started?"true":"false",animation_stopped?"true":"false",
        animation_first?"true":"false",animation_second?"true":"false",animation_advanced?"true":"false",
        aim_observed,montage_first_position,montage_last_position,montage_observed_rate,
        catalog_profile,catalog_scan_slices,catalog_scan_max_us,
        native_natural?"true":"false",native_enrolled?"true":"false",offer_confirmed?"true":"false",
        stale_offer_rejected?"true":"false",native_candidates,enum_max_us,
        static_cast<unsigned>(first_offer.class_id),static_cast<unsigned>(first_offer.mesh_id),
        first_clean?"true":"false",reuse_enrolled?"true":"false",reuse_verified?"true":"false",second_clean?"true":"false",
        early_reuse_rejected?"true":"false",old_generation_rejected?"true":"false",natural_reuse_alive?"true":"false",
        reuse_path.c_str(),reuse_hp,offer.generation);
    if(bytes>0&&static_cast<unsigned>(bytes)<sizeof(text))write_file(report_path,text,static_cast<unsigned>(bytes));
    quitting=true;
    alignas(16) unsigned char parameters[32]{};
    std::memcpy(parameters,&context,8);
    event(cdo,fn,parameters);
}
static unsigned long long counter(const char* json,const char* key){
    const auto field=std::strstr(json,key);
    return field?std::strtoull(field+std::strlen(key),nullptr,10):0;
}
static void verify_montage_stop(){
    const auto status=read_native_status(core.status);
    if(counter(status.c_str(),"\"anim_stops\":")==1&&fixture_montage_stopped()){
        animation_stopped=true;
        write_file(stop_verified_marker.c_str(),"verified",8);
    }
}
static void receive_terminal(){
    const auto status=read_native_status(core.status);
    const auto applied=counter(status.c_str(),"\"death_applied\":");
    if(!applied)return;
    const auto visual=resolve(display);
    display_hidden=visual&&U::get_bool(reinterpret_cast<void*>(metadata.arguments[7]),reinterpret_cast<void*>(visual),0);
    const auto actor=resolve(enemy);
    death_actor_expired=!actor;
    if(actor){
        const auto hp_offset=at<int>(metadata.arguments[2],76);
        death_hp_zero=at<float>(actor,static_cast<unsigned>(hp_offset))==0.f;
    }
    if(applied!=1||!display_hidden||(!death_actor_expired&&!death_hp_zero)){phase=30;return;}
    write_file(death_verified_marker.c_str(),"verified",8);
    death_verified=true;
}

#include "retirement_fixture.inc"
#include "automatic_fixture.inc"
#include "steam_recovery_game_fixture.inc"
#include "player_fixture.inc"

static void steam_smoke_tick(){
    if(!resolve(title)||!resolve(library)||!resolve(quit)){title_step();return;}
    if(!started){
        if(!steam_rooms_opened){
            steam_invalid_rejected=!core.start_steam(1,0,5,0)&&!core.start_steam(0,1,0,0);
            if(!steam_invalid_rejected){phase=68;return;}
        }
        const bool joining=steam_rooms_left==1&&!steam_join_checked;
        const bool launched=joining?core.start_steam(0,steam_first_lobby,0,0):core.start_steam(1,0,4,0);
        if(!launched){
            if(!steam_rooms_left)phase=68;
            return;
        }
        steam_join_probe=joining;
        started=true;
    }
    SR::Status state{};
    if(steam_closing&&!core.stop())return;
    if(!core.steam_room(&state,sizeof(state))){phase=69;return;}
    if(steam_join_probe){
        if(state.phase!=static_cast<unsigned>(SR::Phase::failed))return;
        steam_closed_join_response=state.enter_response;
        if(state.failure!=static_cast<unsigned>(SR::Failure::call)||state.enter_response!=2){phase=74;return;}
        if(!core.stop())return;
        steam_join_probe=false;
        steam_join_checked=true;
        started=false;
        return;
    }
    if(state.phase==static_cast<unsigned>(SR::Phase::failed)){phase=70;return;}
    if(steam_closing){
        if(state.phase!=static_cast<unsigned>(SR::Phase::closed)||state.failure||state.left!=1){phase=72;return;}
        ++steam_rooms_left;
        steam_closing=false;
        if(steam_rooms_left<2){started=false;return;}
        passed=true;
        phase=3;
        return;
    }
    if(state.phase!=static_cast<unsigned>(SR::Phase::waiting))return;
    if(state.members!=1||state.limit!=4||state.expected!=4||!state.lobby||!state.nonce){phase=71;return;}
    ++steam_rooms_opened;
    if(steam_rooms_opened==1){
        steam_first_lobby=state.lobby;
        steam_first_nonce=state.nonce;
    }else{
        steam_rooms_distinct=steam_first_lobby!=state.lobby&&steam_first_nonce!=state.nonce;
        if(!steam_rooms_distinct){phase=73;return;}
    }
    steam_opened=state;
    steam_closing=true;
}

static void tick(void* engine,float dt,bool){
    if(quitting)return;
    if(!game_thread){
        game_thread=GetCurrentThreadId();
        deadline=GetTickCount64()+((reopen_lab||persistent_reload_lab)?100000:
                                 (player_bone_lab||world_music_lab||world_elevator_guest_lab||world_castle_gate_lab||world_arena_lab||world_cutscene_lab||world_boss_lab||world_lan_lab)?90000:
                                 player_gear_lab?65000:player_lab?45000:75000);
        LARGE_INTEGER frequency{};
        if(QueryPerformanceFrequency(&frequency))counter_us=1000000.0/static_cast<double>(frequency.QuadPart);
    }
    if(GetCurrentThreadId()!=game_thread){++wrong_thread;return;}
    if(phase==3||phase>=10){
        if(phase==3&&!host&&!peer_finished&&!player_lab)core.tick(dt);
        finish();
        return;
    }
    if(!host&&!player_lab&&peer_finished&&(!reopen_lab||reopen_stage!=ReopenStage::done)){
        phase=33;
        finish();
        return;
    }
    if(GetTickCount64()>=deadline){phase+=20;finish();return;}
    if(!initialized){
        if(!metadata.step(image+0x4A64260,image+0x4A27F00,read_local,assign))return;
        initialized=core.initialize(metadata.arguments.data(),static_cast<unsigned>(metadata.arguments.size()))!=0;
        if(!initialized){phase=17;return;}
    }
    if(steam_smoke){steam_smoke_tick();return;}
    if(phase==0){title_step();return;}
    if(phase==1){world_step();return;}
    if(recovery_lab){recovery_tick(engine,dt);return;}
    if(player_lab){player_tick(engine,dt);return;}
    if(automatic_lab){automatic_tick(engine,dt);return;}
    if(!native_enrolled||(!host&&!offer_confirmed)){
        const auto status=read_native_status(core.status);
        if(status.find("\"build\":-1,")!=std::string::npos){phase=49;return;}
        if(status.find("\"build\":1,")==std::string::npos)return;
        const auto context=resolve(player);
        if(!context){phase=49;return;}
        if(host){
            if(!prepare_natural_host(context))return;
            const auto kill=resolve(methods[kill_actor]);
            if(!kill){phase=34;return;}
            original_kill_exec=at<Address>(kill,216);
            if(!core.observe_kill()||at<Address>(kill,216)==original_kill_exec){phase=34;return;}
            kill_observer_installed=true;
        }else{
            if(!ready_published){
                write_file(ready_path,"ready",5);
                ready_published=true;
            }
            core.tick(dt);
            if(!native_enrolled&&!prepare_offered_replica(context))return;
            // Hold the third receipt long enough to require repeated offers.
            if(client_count==3&&role_index==3&&GetTickCount64()-offer_seen_at<500)return;
            offer_confirmed=core.confirm_replica(offer.id,offer.generation)!=0;
            return;
        }
    }
    // Keep polling the original game while a single lab client defers its
    // core consumption. The socket worker still receives retries.
    if(!host&&animation_stopped&&!death_verified&&client_count==3&&role_index==3){
        const auto published=death_publish_seen.load();
        if(!published||GetTickCount64()-published<650)return;
    }
    if(!host&&death_verified&&!first_clean&&client_count==3&&role_index==3){
        const auto published=retirement_notice_seen.load();
        if(!published||GetTickCount64()-published<650)return;
    }
    if(host&&!death_published&&
       !set_fixture_animation_markers(second_marker_sent?second_aim:first_aim,!second_marker_sent)){
        phase=39;
        return;
    }
    LARGE_INTEGER begin{},end{};
    QueryPerformanceCounter(&begin);
    core.tick(dt);
    QueryPerformanceCounter(&end);
    if(core_sample_count<core_us.size())
        core_us[core_sample_count++]=static_cast<double>(end.QuadPart-begin.QuadPart)*counter_us;
    ++active_ticks;
    if(host&&!active_since&&active_ticks%16==0){
        const auto status=read_native_status(core.status);
        if(counter(status.c_str(),"\"enroll_acked\":")==client_count)active_since=GetTickCount64();
    }
    if((host&&death_published)||(!host&&death_verified)){
        retirement_fixture_step();
        return;
    }
    if(!host&&alive_verified){
        if(animation_stopped)receive_terminal();
        else if(active_ticks%8==0)verify_montage_stop();
        return;
    }

    const auto actor=resolve(enemy);
    if(!actor){phase=18;return;}
    const auto hp_offset=at<int>(metadata.arguments[2],76);
    hp_after=at<float>(actor,static_cast<unsigned>(hp_offset));
    if(!host&&std::abs(hp_after-host_hp)<0.001f)first_snapshot_seen=true;
    if(host&&active_since&&!second_marker_sent&&GetTickCount64()-active_since>=1500){
        if(!start_fixture_montage()){phase=40;return;}
        std::memcpy(reinterpret_cast<void*>(actor+hp_offset),&final_hp,sizeof(final_hp));
        second_marker_sent=true;
    }
    if(!host)sample_fixture_animation();
    if(!host&&first_snapshot_seen&&std::abs(hp_after-final_hp)<0.001f&&
       animation_first&&animation_second&&animation_advanced){
        const auto visual=resolve(display);
        const auto pose=visual?object_field(visual,"SkeletalMeshComponent"):0;
        if(!pose){phase=31;return;}
        position_after=location(actor);
        display_position=component_location(pose);
        write_file(verified_path,"verified",8);
        alive_verified=true;
    }
    if(host&&second_marker_sent&&peers_alive_verified&&!animation_stopped){
        if(!stop_fixture_montage())phase=41;
        return;
    }
    if(host&&animation_stopped&&peers_montage_stopped){
        const auto kill=resolve(methods[kill_actor]);
        const auto mesh=object_field(actor,"Mesh");
        if(!kill||!mesh||!kill_observer_installed){phase=32;return;}
        position_after=location(actor);
        display_position=component_location(mesh);
        unsigned char args[2]{1,1};
        event(actor,kill,args);
        const auto status=read_native_status(core.status);
        if(counter(status.c_str(),"\"kill_queued\":")!=1||
           counter(status.c_str(),"\"kill_forwarded\":")!=1){phase=36;return;}
        if(!core.retire(1,1)){phase=52;return;}
        early_reuse_rejected=resolve(reuse_candidate)&&!core.enroll(1,2,&reuse_candidate);
        if(!early_reuse_rejected){phase=53;return;}
        death_published=true;
        write_file(death_marker.c_str(),"published",9);
    }
}
template<class Fn>static bool load(HMODULE module,const char* name,Fn& target){
    const auto address=GetProcAddress(module,name);
    std::memcpy(&target,&address,sizeof(target));
    return address!=nullptr;
}
static bool environment(const wchar_t* name,wchar_t* out,DWORD size){
    const auto length=GetEnvironmentVariableW(name,out,size);
    return length&&length<size;
}
static std::wstring marker(unsigned role,const wchar_t* file){
    const std::wstring_view name=file;
    const bool epoch_file=name.starts_with(L"player-first")||name.starts_with(L"player-moved")||
        name==L"lan-slot.bin"||name==L"lan-active.txt";
    const auto prefix=world_lan_lab&&fixture_lan_epoch==2&&epoch_file?L"epoch2-":L"";
    return std::wstring(run_root)+L"/"+role_dirs[role]+L"/"+prefix+file;
}
static bool exists(const std::wstring& path){
    const auto attributes=GetFileAttributesW(path.c_str());
    return attributes!=INVALID_FILE_ATTRIBUTES&&!(attributes&FILE_ATTRIBUTE_DIRECTORY);
}
static DWORD WINAPI start(void*){
    wchar_t path[32768]{},role[16]{},count[8]{};
    if(GetModuleHandleW(L"UE4SS.dll")||!environment(L"QD_NATIVE_CORE",path,32768)||
       !environment(L"QD_NATIVE_REPORT",report_path,32768)||
       !environment(L"QD_NATIVE_CORE_REPORT",core_path,32768)||
       !environment(L"QD_WORLD_ROLE",role,16)||!environment(L"QD_WORLD_CLIENTS",count,8)||
       !environment(L"QD_WORLD_ROOT",run_root,32768))return 0;
    if(count[0]<L'1'||count[0]>L'3'||count[1])return 0;
    client_count=static_cast<unsigned>(count[0]-L'0');
    for(;role_index<=client_count;++role_index)
        if(std::wcscmp(role,role_dirs[role_index])==0)break;
    if(role_index>client_count)return 0;
    host=role_index==0;
    wchar_t automatic[8]{};
    automatic_lab=environment(L"QD_WORLD_AUTOMATIC",automatic,8)&&std::wcscmp(automatic,L"1")==0;
    wchar_t reopen[8]{};
    reopen_lab=automatic_lab&&environment(L"QD_WORLD_REOPEN",reopen,8)&&std::wcscmp(reopen,L"1")==0;
    wchar_t steam[8]{};
    steam_smoke=host&&environment(L"QD_WORLD_STEAM_SMOKE",steam,8)&&std::wcscmp(steam,L"1")==0;
    wchar_t recovery[8]{};
    recovery_lab=host&&environment(L"QD_WORLD_RECOVERY_LAB",recovery,8)&&std::wcscmp(recovery,L"1")==0;
    wchar_t players[8]{};
    player_lab=environment(L"QD_WORLD_PLAYERS",players,8)&&std::wcscmp(players,L"1")==0;
    world_lan_lab=player_lab&&environment(L"QD_WORLD_LAN_IPV4",lan_ipv4,16);
    wchar_t body[8]{};
    player_body_lab=player_lab&&environment(L"QD_WORLD_PLAYER_BODY",body,8)&&std::wcscmp(body,L"1")==0;
    wchar_t rescue[8]{};
    player_rescue_lab=player_lab&&environment(L"QD_WORLD_PLAYER_RESCUE",rescue,8)&&std::wcscmp(rescue,L"1")==0;
    wchar_t medic[8]{};
    player_medic_lab=player_rescue_lab&&environment(L"QD_WORLD_PLAYER_MEDIC",medic,8)&&std::wcscmp(medic,L"1")==0;
    wchar_t replay[2048]{};
    player_medic_replay=player_lab&&environment(L"QD_WORLD_MEDIC_REPLAY",replay,std::size(replay));
    wchar_t presentation[8]{};
    presentation_lab=player_rescue_lab&&environment(L"QD_WORLD_PLAYER_PRESENTATION",presentation,8)&&
                     std::wcscmp(presentation,L"1")==0;
    wchar_t checkpoint[8]{};
    checkpoint_probe=player_lab&&environment(L"QD_WORLD_CHECKPOINT_PROBE",checkpoint,8)&&
                     std::wcscmp(checkpoint,L"1")==0;
    checkpoint_lab=player_lab&&environment(L"QD_WORLD_PLAYER_CHECKPOINT",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    checkpoint_refresh=checkpoint_lab&&environment(L"QD_WORLD_CHECKPOINT_REFRESH",checkpoint,8)&&
                       std::wcscmp(checkpoint,L"1")==0;
    checkpoint_wipe=checkpoint_lab&&environment(L"QD_WORLD_CHECKPOINT_WIPE",checkpoint,8)&&
                    std::wcscmp(checkpoint,L"1")==0;
    checkpoint_wipe_missing=checkpoint_wipe&&environment(L"QD_WORLD_CHECKPOINT_WIPE_MISSING",checkpoint,8)&&
                            std::wcscmp(checkpoint,L"1")==0;
    world_map_probe=player_lab&&environment(L"QD_WORLD_MAP_PROBE",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_progress_lab=player_lab&&environment(L"QD_WORLD_PROGRESS_LAB",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_actor_probe=player_lab&&environment(L"QD_WORLD_ACTOR_PROBE",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_chest_probe=world_actor_probe&&environment(L"QD_WORLD_CHEST_PROBE",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_chest_lab=player_lab&&environment(L"QD_WORLD_CHEST_LAB",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_mechanism_probe=player_lab&&environment(L"QD_WORLD_MECHANISM_PROBE",checkpoint,8)&&
                          std::wcscmp(checkpoint,L"1")==0;
    world_mechanism_lab=player_lab&&environment(L"QD_WORLD_MECHANISM_LAB",checkpoint,8)&&
                        std::wcscmp(checkpoint,L"1")==0;
    world_shrine_gate_lab=world_mechanism_lab&&environment(L"QD_WORLD_SHRINE_GATE_LAB",checkpoint,8)&&
                          std::wcscmp(checkpoint,L"1")==0;
    world_castle_gate_lab=player_lab&&environment(L"QD_WORLD_CASTLE_GATE_LAB",checkpoint,8)&&
                           std::wcscmp(checkpoint,L"1")==0;
    world_barrier_lab=world_castle_gate_lab&&environment(L"QD_WORLD_BARRIER_LAB",checkpoint,8)&&
                       std::wcscmp(checkpoint,L"1")==0;
    world_arena_lab=player_lab&&environment(L"QD_WORLD_ARENA_LAB",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_cutscene_lab=player_lab&&environment(L"QD_WORLD_CUTSCENE_LAB",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_boss_lab=player_lab&&environment(L"QD_WORLD_BOSS_LAB",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_boss_interact_lab=world_boss_lab&&environment(L"QD_WORLD_BOSS_INTERACT_LAB",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_pause_probe=player_lab&&environment(L"QD_WORLD_PAUSE_PROBE",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_pause_lab=player_lab&&environment(L"QD_WORLD_PAUSE_LAB",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_photo_probe=player_lab&&environment(L"QD_WORLD_PHOTO_PROBE",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_photo_lab=player_lab&&environment(L"QD_WORLD_PHOTO_LAB",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_damage_probe=player_lab&&environment(L"QD_WORLD_DAMAGE_PROBE",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_damage_stock=world_damage_probe&&environment(L"QD_WORLD_DAMAGE_STOCK",checkpoint,8)&&
                       std::wcscmp(checkpoint,L"1")==0;
    world_damage_lab=player_lab&&environment(L"QD_WORLD_DAMAGE_LAB",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_enemy_damage_lab=player_lab&&environment(L"QD_WORLD_ENEMY_DAMAGE_LAB",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_threat_lab=player_lab&&environment(L"QD_WORLD_THREAT_LAB",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_threat_memory_lab=world_threat_lab&&environment(L"QD_WORLD_THREAT_MEMORY_LAB",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_hitbox_lab=player_lab&&environment(L"QD_WORLD_HITBOX_LAB",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_host_melee_lab=world_hitbox_lab&&environment(L"QD_WORLD_HOST_MELEE_LAB",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_travel_lab=player_lab&&environment(L"QD_WORLD_TRAVEL_GUARD_LAB",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_stream_probe=player_lab&&environment(L"QD_WORLD_STREAM_PROBE",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_stream_replay=world_stream_probe&&environment(L"QD_WORLD_STREAM_REPLAY",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_stream_lab=player_lab&&environment(L"QD_WORLD_STREAM_LAB",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_arrival_lab=player_lab&&environment(L"QD_WORLD_ARRIVAL_LAB",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_arrival_cancel_lab=world_arrival_lab&&environment(L"QD_WORLD_ARRIVAL_CANCEL_LAB",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_music_lab=player_lab&&environment(L"QD_WORLD_MUSIC_LAB",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_story_lab=player_lab&&environment(L"QD_WORLD_STORY_LAB",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_library_lab=player_lab&&environment(L"QD_WORLD_LIBRARY_LAB",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_elevator_lab=player_lab&&environment(L"QD_WORLD_ELEVATOR_LAB",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    world_elevator_guest_lab=player_lab&&environment(L"QD_WORLD_ELEVATOR_GUEST_LAB",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    guest_safe_save_lab=player_lab&&environment(L"QD_WORLD_GUEST_SAFE_SAVE_LAB",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    persistent_save_probe=player_lab&&environment(L"QD_WORLD_PERSISTENT_SAVE_PROBE",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    persistent_reload_lab=player_lab&&environment(L"QD_WORLD_PERSISTENT_RELOAD_LAB",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    persistent_cross_lab=persistent_reload_lab&&environment(L"QD_WORLD_PERSISTENT_CROSS_LAB",checkpoint,8)&&std::wcscmp(checkpoint,L"1")==0;
    wchar_t interruption[8]{},refund[8]{};
    player_medic_interrupt=player_medic_lab&&environment(L"QD_WORLD_MEDIC_INTERRUPT",interruption,8)&&
                           std::wcscmp(interruption,L"1")==0;
    player_medic_refund=player_medic_replay&&environment(L"QD_WORLD_MEDIC_REPLAY_REFUND",refund,8)&&
                        std::wcscmp(refund,L"1")==0;
    wchar_t gear[8]{};
    player_gear_lab=player_lab&&environment(L"QD_WORLD_PLAYER_GEAR",gear,8)&&std::wcscmp(gear,L"1")==0;
    wchar_t bones[8]{};
    player_bone_lab=player_gear_lab&&environment(L"QD_WORLD_PLAYER_BONES",bones,8)&&std::wcscmp(bones,L"1")==0;
    if(!host&&(!environment(L"QD_WORLD_READY",ready_path,32768)||
              !environment(L"QD_WORLD_VERIFIED",verified_path,32768)))return 0;
    const auto module=LoadLibraryW(path);
    if(!module||!load(module,"QDEnemyInitializeNative",core.initialize)||
       !load(module,"QDEnemyStartNative",core.start)||
       !load(module,"QDEnemyStartNativeSteam",core.start_steam)||
       !load(module,"QDEnemyReadNativeSteamRoom",core.steam_room)||
       !load(module,"QDEnemyStartNativeLan",core.start_lan)||
       !load(module,"QDEnemyReadNativeLanRoom",core.lan_room)||
       !load(module,"QDEnemyPollNativeRecovery",core.recover)||
       !load(module,"QDEnemyConfigureNativeEnumeration",core.configure_enum)||
       !load(module,"QDEnemyEnumerateNativeEnemies",core.enumerate)||
       !load(module,"QDEnemyEnrollNativeEnemy",core.enroll)||
       !load(module,"QDEnemyReadNativeEnrollment",core.read_offer)||
       !load(module,"QDEnemyRegisterNativeReplica",core.register_replica)||
       !load(module,"QDEnemyConfirmNativeReplica",core.confirm_replica)||
       !load(module,"QDEnemyConfigureNativeReplicaDestruction",core.configure_destruction)||
       !load(module,"QDEnemyRetireNativeEnemy",core.retire)||
       !load(module,"QDEnemyPumpNativeWorld",core.pump_world)||
       !load(module,"QDEnemyInspectNativeBinding",core.inspect_binding)||
       !load(module,"QDEnemyBindNativeDisplay",core.bind_display)||
       !load(module,"QDEnemyConfigureNativeDeath",core.configure_death)||
       !load(module,"QDEnemyObserveNativeKill",core.observe_kill)||
       !load(module,"QDEnemyConfigureNativeOwnership",core.configure_ownership)||
       !load(module,"QDEnemyTakeNativeOwnership",core.take_ownership)||
       !load(module,"QDEnemyReleaseNativeOwnership",core.release_ownership)||
       !load(module,"QDEnemyScanNativeAnimation",core.scan_animation)||
       !load(module,"QDEnemyBindCatalogNativeAnimation",core.bind_catalog_animation)||
       !load(module,"QDEnemyNativeStatus",core.status)||!load(module,"QDEnemyStopNative",core.stop)||
       !load(module,"QDEnemyEngineTick",core.tick))return 0;
    if(recovery_lab&&(!load(module,"QDEnemyProbeStartRecovery",core.start_recovery)||
                      !load(module,"QDEnemyProbeChangeRecoveryRoster",core.change_recovery_roster)))return 0;
    if(player_lab&&!load(module,"QDEnemyInspectNativePlayer",core.inspect_player))return 0;
    if((player_rescue_lab||checkpoint_wipe)&&!load(module,"QDEnemyInspectNativeRescue",core.inspect_rescue))return 0;
    if((player_medic_lab||player_medic_replay)&&!load(module,"QDEnemyInspectNativeMedic",core.inspect_medic))return 0;
    if((checkpoint_lab||guest_safe_save_lab)&&!load(module,"QDEnemyInspectNativeCheckpoint",core.inspect_checkpoint))return 0;
    if(player_gear_lab&&(!load(module,"QDEnemyInspectNativeEquipment",core.inspect_equipment)||
                        !load(module,"QDEnemySetNativeSkin",core.set_skin)))return 0;
    if(player_bone_lab&&!load(module,"QDEnemyInspectNativeEquipmentPose",core.inspect_gear_pose))return 0;
    image=reinterpret_cast<Address>(GetModuleHandleW(nullptr));
    death_marker=marker(0,L"death-published.txt");
    death_verified_marker=marker(role_index,L"death-verified.txt");
    stop_verified_marker=marker(role_index,L"montage-stopped.txt");
    retirement_notice_marker=marker(0,L"retire-published.txt");
    reuse_verified_marker=marker(role_index,L"reuse-verified.txt");
    final_clean_marker=marker(role_index,L"final-clean.txt");
    if(!QDInstallNativeTick(&tick))return 0;
    std::array<std::wstring,3> ready,verified,stopped,reused,terminal;
    for(unsigned i=0;i<client_count;++i){
        ready[i]=marker(i+1,L"socket-ready.txt");
        verified[i]=marker(i+1,L"apply-verified.txt");
        stopped[i]=marker(i+1,L"montage-stopped.txt");
        reused[i]=marker(i+1,L"reuse-verified.txt");
        terminal[i]=marker(i+1,L"final-clean.txt");
    }
    const auto stopped_report=marker(0,L"native-world.json");
    for(unsigned attempt=0;attempt<850;++attempt){
        if(host){
            if(std::all_of(ready.begin(),ready.begin()+client_count,exists))peer_ready=true;
            if(std::all_of(verified.begin(),verified.begin()+client_count,exists))peers_alive_verified=true;
            if(std::all_of(stopped.begin(),stopped.begin()+client_count,exists))peers_montage_stopped=true;
            if(std::all_of(reused.begin(),reused.begin()+client_count,exists))peers_reuse_verified=true;
            if(std::all_of(terminal.begin(),terminal.begin()+client_count,exists)){peer_finished=true;break;}
        }else{
            if(!death_publish_seen&&exists(death_marker))death_publish_seen=GetTickCount64();
            if(!retirement_notice_seen&&exists(retirement_notice_marker))retirement_notice_seen=GetTickCount64();
            if(exists(stopped_report)){peer_finished=true;break;}
        }
        Sleep(100);
    }
    return 0;
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){
    if(reason==DLL_PROCESS_ATTACH){
        DisableThreadLibraryCalls(instance);
        const auto thread=CreateThread(nullptr,0,start,nullptr,0,nullptr);
        if(thread)CloseHandle(thread);
    }
    return TRUE;
}
