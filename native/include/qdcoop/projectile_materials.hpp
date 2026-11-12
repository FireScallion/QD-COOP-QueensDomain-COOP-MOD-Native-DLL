#pragma once
#include <array>
#include <string_view>

namespace qdcoop::projectile::assets {
// Slot 0 materials used by the stock projectile components in the Steam build.
inline constexpr std::array<std::string_view,18> mesh_materials{
    "/Game/Enemies/Boner_00/Skeletons24/Gray/Equipment/Boner_Shiny_ArcherKit_MI.Boner_Shiny_ArcherKit_MI",
    "/Game/Enemies/Crawler/Fireball.Fireball",
    "/Game/Enemies/Golem/Mossy/Golem_MossyRock_MI.Golem_MossyRock_MI",
    "/Game/Items/Weapons/Daggers/Explode/Dagger_Explode_MI.Dagger_Explode_MI",
    "/Game/Items/Weapons/Daggers/StrangeStoneBlock/ShopStoneBlock/ShopStoneBlock_MI.ShopStoneBlock_MI",
    "/Game/Items/Weapons/Daggers/TechDagger/Tech_Dagger_MI.Tech_Dagger_MI",
    "/Game/Items/Weapons/Daggers/VampireDagger/Vampire_Dagger_MA.Vampire_Dagger_MA",
    "/Game/Items/Weapons/ElecGun/ElecGun_Alt_ProjectileWaveMat.ElecGun_Alt_ProjectileWaveMat",
    "/Game/Items/Weapons/ElecGun/ElecGun_ProjectileWaveMat.ElecGun_ProjectileWaveMat",
    "/Game/Items/Weapons/EyePea_Syringe/EyePeaSyringe_MI.EyePeaSyringe_MI",
    "/Game/Items/Weapons/HeavyRifle/HeavyRifle_ProjectileMesh_MA.HeavyRifle_ProjectileMesh_MA",
    "/Game/Items/Weapons/Shotgun/Shotgun_AltFire_Projectile_Mat.Shotgun_AltFire_Projectile_Mat",
    "/Game/Items/Weapons/SipehaLongHammer/SipehaMagicBall_MA.SipehaMagicBall_MA",
    "/Game/Items/Weapons/SniperGun/SniperRifle_ForceWave_Mat.SniperRifle_ForceWave_Mat",
    "/Game/Items/Weapons/Torch/Torch_Fireball.Torch_Fireball",
    "/Game/Items/Weapons/WodasoGreatSword/WodasoGS_Wave_MA.WodasoGS_Wave_MA",
    "/Game/VFX/EmitterTemplates/HeavyRock_MI.HeavyRock_MI",
    "/Game/VFX/Meshes/ForceWave_Mat.ForceWave_Mat",
};
}
