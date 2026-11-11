namespace PeerRangeFixture {
using MountFixture::E::NativeBindingView;
UE::WeakKey spawned;
std::array<float,3> origin{},point{};
unsigned id{},stage{};
ULONGLONG next{},spawned_at{};

Address pawn_at(Address instance){
    Address args[]{instance,0,0};
    TitleUi::event(ui.get(TitleUi::gameplay),ui.get(TitleUi::local_pawn),args);
    return args[2];
}

std::array<float,3> position(Address actor){
    std::array<float,3> value{};
    TitleUi::event(actor,MountFixture::fn(MountFixture::location),value.data());
    return value;
}

bool move(Address actor,const std::array<float,3>& where){
    const auto fn=HealthbarFixture::method(actor,"/Script/Engine.Actor","K2_SetActorLocationAndRotation");
    if(!ui.method(fn,166,{{"NewLocation","StructProperty",0,12},{"NewRotation","StructProperty",12,12},
          {"bSweep","BoolProperty",24,1},{"SweepHitResult","StructProperty",28,136},
          {"bTeleport","BoolProperty",164,1},{"ReturnValue","BoolProperty",165,1}}))return false;
    alignas(16) std::array<unsigned char,176> args{};
    std::memcpy(args.data(),where.data(),12);
    args[164]=1;
    TitleUi::event(actor,fn,args.data());
    return args[165]!=0;
}

Address spawn(Address pawn,Address type){
    const auto gameplay=ui.get(TitleUi::gameplay);
    const auto begin=HealthbarFixture::method(gameplay,"/Script/Engine.GameplayStatics","BeginDeferredActorSpawnFromClass");
    const auto finish=HealthbarFixture::method(gameplay,"/Script/Engine.GameplayStatics","FinishSpawningActor");
    if(!type||!ui.method(begin,88,{{"WorldContextObject","ObjectProperty",0,8},{"ActorClass","ClassProperty",8,8},
            {"SpawnTransform","StructProperty",16,48},{"CollisionHandlingOverride","EnumProperty",64,1},
            {"Owner","ObjectProperty",72,8},{"ReturnValue","ObjectProperty",80,8}})||
       !ui.method(finish,72,{{"Actor","ObjectProperty",0,8},{"SpawnTransform","StructProperty",16,48},
            {"ReturnValue","ObjectProperty",64,8}}))return 0;
    const std::array<float,12> transform{0,0,0,1,point[0],point[1],point[2],0,1,1,1,0};
    alignas(16) std::array<unsigned char,96> args{},done{};
    std::memcpy(args.data(),&pawn,8);
    std::memcpy(args.data()+8,&type,8);
    std::memcpy(args.data()+16,transform.data(),48);
    args[64]=1;
    TitleUi::event(gameplay,begin,args.data());
    const auto actor=UE::field<Address>(args.data(),80);
    if(!actor)return 0;
    std::memcpy(done.data(),&actor,8);
    std::memcpy(done.data()+16,transform.data(),48);
    TitleUi::event(gameplay,finish,done.data());
    return UE::field<Address>(done.data(),64)==actor?actor:0;
}

unsigned binding_for(Address actor){
    if(!actor)return 0;
    for(unsigned candidate=1;candidate<=MountFixture::E::capacity;++candidate){
        NativeBindingView view{};
        if(MountFixture::inspect_binding(candidate,&view)&&ui.get(view.actor)==actor)return candidate;
    }
    return 0;
}

bool step(Address instance,ULONGLONG now){
    if(stage==3)return MountFixture::all(L"peer-range-clean");
    if(now<next)return false;
    next=now+100;
    const auto pawn=pawn_at(instance);
    if(!pawn||!MountFixture::fn(MountFixture::location))return false;
    const auto target=MountFixture::file(L"host-peer-range-target.txt");
    const auto enrolled=MountFixture::file(L"host-peer-range-id.txt");
    if(lan.host){
        if(!stage){
            const auto template_actor=ui.get(MountFixture::source_owner);
            if(!template_actor)return false;
            origin=position(pawn);
            point={origin[0]+9000.f,origin[1],origin[2]+400.f};
            const auto actor=spawn(pawn,at<Address>(template_actor,16));
            if(!actor)return false;
            ui.remember(spawned,actor);
            spawned_at=now;
            stage=1;
        }else if(stage==1&&now-spawned_at>=1250){
            const auto actor=ui.get(spawned);
            if(!actor||binding_for(actor)||std::abs(position(actor)[0]-position(pawn)[0])<8000.f)return false;
            if(auto out=_wfopen(target.c_str(),L"wb")){
                std::fprintf(out,"%.9g %.9g %.9g\n",point[0],point[1],point[2]);
                std::fclose(out);
                stage=2;
            }
        }else if(stage==2){
            const auto actor=ui.get(spawned);
            if(!actor)return false;
            if(!id){
                id=binding_for(actor);
                if(!id)return false;
                const auto distance=std::abs(position(actor)[0]-position(pawn)[0]);
                if(distance<8000.f)return false;
                if(auto out=_wfopen(enrolled.c_str(),L"wb")){
                    std::fprintf(out,"%u %.5f\n",id,distance);
                    std::fclose(out);
                }
                MountFixture::mark(L"peer-range-bound");
            }else if(MountFixture::all(L"peer-range-bound")){
                const auto destroy=HealthbarFixture::method(actor,"/Script/Engine.Actor","K2_DestroyActor");
                if(!ui.method(destroy,0,{}))return false;
                TitleUi::event(actor,destroy,nullptr);
                MountFixture::report();
                stage=3;
                MountFixture::mark(L"peer-range-clean");
            }
        }
    }else{
        if(!stage&&lan.role==L"client3"){
            if(auto input=_wfopen(target.c_str(),L"rb")){
                const auto count=std::fscanf(input,"%f %f %f",&point[0],&point[1],&point[2]);
                std::fclose(input);
                if(count!=3)return false;
            }else return false;
            origin=position(pawn);
            if(!move(pawn,point))return false;
            stage=1;
        }
        if(lan.role==L"client3"&&stage==1&&!move(pawn,point))return false;
        if(stage<2){
            if(auto input=_wfopen(enrolled.c_str(),L"rb")){
                const auto count=std::fscanf(input,"%u",&id);
                std::fclose(input);
                if(count!=1||!id||id>MountFixture::E::capacity)return false;
            }else return false;
            NativeBindingView view{};
            if(!MountFixture::inspect_binding(id,&view)||!view.confirmed||!ui.get(view.actor)||!ui.get(view.display))return false;
            stage=2;
            MountFixture::mark(L"peer-range-bound");
        }else if(MountFixture::seen(L"host-peer-range-clean.txt")){
            if(lan.role==L"client3"&&!move(pawn,origin))return false;
            stage=3;
            MountFixture::mark(L"peer-range-clean");
        }
    }
    return stage==3&&MountFixture::all(L"peer-range-clean");
}
}
