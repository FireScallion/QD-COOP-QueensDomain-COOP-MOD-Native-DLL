// Only the pinned game thread may bind a playable world or invoke its functions.
namespace NativeWorld=qdcoop::native_world;
enum WorldObject {
    world_game_instance,world_level,world_type,world_player_type,world_player_pawn,world_pawn_local,
    world_enumerate,world_destroy,world_kill,world_actor_tick,world_component_tick,
    world_player_asset_first,world_kismet=world_player_asset_first,world_make_path,world_soft_ref,
    world_load_asset,world_play_animation,world_make_class,world_soft_class,world_load_class,
    world_anim_instance,world_montage_layout,world_control_rotation,
    world_scene_parent,world_scene_socket,world_socket_transform,world_math,world_make_relative,
    world_strings,world_string_name,world_static_component,world_pose_component,world_static_asset,
    world_material_type,world_mesh_component,world_add_component,world_attach_component,world_destroy_component,
    world_set_static,world_component_collision,world_component_overlap,world_relative_set,world_material_count,
    world_get_material,world_set_material,world_material_slot,
    world_bone_first,world_copy_pose=world_bone_first,world_get_bone_index,world_get_bone_count,
    world_life_first,world_life_health=world_life_first,world_life_cancel,world_life_cure,world_life_interact,
    world_life_block,world_life_unblock,world_life_controller,world_life_ignore,world_life_stop,
    world_life_disable,world_life_mode,world_life_visible,world_life_visibility,world_life_trace,
    world_life_relative,world_life_count,
    world_medic_first,world_medic_save_index=world_medic_first,world_medic_save_name,world_medic_utils,
    world_medic_save_type,world_medic_write,world_medic_read,world_medic_soft,world_medic_consume,
    world_medic_credit,world_medic_functions,world_medic_map_add,world_medic_map_find,
    world_medic_refresh,world_medic_count_use,world_medic_potion,world_medic_herb,
    world_view_first,world_view_move=world_view_first,world_view_rotate,world_view_capsule,
    world_view_manager,world_view_location,world_view_hud,world_view_highlight,world_view_power,
    world_view_dynamic,world_view_scalar,world_view_read_scalar,world_view_pitch_input,world_view_yaw_input,
    world_view_pitch_multiplier,world_view_yaw_multiplier,world_view_pitch,world_view_yaw,
    world_view_mouse_pitch,world_view_mouse_yaw,world_view_uber,world_view_delta,world_view_tick,
    world_cp_first,world_cp_save=world_cp_first,world_cp_save_bp,world_cp_status,world_cp_travel,
    world_progress_remove,world_actor_first,world_actor_chest=world_actor_first,world_actor_restore,
    world_actor_clear,world_actor_collision,world_actor_play,world_actor_playing,world_actor_play_sound,
    world_actor_sound_type,world_breakable_barricade,world_breakable_permanent,
    world_mechanism_first,world_mechanism_door=world_mechanism_first,
    world_mechanism_lever,world_mechanism_open,world_mechanism_close,world_mechanism_pull,world_mechanism_reset,
    world_quest_first,world_quest_notify=world_quest_first,world_quest_update,
    world_pause_first,world_pause_mode=world_pause_first,world_pause_game_get,world_pause_game_set,
    world_pause_menu_set,world_pause_enemy_set,world_pause_overlay,
    world_modal_first,world_modal_global_get=world_modal_first,world_modal_global_set,
    world_modal_find,world_modal_library,world_modal_type,world_modal_viewport,world_modal_visible,
    world_damage_first,world_damage_enemy=world_damage_first,world_damage_proxy_class,
    world_damage_player,world_damage_proxy_default,
    world_hit_first,world_hit_primitive=world_hit_first,world_hit_object_type,world_hit_response,
    world_hit_set_all,world_hit_set_response,world_hit_profile,
    world_threat_first,world_threat_get_gs=world_threat_first,world_threat_gs_type,
    world_threat_register,world_threat_unregister,
    world_threat_height,world_threat_forward,world_threat_awareness,world_threat_aggro,
    world_threat_unaggro,world_threat_los_update,world_threat_stop,world_threat_tick_get,
    world_threat_chase,world_threat_last_seen,
    world_travel_ui,world_travel_save_ui,
    world_stream_manager,world_stream_list,world_stream_restore,world_stream_flush,world_stream_busy,
    world_stream_control,
    world_travel_create_save,
    world_music_type,world_music_cue,world_music_override,world_music_clear,
    world_guest_save_levels,world_guest_save_music,
    world_shrine_timeline,world_shrine_playing,world_shrine_reversing,world_shrine_position,world_shrine_length,
    world_shrine_stop,world_shrine_seek,
    world_library_book,world_library_gate,
    world_elevator_type,world_elevator_lever,world_elevator_start,world_elevator_update,world_elevator_location,
    world_barrier_primitive,world_barrier_relative,world_barrier_hidden,
    world_barrier_actor,world_barrier_actor_get,world_barrier_actor_set,world_barrier_actor_hidden,world_barrier_actor_parts,
    world_barrier_bounds,world_barrier_scale,world_barrier_data_float,world_barrier_data_vector,
    world_barrier_necro,world_barrier_desert,world_barrier_library,world_barrier_aqua_exit,world_barrier_grave,world_barrier_aqua_entry,
    world_projectile_visual,world_projectile_mobility,world_projectile_scene,world_projectile_arc,
    world_projectile_niagara,world_projectile_system,world_projectile_fx_library,world_projectile_spawn_fx,
    world_projectile_is_active,world_projectile_activate,world_projectile_deactivate,world_projectile_spawn_location,
    world_projectile_set_active,
    world_projectile_niagara_float,world_projectile_niagara_string_float,world_projectile_fx_float,
    world_projectile_niagara_bool,world_projectile_niagara_string_bool,
    world_projectile_niagara_int,world_projectile_niagara_string_int,
    world_projectile_niagara_vec3,world_projectile_niagara_string_vec3,
    world_projectile_niagara_color,world_projectile_niagara_string_color,
    world_projectile_niagara_set_asset,
    world_projectile_math,world_projectile_make_transform,
    world_projectile_cascade_component,world_projectile_cascade_asset,
    world_projectile_cascade_location,world_projectile_cascade_attached,
    world_sound_component,world_sound_attenuation,world_sound_at_location,world_sound_attached,world_sound_stop,
    world_sound_play_2d,world_sound_spawn_2d,world_sound_create_2d,
    world_sound_play,world_sound_playing,world_sound_volume,world_sound_pitch,world_sound_fade_out,world_sound_fade_in,
    world_placed_map_clear,world_placed_sphere,world_placed_item_type,world_placed_suspend,world_placed_sphere_radius,
    world_body_scalar,world_mount_master,world_mount_anim,world_mount_children,
    world_hud_enemy_damage,world_hud_boss_damage,world_hud_boss_add,world_hud_boss_remove,world_hud_group_type,
    world_hud_stop,world_hud_clear,world_hud_opacity,
    world_reward_drop,world_reward_drop_library,world_reward_soft_string,world_reward_fog,world_reward_item_type,
    world_reward_add_xp,
    world_cine_base,world_cine_begin,world_cine_skip,world_cine_sequence,world_cine_graph,world_cine_signature,
    world_cine_play,world_cine_pause,world_cine_time,world_cine_seek,
    world_cine_gameplay,world_cine_stream_get,world_cine_stream_load,world_cine_stream_show,world_cine_stream_visible,
    world_cine_lock,world_cine_unlock,world_cine_hide,world_cine_unhide,
    world_cine_first,world_cine_ending1=world_cine_first,world_cine_ending2,world_cine_ending3,
    world_cine_ending4,world_cine_ritual,world_cine_sipeha,
    world_object_count
};
static constexpr std::string_view world_paths[]{
    "/Script/Engine.GameInstance",
    "/Script/Engine.Level",
    "/Script/Engine.World",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C",
    "/Script/Engine.GameplayStatics.GetPlayerPawn",
    "/Script/Engine.Pawn.IsLocallyControlled",
    "/Script/Engine.GameplayStatics.GetAllActorsOfClass",
    "/Script/Engine.Actor.K2_DestroyActor",
    "/Game/Enemies/EnemyBase.EnemyBase_C.Kill",
    "/Script/Engine.Actor.SetActorTickEnabled",
    "/Script/Engine.ActorComponent.SetComponentTickEnabled",
    "/Script/Engine.Default__KismetSystemLibrary",
    "/Script/Engine.KismetSystemLibrary.MakeSoftObjectPath",
    "/Script/Engine.KismetSystemLibrary.Conv_SoftObjPathToSoftObjRef",
    "/Script/Engine.KismetSystemLibrary.LoadAsset_Blocking",
    "/Script/Engine.SkeletalMeshComponent.PlayAnimation",
    "/Script/Engine.KismetSystemLibrary.MakeSoftClassPath",
    "/Script/Engine.KismetSystemLibrary.Conv_SoftClassPathToSoftClassRef",
    "/Script/Engine.KismetSystemLibrary.LoadClassAsset_Blocking",
    "/Script/Engine.AnimInstance",
    "/Script/Engine.AnimMontageInstance",
    "/Script/Engine.Pawn.GetControlRotation",
    "/Script/Engine.SceneComponent.GetAttachParent",
    "/Script/Engine.SceneComponent.GetAttachSocketName",
    "/Script/Engine.SceneComponent.GetSocketTransform",
    "/Script/Engine.Default__KismetMathLibrary",
    "/Script/Engine.KismetMathLibrary.MakeRelativeTransform",
    "/Script/Engine.Default__KismetStringLibrary",
    "/Script/Engine.KismetStringLibrary.Conv_StringToName",
    "/Script/Engine.StaticMeshComponent",
    "/Script/Engine.PoseableMeshComponent",
    "/Script/Engine.StaticMesh",
    "/Script/Engine.MaterialInterface",
    "/Script/Engine.MeshComponent",
    "/Script/Engine.Actor.AddComponentByClass",
    "/Script/Engine.SceneComponent.K2_AttachToComponent",
    "/Script/Engine.ActorComponent.K2_DestroyComponent",
    "/Script/Engine.StaticMeshComponent.SetStaticMesh",
    "/Script/Engine.PrimitiveComponent.SetCollisionEnabled",
    "/Script/Engine.PrimitiveComponent.SetGenerateOverlapEvents",
    "/Script/Engine.SceneComponent.K2_SetRelativeTransform",
    "/Script/Engine.PrimitiveComponent.GetNumMaterials",
    "/Script/Engine.PrimitiveComponent.GetMaterial",
    "/Script/Engine.PrimitiveComponent.SetMaterial",
    "/Script/Engine.MeshComponent.GetMaterialIndex",
    "/Script/Engine.PoseableMeshComponent.CopyPoseFromSkeletalComponent",
    "/Script/Engine.SkinnedMeshComponent.GetBoneIndex",
    "/Script/Engine.SkinnedMeshComponent.GetNumBones",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.Set Health Points",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.CancelCurrentAttacks",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.CureAllNegativeEffects",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.CanInteractFlags",
    "/Game/Blueprints/FS_GameInstance.FS_GameInstance_C.BlockSuspendSaves",
    "/Game/Blueprints/FS_GameInstance.FS_GameInstance_C.UnBlockSuspendSaves",
    "/Script/Engine.Pawn.GetController",
    "/Script/Engine.Controller.SetIgnoreMoveInput",
    "/Script/Engine.MovementComponent.StopMovementImmediately",
    "/Script/Engine.CharacterMovementComponent.DisableMovement",
    "/Script/Engine.CharacterMovementComponent.SetMovementMode",
    "/Script/Engine.SceneComponent.IsVisible",
    "/Script/Engine.SceneComponent.SetVisibility",
    "/Script/Engine.KismetSystemLibrary.LineTraceSingle",
    "/Script/Engine.SceneComponent.K2_SetRelativeLocation",
    "/Game/Blueprints/FS_GameInstance.FS_GameInstance_C.ReadInventoryItemCount",
    "/Game/Blueprints/FS_GameInstance.FS_GameInstance_C.GetActiveSaveSlotIndex",
    "/Script/QueensDomain.BlueprintUtils.GetSaveSlotName",
    "/Script/QueensDomain.Default__BlueprintUtils",
    "/Game/Blueprints/SaveSystem/FS_SaveSlot.FS_SaveSlot_C",
    "/Script/Engine.GameplayStatics.SaveGameToSlot",
    "/Script/Engine.GameplayStatics.LoadGameFromSlot",
    "/Script/Engine.KismetSystemLibrary.Conv_ObjectToSoftObjectReference",
    "/Game/Blueprints/SaveSystem/SaveFunctions.SaveFunctions_C.TryConsumingItemFromInventory",
    "/Game/Blueprints/SaveSystem/SaveFunctions.SaveFunctions_C.AddItemToInventory",
    "/Game/Blueprints/SaveSystem/SaveFunctions.Default__SaveFunctions_C",
    "/Script/Engine.BlueprintMapLibrary.Map_Add",
    "/Script/Engine.BlueprintMapLibrary.Map_Find",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.RefreshSelectedConsumable",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.CountUsedConsumable",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.ApplyHealingPotion",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.ApplyHerb",
    "/Script/Engine.SceneComponent.K2_SetWorldLocation",
    "/Script/Engine.Actor.K2_SetActorRotation",
    "/Script/Engine.CapsuleComponent.GetScaledCapsuleHalfHeight",
    "/Script/Engine.GameplayStatics.GetPlayerCameraManager",
    "/Script/Engine.PlayerCameraManager.GetCameraLocation",
    "/Script/Engine.PlayerController.GetHUD",
    "/Game/Player/Assets/Input/FS_HUD.FS_HUD_C.CanInteractHighlight",
    "/Game/UI/HUD/Widgets/StatusCluster.StatusCluster_C.UpdatePowerMeter",
    "/Script/UMG.Image.GetDynamicMaterial",
    "/Script/Engine.MaterialInstanceDynamic.SetScalarParameterValue",
    "/Script/Engine.MaterialInstanceDynamic.K2_GetScalarParameterValue",
    "/Script/Engine.Pawn.AddControllerPitchInput",
    "/Script/Engine.Pawn.AddControllerYawInput",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.GetVerticalInputMultiplier",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.Get Horizontal Input Multiplier",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.HandlePitchInput",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.HandleYawInput",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.InpAxisEvt_LookUpMouse_K2Node_InputAxisEvent_2",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.InpAxisEvt_TurnMouse_K2Node_InputAxisEvent_3",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.ExecuteUbergraph_FS_PlayerChar",
    "/Script/Engine.GameplayStatics.GetWorldDeltaSeconds",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.Tick Camera",
    "/Game/Blueprints/FS_GameInstance.FS_GameInstance_C.GI_SaveGameToDisk",
    "/Game/Blueprints/FS_GameInstance.FS_GameInstance_C.Save Game to Disk",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.Player_WriteStatusToSave",
    "/Script/Engine.GameplayStatics.OpenLevel",
    "/Script/Engine.BlueprintMapLibrary.Map_Remove",
    "/Game/Props/Chest_00/Chest.Chest_C",
    "/Game/Blueprints/SaveSystem/SavableActor.SavableActor_C.RestoreFromSave",
    "/Script/Engine.KismetArrayLibrary.Array_Clear",
    "/Script/Engine.PrimitiveComponent.GetCollisionEnabled",
    "/Script/Engine.TimelineComponent.PlayFromStart",
    "/Script/Engine.TimelineComponent.IsPlaying",
    "/Script/Engine.GameplayStatics.PlaySoundAtLocation",
    "/Script/Engine.SoundBase",
    "/Game/Blueprints/BreakableBarricade/BreakableBarricade.BreakableBarricade_C",
    "/Game/Props/Containers/Permabreakable_base.Permabreakable_base_C",
    "/Game/Blueprints/Door_Base.Door_Base_C",
    "/Game/LD45/Blueprints/Objects/Gate/Lever.Lever_C",
    "/Game/Blueprints/Door_Base.Door_Base_C.OpenDoor",
    "/Game/Blueprints/Door_Base.Door_Base_C.CloseDoor",
    "/Game/LD45/Blueprints/Objects/Gate/Lever.Lever_C.AnimateLeverPull",
    "/Game/LD45/Blueprints/Objects/Gate/Lever.Lever_C.AnimateLeverReset",
    "/Game/Blueprints/FS_GameInstance.FS_GameInstance_C.GI_NotifyWorldStateChanged",
    "/Game/Blueprints/FS_GameInstance.FS_GameInstance_C.GI_UpdateQuestState",
    "/Script/Engine.GameplayStatics.GetGameMode",
    "/Script/Engine.GameplayStatics.IsGamePaused",
    "/Script/Engine.GameplayStatics.SetGamePaused",
    "/Game/LD45/Blueprints/Base_LD45_GM.Base_LD45_GM_C.SetPauseTimeDilation",
    "/Game/LD45/Blueprints/Base_LD45_GM.Base_LD45_GM_C.SetPausedEnemyTimeDilation",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.CheckForOverlayPause",
    "/Script/Engine.GameplayStatics.GetGlobalTimeDilation",
    "/Script/Engine.GameplayStatics.SetGlobalTimeDilation",
    "/Script/UMG.WidgetBlueprintLibrary.GetAllWidgetsOfClass",
    "/Script/UMG.Default__WidgetBlueprintLibrary",
    "/Script/UMG.UserWidget",
    "/Script/UMG.UserWidget.IsInViewport",
    "/Script/UMG.Widget.IsVisible",
    "/Game/Enemies/EnemyBase.EnemyBase_C.TryApplyDamage",
    "/Game/QDCoop/QDCoopGuestTarget.QDCoopGuestTarget_C",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.TryApplyDamage",
    "/Game/QDCoop/QDCoopGuestTarget.Default__QDCoopGuestTarget_C",
    "/Script/Engine.PrimitiveComponent",
    "/Script/Engine.PrimitiveComponent.GetCollisionObjectType",
    "/Script/Engine.PrimitiveComponent.GetCollisionResponseToChannel",
    "/Script/Engine.PrimitiveComponent.SetCollisionResponseToAllChannels",
    "/Script/Engine.PrimitiveComponent.SetCollisionResponseToChannel",
    "/Script/Engine.PrimitiveComponent.SetCollisionProfileName",
    "/Script/Engine.GameplayStatics.GetGameState",
    "/Script/FBAttributes.FB_GameState",
    "/Script/FBAttributes.FB_GameState.RegisterDamageableActor",
    "/Script/FBAttributes.FB_GameState.UnregisterDamageableActor",
    "/Game/Enemies/EnemyBase.EnemyBase_C.GetAggroHeight",
    "/Game/Enemies/EnemyBase.EnemyBase_C.GetAIForwardVector",
    "/Script/QueensDomain.EnemyAwarenessProfile.GetRealMaxRangeForProfile",
    "/Game/Enemies/EnemyBase.EnemyBase_C.ForceAggroupdate",
    "/Game/Enemies/EnemyBase.EnemyBase_C.ForceUnaggro",
    "/Script/QueensDomain.EnemyLineOfSightComponent.UpdateLOSParams",
    "/Script/Engine.Controller.StopMovement",
    "/Script/Engine.ActorComponent.IsComponentTickEnabled",
    "/Game/Enemies/EnemyBase.EnemyBase_C.GetChaseTarget",
    "/Game/Enemies/EnemyBase.EnemyBase_C.GetLastSeenPlayerLoc",
    "/Game/UI/Menus/Teleporter/TeleporterUI.TeleporterUI_C",
    "/Game/UI/Menus/SaveMenu.SaveMenu_C",
    "/Script/QueensDomain.FS_LevelManager",
    "/Script/QueensDomain.FS_LevelList",
    "/Script/QueensDomain.FS_LevelManager.RestoreFromSuspendState",
    "/Script/QueensDomain.FS_LevelManager.FlushLevelManagerActions",
    "/Script/QueensDomain.FS_LevelManager.IsProcessingLevelChanges",
    "/Script/Engine.Controller.SetControlRotation",
    "/Script/Engine.GameplayStatics.CreateSaveGameObject",
    "/Game/Blueprints/MusicControl/MusicController.MusicController_C",
    "/Script/Engine.SoundCue",
    "/Game/Blueprints/MusicControl/MusicController.MusicController_C.OverrideTrack",
    "/Game/Blueprints/MusicControl/MusicController.MusicController_C.ClearOverrideTrack",
    "/Script/QueensDomain.FS_LevelManager.GetSafeLocationSuspendState",
    "/Script/QueensDomain.FS_MusicSystemBase.GetSafeLocationSuspendState",
    "/Script/Engine.TimelineComponent",
    "/Script/Engine.TimelineComponent.IsPlaying",
    "/Script/Engine.TimelineComponent.IsReversing",
    "/Script/Engine.TimelineComponent.GetPlaybackPosition",
    "/Script/Engine.TimelineComponent.GetTimelineLength",
    "/Script/Engine.TimelineComponent.Stop",
    "/Script/Engine.TimelineComponent.SetPlaybackPosition",
    "/Game/Items/KeyItems/BookKeys/LIB_BookRecepticle_BP.LIB_BookRecepticle_BP_C",
    "/Game/RuinsLevel/Library/BossDoor/Lib_TreasureGate_BP.Lib_TreasureGate_BP_C",
    "/Game/Blueprints/Elevator/Elevator_Base.Elevator_Base_C",
    "/Game/Blueprints/Elevator/ElevatorLever.ElevatorLever_C",
    "/Game/Blueprints/Elevator/Elevator_Base.Elevator_Base_C.StartElevatorTowardsEnd",
    "/Game/Blueprints/Elevator/Elevator_Base.Elevator_Base_C.UpdateLevers",
    "/Script/Engine.SceneComponent.K2_GetComponentLocation",
    "/Script/Engine.PrimitiveComponent",
    "/Script/Engine.SceneComponent.GetRelativeTransform",
    "/Script/Engine.SceneComponent.SetHiddenInGame",
    "/Script/Engine.Actor",
    "/Script/Engine.Actor.GetActorEnableCollision",
    "/Script/Engine.Actor.SetActorEnableCollision",
    "/Script/Engine.Actor.SetActorHiddenInGame",
    "/Script/Engine.Actor.K2_GetComponentsByClass",
    "/Script/Engine.StaticMeshComponent.GetLocalBounds",
    "/Script/Engine.SceneComponent.K2_GetComponentScale",
    "/Script/Engine.PrimitiveComponent.SetCustomPrimitiveDataFloat",
    "/Script/Engine.PrimitiveComponent.SetCustomPrimitiveDataVector3",
    "/Game/Enemies/Necronomicon/Necro_Boss_Door.Necro_Boss_Door_C",
    "/Game/Enemies/JosamiKnight_00/DesBoss/DesBoss_Gate.DesBoss_Gate_C",
    "/Game/RuinsLevel/Library/BossDoor/Lib_BossDoor_BP.Lib_BossDoor_BP_C",
    "/Game/RuinsLevel/AquaRuins/AquaBossExitDoor.AquaBossExitDoor_C",
    "/Game/RuinsLevel/AquaRuins/GraveyardBoss_Gate.GraveyardBoss_Gate_C",
    "/Game/RuinsLevel/AquaRuins/BossDoor/AquaBossDoor_BP.AquaBossDoor_BP_C",
    "/Script/Engine.StaticMeshActor",
    "/Script/Engine.SceneComponent.SetMobility",
    "/Script/Engine.SceneComponent",
    "/Game/Items/Weapons/_Shared/ArcProjectile_Base.ArcProjectile_Base_C",
    "/Script/Niagara.NiagaraComponent",
    "/Script/Niagara.NiagaraSystem",
    "/Script/Niagara.Default__NiagaraFunctionLibrary",
    "/Script/Niagara.NiagaraFunctionLibrary.SpawnSystemAttached",
    "/Script/Engine.ActorComponent.IsActive",
    "/Script/Engine.ActorComponent.Activate",
    "/Script/Engine.ActorComponent.Deactivate",
    "/Script/Niagara.NiagaraFunctionLibrary.SpawnSystemAtLocation",
    "/Script/Engine.ActorComponent.SetActive",
    "/Script/Niagara.NiagaraComponent.SetVariableFloat",
    "/Script/Niagara.NiagaraComponent.SetNiagaraVariableFloat",
    "/Script/Engine.FXSystemComponent.SetFloatParameter",
    "/Script/Niagara.NiagaraComponent.SetVariableBool",
    "/Script/Niagara.NiagaraComponent.SetNiagaraVariableBool",
    "/Script/Niagara.NiagaraComponent.SetVariableInt",
    "/Script/Niagara.NiagaraComponent.SetNiagaraVariableInt",
    "/Script/Niagara.NiagaraComponent.SetVariableVec3",
    "/Script/Niagara.NiagaraComponent.SetNiagaraVariableVec3",
    "/Script/Niagara.NiagaraComponent.SetVariableLinearColor",
    "/Script/Niagara.NiagaraComponent.SetNiagaraVariableLinearColor",
    "/Script/Niagara.NiagaraComponent.SetAsset",
    "/Script/Engine.Default__KismetMathLibrary",
    "/Script/Engine.KismetMathLibrary.MakeTransform",
    "/Script/Engine.ParticleSystemComponent",
    "/Script/Engine.ParticleSystem",
    "/Script/Engine.GameplayStatics.SpawnEmitterAtLocation",
    "/Script/Engine.GameplayStatics.SpawnEmitterAttached",
    "/Script/Engine.AudioComponent",
    "/Script/Engine.SoundAttenuation",
    "/Script/Engine.GameplayStatics.SpawnSoundAtLocation",
    "/Script/Engine.GameplayStatics.SpawnSoundAttached",
    "/Script/Engine.AudioComponent.Stop",
    "/Script/Engine.GameplayStatics.PlaySound2D",
    "/Script/Engine.GameplayStatics.SpawnSound2D",
    "/Script/Engine.GameplayStatics.CreateSound2D",
    "/Script/Engine.AudioComponent.Play",
    "/Script/Engine.AudioComponent.IsPlaying",
    "/Script/Engine.AudioComponent.SetVolumeMultiplier",
    "/Script/Engine.AudioComponent.SetPitchMultiplier",
    "/Script/Engine.AudioComponent.FadeOut",
    "/Script/Engine.AudioComponent.FadeIn",
    "/Script/Engine.BlueprintMapLibrary.Map_Clear",
    "/Script/Engine.SphereComponent",
    "/Game/Blueprints/ItemSystem/FS_ItemData.FS_ItemData_C",
    "/Game/Blueprints/FS_GameInstance.FS_GameInstance_C.GI_WriteSuspendSaveImmediately",
    "/Script/Engine.SphereComponent.SetSphereRadius",
    "/Script/Engine.MeshComponent.SetScalarParameterValueOnMaterials",
    "/Script/Engine.SkinnedMeshComponent.SetMasterPoseComponent",
    "/Script/Engine.SkeletalMeshComponent.SetAnimInstanceClass",
    "/Script/Engine.SceneComponent.GetChildrenComponents",
    "/Game/UI/HUD/Widgets/EnemyHealthBar/EnemyHealthBar_base.EnemyHealthBar_base_C.Damaged",
    "/Game/UI/HUD/Widgets/EnemyHealthBar/BossHealthBar.BossHealthBar_C.Damaged",
    "/Game/UI/HUD/Widgets/EnemyHealthBar/BossHealthBar.BossHealthBar_C.AddTarget",
    "/Game/UI/HUD/Widgets/EnemyHealthBar/BossHealthBar.BossHealthBar_C.Deactivate",
    "/Game/Enemies/Shared/EnemyData.EnemyData_C",
    "/Script/UMG.UserWidget.StopAllAnimations",
    "/Script/UMG.PanelWidget.ClearChildren",
    "/Script/UMG.Widget.SetRenderOpacity",
    "/Game/Blueprints/ItemSystem/SpawnItemDrop.SpawnItemDrop_C.TrySpawningItem",
    "/Game/Blueprints/ItemSystem/SpawnItemDrop.Default__SpawnItemDrop_C",
    "/Script/Engine.KismetSystemLibrary.Conv_SoftObjectReferenceToString",
    "/Game/Enemies/EnemyBase.EnemyBase_C.SetUpDropFog",
    "/Script/QueensDomain.FS_Item",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.AddXP",
    "/Game/Cutscenes/CutsceneManagerBase.CutsceneManagerBase_C",
    "/Game/Cutscenes/CutsceneManagerBase.CutsceneManagerBase_C.BeginCutscene",
    "/Game/Cutscenes/CutsceneManagerBase.CutsceneManagerBase_C.SkipCutscene",
    "/Game/Cutscenes/CutsceneManagerBase.CutsceneManagerBase_C.GetSequencePlayer",
    "/Game/Cutscenes/CutsceneManagerBase.CutsceneManagerBase_C.ExecuteUbergraph_CutsceneManagerBase",
    "/Game/Cutscenes/CutsceneManagerBase.CutsceneManagerBase_C.CutsceneFinished__DelegateSignature",
    "/Script/MovieScene.MovieSceneSequencePlayer.Play",
    "/Script/MovieScene.MovieSceneSequencePlayer.Pause",
    "/Script/MovieScene.MovieSceneSequencePlayer.GetCurrentTime",
    "/Script/MovieScene.MovieSceneSequencePlayer.SetPlaybackPosition",
    "/Script/Engine.Default__GameplayStatics",
    "/Script/Engine.GameplayStatics.GetStreamingLevel",
    "/Script/Engine.LevelStreaming.SetShouldBeLoaded",
    "/Script/Engine.LevelStreaming.SetShouldBeVisible",
    "/Script/Engine.LevelStreaming.IsLevelVisible",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.LockInputForLoadingOrCutscene",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.UnlockInputForLoadingOrCutscene",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.HideHUD",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.UnhideHUD",
    "/Game/Cutscenes/Ending1/Ending1CutsceneManager.Ending1CutsceneManager_C",
    "/Game/Cutscenes/Ending2/Ending2CutsceneManager.Ending2CutsceneManager_C",
    "/Game/Cutscenes/Ending3/Ending3CutsceneManager.Ending3CutsceneManager_C",
    "/Game/Cutscenes/Ending4/Ending4CutsceneManager.Ending4CutsceneManager_C",
    "/Game/Cutscenes/RitualCutscene/RitualCutsceneManager.RitualCutsceneManager_C",
    "/Game/Cutscenes/SipehaTransition/SipehaTransitionCutsceneManager.SipehaTransitionCutsceneManager_C"
};
static_assert(std::size(world_paths)==world_object_count);
static std::array<Weak,world_object_count+factory_metadata_count> world_inputs{};
static unsigned world_input_count(){
    return world_object_count+(host_role?static_cast<unsigned>(factory_object_count):factory_metadata_count);
}
static Weak attached_world{},world_engine_type{},world_valid_pawn_fn{},world_valid_local_fn{};
static int world_instance_at=-1,world_owner_at=-1;
static bool world_owner_checked,world_configured,world_rescan;
static unsigned world_cursor,world_cine_cursor;
static ULONGLONG world_next_prepare,world_missing_since,world_next_rescan;
static std::uint64_t world_rescan_unloaded;

