namespace HealthbarFixture {
constexpr std::string_view boss_path="/Game/UI/HUD/Widgets/EnemyHealthBar/BossHealthBar.BossHealthBar_C";
constexpr std::string_view enemy_path="/Game/UI/HUD/Widgets/EnemyHealthBar/EnemyHealthBar_base.EnemyHealthBar_base_C";
unsigned stage{};
ULONGLONG next{};
float stock_hp{},test_hp{};

Address method(Address widget,std::string_view owner,std::string_view name){
    const auto path=std::string(owner)+"."+std::string(name);
    for(auto type=at<Address>(widget,16);type;type=at<Address>(type,64)){
        if(!path_is(type,owner))continue;
        unsigned count{};
        for(auto child=at<Address>(type,72);child&&count<1024;child=at<Address>(child,40),++count)
            if(at<Address>(child,32)==type&&leaf(child)==name&&path_is(child,path))return child;
        break;
    }
    return 0;
}

struct Targets {Address* data{};int count{},capacity{};};

bool report(Address boss,Address enemy,Address actor,const Targets& targets){
    const auto boss_hp=ui.member(boss,"CurrentHP","FloatProperty",4);
    const auto enemy_hp=ui.member(enemy,"CurrentHP","FloatProperty",4);
    if(auto out=_wfopen(MountFixture::file(lan.role+L"-healthbar.txt").c_str(),L"wb")){
        std::fprintf(out,"stage=%u actor=%u targets=%d group=%u stock=%.5f test=%.5f boss=%.5f enemy=%.5f bound=%u\n",
            stage,actor!=0,targets.count,ui.object(boss,"CurrentGroup")!=0,stock_hp,test_hp,
            boss_hp?at<float>(boss,at<unsigned>(boss_hp,76)):-1.f,enemy_hp?at<float>(enemy,at<unsigned>(enemy_hp,76)):-1.f,
            ui.object(enemy,"Enemy")==actor);
        std::fclose(out);
    }
    return boss_hp&&enemy_hp;
}

bool step(Address instance,ULONGLONG now){
    if(stage==3)return MountFixture::all(L"healthbar-clean");
    if(now<next)return false;
    next=now+100;
    Address pawn[]{instance,0,0};
    TitleUi::event(ui.get(TitleUi::gameplay),ui.get(TitleUi::local_pawn),pawn);
    const auto hud=ui.object(pawn[2],"HUD");
    const auto zone=ui.object(hud,"EnemyStatusZone");
    const auto boss=ui.object(zone,"BossHealthBar"),enemy=ui.object(zone,"EnemyHealthBar_base");
    MountFixture::E::NativeBindingView binding{};
    if(!boss||!enemy||!MountFixture::inspect_binding||!MountFixture::source_id||
       !MountFixture::inspect_binding(MountFixture::source_id,&binding))return false;
    const auto actor=ui.get(binding.actor),hp_field=ui.member(actor,"HP","FloatProperty",4);
    const auto targets_field=ui.member(boss,"Targets","ArrayProperty",16);
    if(!hp_field||!targets_field)return false;
    const auto hp_at=at<unsigned>(hp_field,76);
    const auto targets=at<Targets>(boss,at<unsigned>(targets_field,76));
    if(!report(boss,enemy,actor,targets))return false;
    const auto host_spec=MountFixture::file(L"host-healthbar-values.txt");
    if(lan.host){
        if(!stage){
            if(targets.count)return false;
            const auto add=method(boss,boss_path,"AddTarget");
            if(!ui.method(add,16,{{"Enemy","ObjectProperty",0,8},{"Group","ObjectProperty",8,8}}))return false;
            stock_hp=at<float>(actor,hp_at);
            if(!std::isfinite(stock_hp)||stock_hp<=.01f)return false;
            test_hp=.625f*stock_hp;
            Address args[]{actor,0};
            TitleUi::event(boss,add,args);
            stage=1;
            if(auto out=_wfopen(host_spec.c_str(),L"wb")){
                std::fprintf(out,"%.9g %.9g\n",stock_hp,test_hp);
                std::fclose(out);
            }
            MountFixture::mark(L"healthbar-ready");
        }else if(stage==1&&MountFixture::all(L"healthbar-ready")){
            std::memcpy(reinterpret_cast<void*>(actor+hp_at),&test_hp,sizeof(test_hp));
            stage=2;
            MountFixture::mark(L"healthbar-hit");
        }else if(stage==2&&MountFixture::seen(L"client-healthbar-hit.txt")&&
                 MountFixture::seen(L"client2-healthbar-hit.txt")&&MountFixture::seen(L"client3-healthbar-hit.txt")){
            const auto remove=method(boss,boss_path,"Deactivate");
            if(!ui.method(remove,16,{{"Group","ObjectProperty",0,8},{"Enemy","ObjectProperty",8,8}}))return false;
            Address args[]{ui.object(boss,"CurrentGroup"),actor};
            TitleUi::event(boss,remove,args);
            std::memcpy(reinterpret_cast<void*>(actor+hp_at),&stock_hp,sizeof(stock_hp));
            stage=3;
            MountFixture::mark(L"healthbar-clean");
        }
    }else if(!stage&&MountFixture::seen(L"host-healthbar-ready.txt")){
        if(auto in=_wfopen(host_spec.c_str(),L"rb")){
            const auto read=std::fscanf(in,"%f %f",&stock_hp,&test_hp);
            std::fclose(in);
            if(read!=2)return false;
        }else return false;
        const auto set=method(enemy,enemy_path,"SetTarget");
        if(!ui.method(set,12,{{"Enemy","ObjectProperty",0,8},{"Damage","FloatProperty",8,4}}))return false;
        struct {Address enemy;float damage;} args{actor,0.f};
        TitleUi::event(enemy,set,&args);
        if(ui.object(enemy,"Enemy")!=actor)return false;
        stage=1;
        MountFixture::mark(L"healthbar-ready");
    }else if(stage==1&&MountFixture::seen(L"host-healthbar-hit.txt")){
        const auto boss_hp=ui.member(boss,"CurrentHP","FloatProperty",4),enemy_hp=ui.member(enemy,"CurrentHP","FloatProperty",4);
        if(targets.count==1&&targets.data&&targets.data[0]==actor&&boss_hp&&enemy_hp&&
           std::abs(at<float>(boss,at<unsigned>(boss_hp,76))-test_hp)<.001f&&
           std::abs(at<float>(enemy,at<unsigned>(enemy_hp,76))-test_hp)<.001f){
            stage=2;
            if(auto out=_wfopen(MountFixture::file(lan.role+L"-healthbar-hit.txt").c_str(),L"wb")){
                std::fprintf(out,"stock=%.5f expected=%.5f boss=%.5f enemy=%.5f targets=%d\n",stock_hp,test_hp,
                    at<float>(boss,at<unsigned>(boss_hp,76)),at<float>(enemy,at<unsigned>(enemy_hp,76)),targets.count);
                std::fclose(out);
            }
        }
    }else if(stage==2&&MountFixture::seen(L"host-healthbar-clean.txt")&&!targets.count){
        stage=3;
        MountFixture::mark(L"healthbar-clean");
    }
    MountFixture::report();
    return stage==3&&MountFixture::all(L"healthbar-clean");
}
}
