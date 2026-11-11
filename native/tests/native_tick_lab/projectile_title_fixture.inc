namespace ProjectileFixture {
enum Symbol {dagger,fire,exit_cutscene,position,scale,actor_collision,component_collision,destroying,destroy,
             fx_active,fx_parent,begin_spawn,finish_spawn,static_component,component_position,dagger_hit,
             niagara_type,mesh_actor_type,particle_type,emitter_location,count};
constexpr std::array<const char*,count> paths{
    "/Game/Items/Weapons/Daggers/Default/DefaultDagger_Projectile_BP.DefaultDagger_Projectile_BP_C",
    "/Game/Player/FS_PlayerChar.FS_PlayerChar_C.FireDagger",
    "/Game/Blueprints/FS_GameInstance.FS_GameInstance_C.ExitCutsceneMode",
    "/Script/Engine.Actor.K2_GetActorLocation","/Script/Engine.SceneComponent.K2_GetComponentScale","/Script/Engine.Actor.GetActorEnableCollision",
    "/Script/Engine.PrimitiveComponent.GetCollisionEnabled","/Script/Engine.Actor.IsActorBeingDestroyed",
    "/Script/Engine.Actor.K2_DestroyActor",
    "/Script/Engine.ActorComponent.IsActive","/Script/Engine.SceneComponent.GetAttachParent",
    "/Script/Engine.GameplayStatics.BeginDeferredActorSpawnFromClass","/Script/Engine.GameplayStatics.FinishSpawningActor",
    "/Script/Engine.StaticMeshComponent","/Script/Engine.SceneComponent.K2_GetComponentLocation",
    "/Game/Items/Weapons/Daggers/Dagger_Projectile_Base.Dagger_Projectile_Base_C.ImpactFX",
    "/Script/Niagara.NiagaraComponent","/Script/Engine.StaticMeshActor",
    "/Script/Engine.ParticleSystemComponent","/Script/Engine.GameplayStatics.SpawnEmitterAtLocation"
};
constexpr std::string_view mesh_path="/Game/Items/Weapons/Daggers/Default/Default_Dagger_Low.Default_Dagger_Low";
constexpr std::string_view trail_path="/Game/Items/Weapons/Daggers/Default/Dagger_Projectile_Trail.Dagger_Projectile_Trail";
constexpr std::string_view gun_class="/Game/Items/Weapons/Default_Gun_Projectile.Default_Gun_Projectile_C";
constexpr std::string_view gun_mesh="/Game/Items/Weapons/Slashgun/SlashGunProjectile.SlashGunProjectile";
constexpr std::string_view gun_trail="/Game/Items/Weapons/Slashgun/SlashGun_Trail.SlashGun_Trail";
constexpr std::string_view enemy_class="/Game/Enemies/Boner_00/Skeletons24/Black/Boner_Gun_Projectile.Boner_Gun_Projectile_C";
constexpr std::string_view enemy_trail="/Game/Enemies/Boner_00/Skeletons24/Black/Boner_Gun_ProjectileTrail.Boner_Gun_ProjectileTrail";
constexpr std::string_view impact_asset="/Game/Enemies/Boner_00/Skeletons24/Black/Boner_SlashGun_ImpactFX.Boner_SlashGun_ImpactFX";
constexpr std::string_view dagger_impact="/Game/Items/Weapons/Daggers/Shared/Default_Dagger_ImpactVFX.Default_Dagger_ImpactVFX";
constexpr std::string_view cascade_impact="/Game/LD45/FX/BloodSplosion.BloodSplosion";
std::array<UE::WeakKey,count> objects{};
UE::WeakKey actor{},shooter{},trail{},burst{};
std::wstring directory;
ULONGLONG ready{},last_scan{},last_report{},next_asset_load{};
float traveled{};
bool fired{},no_collision{},scale_matches{},moved{},retired{},requested_destroy{},reported{},trail_active{},trail_gone{},gun{},enemy{};
bool impact{},impact_active{},impact_gone{},impact_position{},impact_triggered{},cascade{};
std::array<float,3> first_position{};
unsigned (*read_core)(char*,unsigned){};
constexpr std::array<const wchar_t*,4> relay_roles{L"host",L"client",L"client2",L"client3"};

Address get(Symbol id){return ui.get(objects[id]);}
std::wstring path(std::wstring_view name){return directory+L"/"+std::wstring(name);}
std::wstring_view source_name(){return enemy?L"host":L"client";}
bool source_role(){return lan.role==source_name();}
bool rendered_receiver(){return lan.role==(enemy?L"client":L"host");}
std::string_view stock_class(){return enemy?enemy_class:gun?gun_class:paths[dagger];}
bool all_joined(){
    for(const auto role:relay_roles)if(!marker_exists(path(std::wstring(role)+L"/ready.txt").c_str()))return false;
    return true;
}
bool all_receivers(std::wstring_view suffix){
    for(const auto role:relay_roles)
        if(std::wstring_view(role)!=source_name()&&!marker_exists(path(L"projectile-"+std::wstring(role)+std::wstring(suffix)).c_str()))return false;
    return true;
}
bool gone(Address value){
    if(!value)return true;
    if(!ui.method(get(destroying),1,{{"ReturnValue","BoolProperty",0,1}}))return false;
    unsigned char result{};
    TitleUi::event(value,get(destroying),&result);
    return result!=0;
}
bool active_fx(Address effect){
    if(!effect||!ui.method(get(fx_active),1,{{"ReturnValue","BoolProperty",0,1}}))return false;
    unsigned char active{};
    TitleUi::event(effect,get(fx_active),&active);
    return active!=0;
}
Address static_mesh(Address object){
    const auto component=ui.object(object,"StaticMeshComponent");
    const auto mesh=ui.object(component,"StaticMesh");
    return mesh&&path_is(mesh,gun||enemy?gun_mesh:mesh_path)?component:0;
}
void scan() {
    const auto array=at<UE::ObjectArray>(image+TitleUi::object_array,0);
    if(!UE::object_array_shape(array))return;
    const auto watched=ui.get(actor);
    const auto niagara=get(niagara_type),mesh_actor=get(mesh_actor_type),particle=get(particle_type);
    bool binding{};
    for(unsigned symbol=0;symbol<objects.size();++symbol){
        if(symbol==dagger&&(!source_role()||fired))continue;
        if(symbol==dagger_hit&&(!source_role()||!impact||enemy))continue;
        if((symbol==particle_type||symbol==emitter_location)&&!cascade)continue;
        if(!ui.get(objects[symbol]))binding=true;
    }
    for(unsigned i=static_cast<unsigned>(array.num_elements);i;){
        --i;
        const auto chunk=at<Address>(array.chunks,(i/UE::objects_per_chunk)*8);
        if(!chunk)continue;
        const auto item=at<UE::ObjectItem>(chunk,(i%UE::objects_per_chunk)*sizeof(UE::ObjectItem));
        if(!item.object||(item.flags&0x30000000u))continue;
        const auto type=at<Address>(item.object,16);
        if(!watched&&!(at<unsigned>(item.object,8)&0x10u)){
            if(source_role()&&fired&&!requested_destroy&&get(dagger)&&type==get(dagger)&&
               (ui.object(item.object,"Owner")==ui.get(shooter)||ui.object(item.object,"Instigator")==ui.get(shooter)))
                ui.remember(actor,item.object);
            if(!source_role()&&type==mesh_actor&&static_mesh(item.object))ui.remember(actor,item.object);
        }
        if((type==niagara||(cascade&&type==particle))&&!(at<unsigned>(item.object,8)&0x10u)&&
           ui.method(get(fx_parent),8,{{"ReturnValue","ObjectProperty",0,8}})){
            const auto asset=ui.object(item.object,type==particle?"Template":"Asset");
            if(type==niagara&&!source_role()&&asset&&watched&&path_is(asset,enemy?enemy_trail:gun?gun_trail:trail_path)){
                Address parent{};
                TitleUi::event(item.object,get(fx_parent),&parent);
                if(parent==static_mesh(watched))ui.remember(trail,item.object);
            }else if(asset&&impact&&!impact_position&&(source_role()||rendered_receiver())&&
                     path_is(asset,cascade?cascade_impact:enemy?impact_asset:dagger_impact)){
                Address parent{};
                TitleUi::event(item.object,get(fx_parent),&parent);
                if(source_role()?!parent:(parent&&ui.is(parent,get(static_component))&&!ui.object(parent,"StaticMesh")))
                    ui.remember(burst,item.object);
            }
        }
        if(!binding)continue;
        const auto name=leaf(item.object);
        for(unsigned j=0;j<paths.size();++j){
            if(ui.get(objects[j]))continue;
            const std::string_view expected=j==dagger?stock_class():paths[j];
            if(name==expected.substr(expected.rfind('.')+1)&&path_is(item.object,expected))ui.remember(objects[j],item.object);
        }
    }
}
bool no_collisions(Address value) {
    const auto component=static_mesh(value);
    if(!component||!ui.method(get(actor_collision),1,{{"ReturnValue","BoolProperty",0,1}})||
       !ui.method(get(component_collision),1,{{"ReturnValue","ByteProperty",0,1}}))return false;
    unsigned char actor_mode=1,component_mode=255;
    TitleUi::event(value,get(actor_collision),&actor_mode);
    TitleUi::event(component,get(component_collision),&component_mode);
    return actor_mode==0&&component_mode==0;
}
bool core_proof(const char* a,const char* b){
    if(!read_core){
        const auto core=GetModuleHandleW(L"QDCoopNativeM1.dll");
        if(!core||!load_export(core,"QDEnemyNativeStatus",read_core))return false;
    }
    const auto size=read_core(nullptr,0);
    if(!size||size>16384)return false;
    std::string status(size,'\0');
    if(read_core(status.data(),size)!=size)return false;
    if(auto file=_wfopen(path(L"projectile-core-"+lan.role+L".json").c_str(),L"wb")){
        std::fputs(status.c_str(),file);
        std::fclose(file);
    }
    return status.find(a)!=status.npos&&status.find(b)!=status.npos;
}
void report(){
    if(auto file=_wfopen(path(L"projectile-"+lan.role+L".json").c_str(),L"wb")){
        std::fprintf(file,"{\"passed\":1,\"host\":%u,\"source\":%u,\"gun\":%u,\"enemy\":%u,\"fired\":%u,\"moved\":%u,\"collision_disabled\":%u,"
            "\"scale_matches\":%u,\"retired\":%u,\"trail_active\":%u,\"trail_gone\":%u,\"impact\":%u,"
            "\"impact_active\":%u,\"impact_gone\":%u,\"impact_position\":%u,\"cascade\":%u,\"distance\":%.3f}\n",
            lan.host,source_role(),gun,enemy,fired,moved,no_collision,scale_matches,retired,trail_active,trail_gone,
            impact,impact_active,impact_gone,impact_position,cascade,static_cast<double>(traveled));
        std::fclose(file);
    }
    reported=true;
}
bool record_impact_origin(Address effect){
    if(!ui.method(get(component_position),12,{{"ReturnValue","StructProperty",0,12}}))return false;
    std::array<float,3> origin{};
    TitleUi::event(effect,get(component_position),origin.data());
    auto file=_wfopen(path(L"impact-origin.bin").c_str(),L"wb");
    if(!file)return false;
    const bool written=std::fwrite(origin.data(),sizeof(float),origin.size(),file)==origin.size();
    std::fclose(file);
    return written;
}
bool at_impact_origin(Address effect){
    if(!ui.method(get(component_position),12,{{"ReturnValue","StructProperty",0,12}}))return false;
    std::array<float,3> origin{},actual{};
    auto file=_wfopen(path(L"impact-origin.bin").c_str(),L"rb");
    if(!file)return false;
    const bool read=std::fread(origin.data(),sizeof(float),origin.size(),file)==origin.size();
    std::fclose(file);
    if(!read)return false;
    TitleUi::event(effect,get(component_position),actual.data());
    for(unsigned axis=0;axis<3;++axis)if(std::abs(actual[axis]-origin[axis])>1.f)return false;
    return true;
}
void spawn_cascade(Address projectile,Address system,const std::array<float,3>& contact){
    alignas(16) std::array<unsigned char,64> args{};
    std::memcpy(args.data(),&projectile,8);
    std::memcpy(args.data()+8,&system,8);
    std::memcpy(args.data()+16,contact.data(),12);
    constexpr std::array scale{1.f,1.f,1.f};
    std::memcpy(args.data()+40,scale.data(),12);
    args[52]=args[54]=1;
    TitleUi::event(ui.get(TitleUi::gameplay),get(emitter_location),args.data());
    const auto effect=UE::field<Address>(args.data(),56);
    if(!ui.is(effect,get(particle_type))||ui.object(effect,"Template")!=system||!active_fx(effect))return;
    ui.remember(burst,effect);
    impact_active=true;
    impact_position=record_impact_origin(effect);
}
bool fire_projectile(Address pawn){
    if(!gun&&!enemy&&!impact){
        if(!ui.method(get(fire),9,{{"Class","ClassProperty",0,8},{"Success","BoolProperty",8,1}}))return false;
        struct Args {Address type;bool success;} args{get(dagger),false};
        TitleUi::event(pawn,get(fire),&args);
        return args.success;
    }
    const auto begin=get(begin_spawn),finish=get(finish_spawn);
    Address particle{};
    if(cascade){
        if(!ui.method(get(emitter_location),64,{{"WorldContextObject","ObjectProperty",0,8},{"EmitterTemplate","ObjectProperty",8,8},
           {"Location","StructProperty",16,12},{"Rotation","StructProperty",28,12},{"Scale","StructProperty",40,12},
           {"bAutoDestroy","BoolProperty",52,1},{"PoolingMethod","EnumProperty",53,1},{"bAutoActivateSystem","BoolProperty",54,1},
           {"ReturnValue","ObjectProperty",56,8}}))return false;
        particle=ui.load_asset_path(cascade_impact);
        if(!particle)return false;
    }
    if(impact&&!enemy&&!ui.method(get(dagger_hit),24,{{"Pos","StructProperty",0,12},{"Normal","StructProperty",12,12}}))
        return false;
    if(!ui.method(begin,88,{{"WorldContextObject","ObjectProperty",0,8},{"ActorClass","ClassProperty",8,8},
                           {"SpawnTransform","StructProperty",16,48},{"CollisionHandlingOverride","EnumProperty",64,1},
                           {"Owner","ObjectProperty",72,8},{"ReturnValue","ObjectProperty",80,8}})||
       !ui.method(finish,72,{{"Actor","ObjectProperty",0,8},{"SpawnTransform","StructProperty",16,48},
                            {"ReturnValue","ObjectProperty",64,8}})||
       !ui.method(get(position),12,{{"ReturnValue","StructProperty",0,12}}))return false;
    std::array<float,3> origin{};
    TitleUi::event(pawn,get(position),origin.data());
    const std::array<float,12> transform{0,0,0,1,origin[0],origin[1],origin[2]+260.f,0,1,1,1,0};
    alignas(16) std::array<unsigned char,96> args{},done{};
    const auto type=get(dagger),gameplay=ui.get(TitleUi::gameplay);
    std::memcpy(args.data(),&pawn,8);
    std::memcpy(args.data()+8,&type,8);
    std::memcpy(args.data()+16,transform.data(),48);
    args[64]=1;
    if(!enemy)std::memcpy(args.data()+72,&pawn,8);
    TitleUi::event(gameplay,begin,args.data());
    const auto projectile=UE::field<Address>(args.data(),80);
    if(!projectile)return false;
    std::memcpy(done.data(),&projectile,8);
    std::memcpy(done.data()+16,transform.data(),48);
    TitleUi::event(gameplay,finish,done.data());
    if(UE::field<Address>(done.data(),64)!=projectile)return false;
    ui.remember(actor,projectile);
    if(cascade){
        spawn_cascade(projectile,particle,{origin[0]+180.f,origin[1],origin[2]+40.f});
    }else if(impact&&!enemy){
        std::array<float,6> contact{origin[0]+180.f,origin[1],origin[2]+40.f,0.f,0.f,1.f};
        TitleUi::event(projectile,get(dagger_hit),contact.data());
        impact_triggered=true;
    }
    return ui.get(actor)==projectile;
}
bool step(Address instance,ULONGLONG now) {
    if(reported)return !lan.host||marker_exists(path(L"projectile-source-complete.txt").c_str());
    if(directory.empty()){
        wchar_t root[32768]{};
        if(!environment_title(L"QD_PROJECTILE_TITLE_ROOT",root,32768))return false;
        directory=root;
        wchar_t variant[12]{};
        if(environment_title(L"QD_PROJECTILE_VARIANT",variant,12)){
            const bool player_impact=std::wcscmp(variant,L"dagger-hit")==0;
            const bool enemy_impact=std::wcscmp(variant,L"impact")==0;
            gun=std::wcscmp(variant,L"gun")==0;
            cascade=std::wcscmp(variant,L"cascade")==0;
            impact=player_impact||enemy_impact||cascade;
            enemy=enemy_impact||cascade||std::wcscmp(variant,L"enemy")==0;
        }
    }
    if(!ready)ready=now;
    if(now-last_report>=1000){
        last_report=now;
        core_proof("","");
    }
    const bool finding_impact=impact&&!impact_position&&
        (source_role()?(enemy?fired:impact_triggered):rendered_receiver()&&(!enemy||cascade||moved));
    if(now-last_scan>=(finding_impact?100u:350u)&&
       (!ui.get(actor)||!get(position)||(source_role()&&impact&&!enemy&&!get(dagger_hit))||
        (rendered_receiver()&&!moved&&!ui.get(trail))||finding_impact)){
        last_scan=now;
        scan();
    }
    if(finding_impact&&active_fx(ui.get(burst))){
        impact_active=true;
        impact_position=source_role()?record_impact_origin(ui.get(burst)):at_impact_origin(ui.get(burst));
    }
    if(source_role()){
        if(!fired){
            if(now-ready<1800||!all_joined())return false;
            if((gun||enemy||impact)&&!get(dagger)&&now>=next_asset_load){
                next_asset_load=now+1500;
                if(const auto type=ui.load_asset_path(stock_class()))ui.remember(objects[dagger],type);
            }
            if(!get(dagger))return false;
            if(const auto cutscene=ui.member(instance,"InCutscene","BoolProperty",1);
               cutscene&&UE::get_bool(reinterpret_cast<void*>(cutscene),reinterpret_cast<const void*>(instance),0)){
                if(ui.method(get(exit_cutscene),0,{}))TitleUi::event(instance,get(exit_cutscene),nullptr);
                return false;
            }
            Address pawn[]{instance,0,0};
            TitleUi::event(ui.get(TitleUi::gameplay),ui.get(TitleUi::local_pawn),pawn);
            if(!pawn[2])return false;
            if(!enemy)ui.remember(shooter,pawn[2]);
            fired=fire_projectile(pawn[2]);
            return false;
        }
        if((!impact||((!enemy||cascade)&&impact_position))&&!requested_destroy&&all_receivers(L"-moved.txt")){
            const auto source=ui.get(actor);
            if(!gone(source)){
                if(!ui.method(get(destroy),0,{}))return false;
                TitleUi::event(source,get(destroy),nullptr);
            }
            requested_destroy=true;
        }
        const bool projectile_ended=((enemy&&impact&&!cascade)||requested_destroy)&&gone(ui.get(actor));
        const bool source_ended=projectile_ended&&(!impact||(impact_position&&!active_fx(ui.get(burst))));
        if(source_ended&&all_receivers(L"-gone.txt")&&
           core_proof(impact?"\"projectile_observed\":2,":"\"projectile_observed\":1,",
                      impact?"\"projectile_retired\":2,":"\"projectile_retired\":1,")){
            retired=true;
            impact_gone=impact;
            report();
            marker(path(L"projectile-source-complete.txt").c_str());
        }
    }else if(!moved){
        const auto visual=ui.get(actor);
        const auto component=static_mesh(visual);
        const auto effect=ui.get(trail);
        if(!component||!ui.method(get(position),12,{{"ReturnValue","StructProperty",0,12}})||
           !ui.method(get(scale),12,{{"ReturnValue","StructProperty",0,12}})||!no_collisions(visual))return false;
        if(rendered_receiver()){
            trail_active=active_fx(effect);
            if(!trail_active)return false;
        }
        no_collision=true;
        std::array<float,3> actual_scale{};
        TitleUi::event(component,get(scale),actual_scale.data());
        const std::array<float,3> expected=enemy?std::array{.521728f,.600343f,.25f}:
            gun?std::array{.613495f,1.740524f,.5f}:std::array{-1.f,1.f,1.f};
        scale_matches=std::abs(actual_scale[0]-expected[0])<.01f&&std::abs(actual_scale[1]-expected[1])<.01f&&
            std::abs(actual_scale[2]-expected[2])<.01f;
        if(!scale_matches)return false;
        std::array<float,3> current{};
        TitleUi::event(visual,get(position),current.data());
        if(first_position==std::array<float,3>{})first_position=current;
        float distance{};
        for(unsigned axis=0;axis<3;++axis){
            const auto delta=current[axis]-first_position[axis];
            distance+=delta*delta;
        }
        traveled=std::sqrt(distance);
        if(traveled>50.f){
            moved=true;
            marker(path(L"projectile-"+lan.role+L"-moved.txt").c_str());
        }
    }else if(gone(ui.get(actor))&&!ui.get(trail)&&(!impact||!rendered_receiver()||(impact_position&&!ui.get(burst)))&&
             core_proof(impact?"\"projectile_retired\":2,":"\"projectile_retired\":1,",
                        impact?"\"projectile_spawned\":2,":"\"projectile_spawned\":1,")&&
             core_proof(impact?"\"projectile_spawned\":2,":"\"projectile_spawned\":1,",
                        impact?"\"projectile_removed\":2,":"\"projectile_removed\":1,")&&
             (!rendered_receiver()||core_proof(impact?"\"projectile_trail_spawned\":2,":"\"projectile_trail_spawned\":1,",
                                              impact?"\"projectile_trail_removed\":2,":"\"projectile_trail_removed\":1,"))){
        trail_gone=true;
        impact_gone=impact&&rendered_receiver();
        retired=true;
        marker(path(L"projectile-"+lan.role+L"-gone.txt").c_str());
        report();
    }
    return reported&&(!lan.host||marker_exists(path(L"projectile-source-complete.txt").c_str()));
}
}