static std::string_view world_input_path(unsigned i){
    if(i<world_object_count)return world_paths[i];
    i-=world_object_count;
    return i<factory_object_count?factory_paths[i]:factory_ownership_paths[i-factory_object_count];
}
static void* world_input(unsigned i){return api.weakget(&world_inputs[i]);}

static void scan_world_inputs(unsigned budget,bool include_cutscenes=false){
    const auto array=AnimU::field<AnimU::ObjectArray>(reinterpret_cast<void*>(native_object_array),0);
    if(!AnimU::object_array_shape(array))return;
    const auto total=static_cast<unsigned>(array.num_elements);
    auto& cursor=include_cutscenes?world_cine_cursor:world_cursor;
    if(cursor>=total)cursor=0;
    const auto end=std::min(cursor+budget,total);
    const auto count=include_cutscenes?static_cast<unsigned>(world_object_count):world_input_count();
    std::array<unsigned,world_inputs.size()> missing{};
    unsigned size=0;
    for(unsigned i=include_cutscenes?static_cast<unsigned>(world_cine_base):0u;i<count;++i){
        if(!include_cutscenes&&i>=world_cine_base&&i<world_object_count)continue;
        if(!world_input(i))missing[size++]=i;
    }
    if(!size)return;
    for(;cursor<end;++cursor){
        const auto chunk=reinterpret_cast<const std::uintptr_t*>(array.chunks)[cursor/AnimU::objects_per_chunk];
        if(!chunk)continue;
        const auto item=reinterpret_cast<const AnimU::ObjectItem*>(chunk)[cursor%AnimU::objects_per_chunk];
        if(!item.object||(item.flags&0x30000000u))continue;
        auto object=reinterpret_cast<void*>(item.object);
        std::string leaf;
        if(!AnimU::name_string(catalog_name_pool,AnimU::field<AnimU::NameKey>(object,24),catalog_read,leaf))
            continue;
        std::string path;
        for(unsigned index=0;index<size;){
            const auto i=missing[index];
            const auto expected=world_input_path(i);
            if(leaf==catalog_leaf(expected)&&(!path.empty()||catalog_path(object,path))&&
               path==expected&&makeweak(world_inputs[i],object)){
                missing[index]=missing[--size];
            }else ++index;
        }
        if(!size)break;
    }
    // Wrap against this slice's snapshot so new objects cannot keep the scan
    // chasing the tail and prevent revisiting an earlier, reused slot.
    if(cursor>=total)cursor=0;
}

struct LocalWorld { void* pawn{}; void* world{}; void* instance{}; };
static void bind_native_life(void* pawn,void* instance,double time);
static void suspend_native_life();

static void* native_actor_world(void* actor){
    const auto level_class=world_input(world_level),world_class=world_input(world_type);
    const auto level=AnimU::field<void*>(actor,32);
    if(!level||!level_class||!world_class||!api.isa(level,level_class))return nullptr;
    if(!world_owner_checked){
        const auto owner=catalog_field(level_class,"OwningWorld",8);
        if(owner)world_owner_at=*api.offset(owner);
        world_owner_checked=true;
    }
    const auto world=AnimU::field<void*>(level,world_owner_at>=0?world_owner_at:32);
    return world&&api.isa(world,world_class)?world:nullptr;
}

static LocalWorld current_local_world(void* engine){
    world_lookup_stage=1; // Canonical inputs.
    const auto instance_class=world_input(world_game_instance),player_class=world_input(world_player_type);
    const auto gameplay=world_input(static_cast<unsigned>(world_object_count)+factory_gameplay);
    const auto pawn_fn=world_input(world_player_pawn);
    const auto local_fn=world_input(world_pawn_local);
    if(!engine||!instance_class||!player_class||!gameplay||!pawn_fn||!local_fn)
        return {};
    world_lookup_stage=2; // Engine-owned game instance.
    const auto engine_type=AnimU::field<void*>(engine,16);
    if(api.weakget(&world_engine_type)!=engine_type){
        const auto property=catalog_field(engine_type,"GameInstance",8);
        if(!property||!makeweak(world_engine_type,engine_type))return {};
        world_instance_at=*api.offset(property);
    }
    if(world_instance_at<0)return {};
    const auto instance=AnimU::field<void*>(engine,world_instance_at);
    if(!instance||!api.isa(instance,instance_class))return {};
    world_lookup_stage=3; // Reflected parameter layouts.
    if(api.weakget(&world_valid_pawn_fn)!=pawn_fn){
        if(*api.parms(pawn_fn)!=24||factory_parameter(pawn_fn,"WorldContextObject",8)!=0||
           factory_parameter(pawn_fn,"PlayerIndex",4)!=8||factory_parameter(pawn_fn,"ReturnValue",8)!=16||
           !makeweak(world_valid_pawn_fn,pawn_fn))return {};
    }
    if(api.weakget(&world_valid_local_fn)!=local_fn){
        if(*api.parms(local_fn)!=1||factory_parameter(local_fn,"ReturnValue",1)!=0||
           !makeweak(world_valid_local_fn,local_fn))return {};
    }
    alignas(16) std::array<unsigned char,24> parameters{};
    std::memcpy(parameters.data(),&instance,sizeof(instance));
    world_lookup_stage=4; // Possessed local player.
    api.event(gameplay,pawn_fn,parameters.data());
    const auto pawn=AnimU::field<void*>(parameters.data(),16);
    if(!pawn||!api.isa(pawn,player_class)||actor_is_destroying(pawn))return {};
    unsigned char local{};
    api.event(pawn,local_fn,&local);
    if(!local)return {};
    world_lookup_stage=5; // Level and world identity.
    const auto world=native_actor_world(pawn);
    if(!world)return {};
    world_lookup_stage=6;
    return {pawn,world,instance};
}

static std::uint64_t native_world_epoch(void* world){
    std::string path;
    if(!catalog_path(world,path)||!path.starts_with("/Game/"))return 0;
    std::uint64_t hash=14695981039346656037ull;
    for(unsigned char byte:path)hash=(hash^byte)*1099511628211ull;
    return hash?hash:1;
}
static NativeWorld::Phase checkpoint_world_transition(const LocalWorld& local,ULONGLONG time);
static NativeWorld::Phase persistent_world_transition(const LocalWorld& local,ULONGLONG time);
static void prepare_persistent_world_scope(void* world);
static void publish_persistent_world(const LocalWorld& local,ULONGLONG time);

static bool configure_world_inputs(){
    world_configure_stage=1;
    world_missing_inputs=0;
    const auto count=world_input_count();
    static_assert(world_player_asset_first+factory_metadata_count<=64);
    for(unsigned i=0;i<count;++i){
        if(i>=world_player_asset_first&&i<world_object_count)continue;
        const auto bit=i<world_player_asset_first?i:i-world_object_count+world_player_asset_first;
        if(!world_input(i))world_missing_inputs|=1ull<<bit;
    }
    if(world_missing_inputs)return false;
    world_configure_stage=2;
    if(!QDEnemyConfigureNativeEnumeration(world_input(world_enumerate)))return false;
    world_configure_stage=3;
    std::array<std::uint64_t,factory_metadata_count> args{};
    const auto factories=count-world_object_count;
    for(unsigned i=0;i<factories;++i)args[i]=reinterpret_cast<std::uintptr_t>(world_input(world_object_count+i));
    if(!configure_native_factory(args.data(),factories))return false;
    world_configure_stage=4;
    if(!configure_native_replica_destruction(world_input(world_destroy)))return false;
    world_configure_stage=5;
    const std::array<std::uint64_t,6> death{
        reinterpret_cast<std::uintptr_t>(world_input(world_kill)),
        reinterpret_cast<std::uintptr_t>(world_input(world_actor_tick)),
        reinterpret_cast<std::uintptr_t>(world_input(world_component_tick)),
        reinterpret_cast<std::uintptr_t>(catalog_field(base_class,"CustomTimeDilation",4))
    };
    if(!configure_death(death,true))return false;
    world_configure_stage=6;
    return true;
}

extern "C" __declspec(dllexport) NativeWorld::Phase QDEnemyPumpNativeWorld(void* engine,float dt,unsigned limit){
    using Phase=NativeWorld::Phase;
    if(!native_animation_ready()||!engine||!limit||limit>E::capacity||
       api.weakget(&metadata_weak[0])!=base_class||
       api.weakget(&metadata_weak[10])!=getters[3].fn)return Phase::failed;
    if(!worker||mode!=1)return Phase::idle;
    if(!native_transport_ready()){
        if(lan_transport&&build_state.load()==0)QDEnemyPollNativeRecovery();
        return Phase::idle;
    }
    const auto time=GetTickCount64();
    if(!automatic_limit&&time>=world_next_prepare){
        world_next_prepare=time+16;
        scan_world_inputs(2048);
        world_configured=world_configured||configure_world_inputs();
        if(world_configured&&!catalog_ready)scan_native_animation(2048);
    }
    const auto local=current_local_world(engine);
    if(const auto transition=checkpoint_world_transition(local,time);transition!=Phase::idle)return transition;
    if(const auto transition=persistent_world_transition(local,time);transition!=Phase::idle)return transition;
    if(!automatic_limit){
        if(!local.pawn)return Phase::waiting_player;
        if(!world_configured||!catalog_ready)return Phase::preparing;
        const auto epoch=native_world_epoch(local.world);
        if(!epoch||!makeweak(attached_world,local.world)||!QDEnemyEnableNativeWorld(local.pawn,limit))
            return Phase::failed;
        envelope.world=qdcoop::persistent_travel::epoch(epoch,checkpoint_world_nonce);
        prepare_persistent_world_scope(local.world);
    }else if(!local.pawn){
        suspend_native_life();
        if(!api.weakget(&attached_world))return Phase::world_changed;
        if(!world_missing_since)world_missing_since=time;
        return time-world_missing_since<2000?Phase::suspended:Phase::world_changed;
    }else if(api.weakget(&attached_world)!=local.world||
             (api.weakget(&automatic_context)!=local.pawn&&!makeweak(automatic_context,local.pawn))){
        suspend_native_life();
        return Phase::world_changed;
    }
    world_missing_since=0;
    if(!host_role&&unconfirmed_offers&&time>=world_next_rescan&&
       (world_rescan||world_rescan_unloaded!=factory_unloaded)){
        world_next_rescan=time+50;
        if(!world_rescan)world_rescan_unloaded=factory_unloaded;
        world_rescan=scan_native_animation(512)==0;
    }
    bind_native_life(local.pawn,local.instance,now());
    QDEnemyEngineTick(dt);
    if(metadata_ready)publish_persistent_world(local,time);
    return metadata_ready?Phase::active:Phase::failed;
}
