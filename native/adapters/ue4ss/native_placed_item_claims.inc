struct PlacedClaims {
    struct Grant {Placed::Claim claim;double expires{};};
    struct Alias {std::string id;qdcoop::Id identity;std::uint32_t generation{};double expires{};};
    Placed::Claim request,reply;
    std::vector<Grant> grants;
    std::array<Alias,Player::slots-1> aliases;
    Placed::Transfer operation;
    double request_until{},next_request{},next_reply{},next_credit{},next_grant{},next_alias{};
};
static PlacedClaims placed_claims;

static void placed_claims_reset(){placed_claims={};}

static const Placed::Transfer* placed_find_transfer(std::string_view id){
    const auto found=std::ranges::find_if(placed_item_transfers,[&](const auto& transfer){
        return WP::name_equal(transfer.drop.id,id);
    });
    return found==placed_item_transfers.end()?nullptr:&*found;
}

static std::string placed_claim_identity(qdcoop::Id identity){
    if(!identity.valid())return {};
    const auto text=Medic::rescue_suffix(identity);
    return std::string(text.begin(),text.end());
}

static bool placed_claim_pending(const Placed::Transfer& transfer,std::string_view winner,std::uint64_t ticket){
    return transfer.state==Placed::Custody::pending&&ticket&&ticket==transfer.ticket&&
        !winner.empty()&&WP::name_equal(winner,transfer.winner);
}

static AnimU::NameKey placed_claim_winner(const Placed::Transfer& transfer){
    return placed_pickup_name(std::string(Placed::winner_prefix)+transfer.drop.id+"~"+transfer.winner);
}

static AnimU::NameKey placed_claim_ticket(const Placed::Transfer& transfer){
    return placed_pickup_name(std::string(Placed::ticket_prefix)+transfer.drop.id+"~"+std::to_string(transfer.ticket));
}

static bool placed_claim_send(const Placed::Claim& claim,unsigned mask){
    E::Packet packet{};
    if(!Placed::encode_claim(envelope,claim,packet)||!send(packet,mask))return false;
    ++placed_claim_sent;
    return true;
}

static bool placed_claim_peer(unsigned lane,double time){
    if(lane>=Player::slots-1||!(active_peer_mask&(1u<<lane)))return false;
    const auto& peer=life_peers[lane+1];
    const auto id=peer.rescue_identity;
    return id.valid()&&time>=peer.received&&time-peer.received<=.8&&id!=medic_identity()&&
        medic_peer(id,time)==static_cast<int>(lane+1);
}

static std::string_view placed_claim_peer_alias(unsigned lane,double time){
    const auto& aliases=placed_claims.aliases;
    const auto& alias=aliases[lane];
    const auto& peer=life_peers[lane+1];
    if(alias.id.empty()||time>=alias.expires||alias.identity!=peer.rescue_identity||alias.generation!=peer.generation||
       WP::name_equal(alias.id,placed_legacy_identity))return {};
    for(unsigned other=0;other<aliases.size();++other){
        const auto& duplicate=aliases[other];
        if(other!=lane&&time<duplicate.expires&&WP::name_equal(alias.id,duplicate.id)&&placed_claim_peer(other,time))return {};
    }
    return alias.id;
}

static void placed_claim_grants(double time){
    auto& state=placed_claims;
    if(time<state.next_grant||std::ranges::none_of(placed_item_transfers,[](const auto& transfer){
        return transfer.state==Placed::Custody::pending&&transfer.ticket;
    }))return;
    state.next_grant=time+.2;
    for(unsigned lane=0;lane<Player::slots-1;++lane){
        if(!placed_claim_peer(lane,time))continue;
        const auto& peer=life_peers[lane+1];
        const auto identity=placed_claim_identity(peer.rescue_identity);
        for(const auto& transfer:placed_item_transfers)
            if(placed_claim_pending(transfer,identity,transfer.ticket))
                placed_claim_send({transfer.drop.id,transfer.ticket,peer.generation,Placed::ClaimOp::grant},1u<<lane);
    }
}

static bool placed_claim_record_grant(Placed::Claim claim,double time){
    if(claim.generation!=local_life_pose.generation)return false;
    auto& grants=placed_claims.grants;
    std::erase_if(grants,[&](const auto& grant){return grant.expires<=time;});
    const auto found=std::ranges::find(grants,claim,&PlacedClaims::Grant::claim);
    if(found!=grants.end())found->expires=time+2.;
    else if(grants.size()<Placed::max_ground_items)grants.push_back({std::move(claim),time+2.});
    else return false;
    return true;
}

static bool placed_claim_queue(std::string_view id,double time){
    auto& state=placed_claims;
    if(!local_life_pose.generation||local_life.life.phase!=LifePhase::standing||!medic_identity().valid()||
       state.reply.ticket)return false;
    if(state.request.ticket&&time<state.request_until)return WP::name_equal(state.request.id,id);
    std::uint64_t ticket{};
    if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&ticket),sizeof(ticket),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0||!ticket)
        return false;
    state.request={std::string(id),ticket,local_life_pose.generation,Placed::ClaimOp::request};
    state.request_until=time+4.;
    state.next_request=time;
    return true;
}

static bool placed_claim_begin(void* pawn,const Placed::Transfer& transfer,PlacedOperation operation,
                               const Placed::Transfer* previous,double time){
    const auto& binding=medic_runtime.save;
    const auto save=medic_active(binding,pawn);
    if(!save||!world_progress_ready()||native_checkpoint.stage!=CheckpointStage::idle||rollback_busy()||
       world_scene_busy()||native_placed_item_save_pending())return false;
    const bool reserve=operation==PlacedOperation::reserve,reopen=operation==PlacedOperation::reopen;
    const auto key=placed_pickup_name(transfer.save_key),winner=placed_claim_winner(transfer),ticket=placed_claim_ticket(transfer);
    if(!key.index||!winner.index||!ticket.index)return false;
    const auto old=reserve?Placed::Custody::ground:Placed::Custody::pending;
    int before{};
    if(!binding.find(binding.markers.address(save),binding.markers.field,&key,&before)||before!=static_cast<int>(old)||
       (!reserve&&(medic_marker(binding,save,winner)!=1||medic_marker(binding,save,ticket)!=1)))return false;
    const auto next=reserve?Placed::Custody::pending:reopen?Placed::Custody::ground:Placed::Custody::claimed;
    const std::array markers{PlacedSaveMarker{key,static_cast<int>(next)},PlacedSaveMarker{winner,!reopen},PlacedSaveMarker{ticket,!reopen}};
    AnimU::NameKey old_winner{},old_ticket{};
    if(previous){
        if(!previous->winner.empty()&&!WP::name_equal(previous->winner,transfer.winner)){
            old_winner=placed_claim_winner(*previous);
            if(!old_winner.index)return false;
        }
        if(previous->ticket&&previous->ticket!=transfer.ticket){
            old_ticket=placed_claim_ticket(*previous);
            if(!old_ticket.index)return false;
        }
    }
    PlacedSaveProof proof;
    if(!placed_save_prepare(pawn,markers,{},-1,proof))return false;
    if(old_winner.index)medic_set_marker(binding,save,old_winner,0);
    if(old_ticket.index)medic_set_marker(binding,save,old_ticket,0);
    for(const auto& marker:markers)medic_set_marker(binding,save,marker.key,marker.value);
    placed_claims.operation=transfer;
    placed_operation=operation;
    placed_save_begin(std::move(proof),time);
    return true;
}

static bool placed_claim_reserve(const Placed::Claim& claim,const LifePeer& peer,std::string_view identity,
                                 std::string_view legacy,double time){
    const auto record=placed_find_transfer(claim.id);
    if(record&&placed_claim_pending(*record,identity,claim.ticket))return true;
    if(placed_operation==PlacedOperation::reserve&&WP::name_equal(placed_claims.operation.drop.id,claim.id)&&
       WP::name_equal(placed_claims.operation.winner,identity)&&placed_claims.operation.ticket==claim.ticket)return true;
    if(!record||record->state!=Placed::Custody::ground||!fresh_life(peer,LifePhase::standing,time)||
       native_placed_item_save_pending())return false;
    const auto pawn=api.weakget(&automatic_context);
    const auto visual=std::ranges::find(placed_item_visuals,record->save_key,&PlacedItemVisual::save_key);
    const auto actor=visual==placed_item_visuals.end()||!visual->wanted||!visual->ready?nullptr:api.weakget(&visual->actor);
    const auto item=placed_item_data(record->drop.item);
    const auto& binding=medic_runtime.save;
    const auto save=medic_active(binding,pawn);
    if(!save||!actor||!item||(!placed_stackable(item)&&!Placed::owner_matches(record->drop.owner,identity,legacy))||
       !placed_claim_visible(pawn,actor,peer.position,record->drop))return false;
    const auto receipt=placed_pickup_name("QDCoopPersonal~got~"+record->drop.id);
    int local_receipt{};
    if(!receipt.index||binding.find(binding.markers.address(save),binding.markers.field,&receipt,&local_receipt))return false;
    auto award=*record;
    award.winner=identity;
    award.ticket=claim.ticket;
    award.state=Placed::Custody::pending;
    return placed_claim_begin(pawn,award,PlacedOperation::reserve,record,time);
}

static bool receive_placed_claim(const ReceivedPacket& received,unsigned kind,double time){
    if(kind!=Placed::claim_kind)return false;
    Placed::Claim claim;
    if(!active_peer_mask||!Placed::decode_claim(envelope,received.packet,claim)){
        ++placed_claim_rejected;
        return true;
    }
    ++placed_claim_received;
    if(!host_role){
        const auto& request=placed_claims.request;
        if(!received.peer&&claim.op==Placed::ClaimOp::grant&&placed_claim_record_grant(std::move(claim),time))return true;
        if(!received.peer&&claim.op==Placed::ClaimOp::refused&&request.ticket==claim.ticket&&
           request.generation==claim.generation&&WP::name_equal(request.id,claim.id))placed_claims.request={};
        else ++placed_claim_rejected;
        return true;
    }
    if(claim.op==Placed::ClaimOp::grant||!placed_claim_peer(received.peer,time)){
        ++placed_claim_rejected;
        return true;
    }
    const auto& peer=life_peers[received.peer+1];
    const auto identity=placed_claim_identity(peer.rescue_identity);
    if(peer.generation!=claim.generation){
        ++placed_claim_rejected;
        return true;
    }
    if(claim.op==Placed::ClaimOp::alias){
        placed_claims.aliases[received.peer]={std::move(claim.id),peer.rescue_identity,peer.generation,time+2.};
        return true;
    }
    if(claim.op==Placed::ClaimOp::request){
        if(!placed_claim_reserve(claim,peer,identity,placed_claim_peer_alias(received.peer,time),time)){
            claim.op=Placed::ClaimOp::refused;
            placed_claim_send(claim,1u<<received.peer);
            ++placed_claim_rejected;
        }
        return true;
    }
    const auto transfer=placed_find_transfer(claim.id);
    if(transfer&&placed_claim_pending(*transfer,identity,claim.ticket)&&!native_placed_item_save_pending()){
        const auto operation=claim.op==Placed::ClaimOp::received?PlacedOperation::settle:PlacedOperation::reopen;
        placed_claim_begin(api.weakget(&automatic_context),*transfer,operation,nullptr,time);
    }
    return true;
}

static void placed_claim_finished(double time){
    auto& state=placed_claims;
    if(placed_operation==PlacedOperation::reserve)++placed_claim_reserved;
    if(placed_operation==PlacedOperation::settle)++placed_claim_settled;
    if(placed_operation==PlacedOperation::credit||placed_operation==PlacedOperation::decline){
        const bool credited=placed_operation==PlacedOperation::credit;
        state.reply={state.operation.drop.id,state.operation.ticket,local_life_pose.generation,
            credited?Placed::ClaimOp::received:Placed::ClaimOp::refused};
        state.next_reply=time;
        if(credited)++placed_pickup_confirmed;
    }
    state.operation={};
}

static bool placed_claim_import(void* pawn,Placed::Transfer& record,double time){
    const auto& binding=medic_runtime.save;
    const auto save=medic_active(binding,pawn);
    if(!save)return false;
    const auto ground=placed_pickup_name(record.save_key),old_winner=placed_claim_winner(record);
    int current{};
    if(!ground.index||!old_winner.index||!binding.find(binding.markers.address(save),binding.markers.field,&ground,&current)||
       current!=1||medic_marker(binding,save,old_winner)!=1)return false;
    const auto item=placed_item_data(record.drop.item);
    if(!item)return false;
    const bool stackable=placed_stackable(item);
    if(Placed::owner_matches(record.winner,placed_claim_identity(medic_identity()),placed_legacy_identity)){
        if(!stackable&&!placed_local_owner(record.drop))return false;
        PlacedSaveProof proof;
        if(placed_credit(pawn,record.drop,false,proof)){
            placed_pickup_pending=record.save_key;
            placed_pickup_drop_key=ground;
            placed_operation=PlacedOperation::local_pickup;
        }else{
            const auto receipt=placed_pickup_name("QDCoopPersonal~got~"+record.drop.id);
            int previous{};
            if(stackable||medic_count(binding,item)<1||!receipt.index||
               binding.find(binding.markers.address(save),binding.markers.field,&receipt,&previous))return false;
            const std::array markers{PlacedSaveMarker{ground,0},PlacedSaveMarker{old_winner,0}};
            if(!placed_save_prepare(pawn,markers,{},-1,proof))return false;
            for(const auto& marker:markers)medic_set_marker(binding,save,marker.key,marker.value);
            placed_operation=PlacedOperation::reopen;
        }
        placed_save_begin(std::move(proof),time);
        return true;
    }
    for(unsigned lane=0;lane<Player::slots-1;++lane){
        if(!placed_claim_peer(lane,time))continue;
        const auto identity=placed_claim_identity(life_peers[lane+1].rescue_identity);
        const auto alias=placed_claim_peer_alias(lane,time);
        if(!Placed::owner_matches(record.winner,identity,alias)||
           (!stackable&&!Placed::owner_matches(record.drop.owner,identity,alias)))continue;
        auto award=record;
        if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&award.ticket),sizeof(award.ticket),
                           BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0||!award.ticket)return false;
        award.winner=identity;
        const auto winner=placed_claim_winner(award),ticket=placed_claim_ticket(award);
        if(!winner.index||!ticket.index)return false;
        const std::array markers{PlacedSaveMarker{ground,1},PlacedSaveMarker{winner,1},PlacedSaveMarker{ticket,1},
            PlacedSaveMarker{old_winner,0}};
        const auto required=std::span(markers).first(WP::name_equal(record.winner,identity)?3:4);
        PlacedSaveProof proof;
        if(!placed_save_prepare(pawn,required,{},-1,proof))return false;
        for(const auto& marker:required)medic_set_marker(binding,save,marker.key,marker.value);
        record=award;
        placed_claims.operation=std::move(award);
        placed_operation=PlacedOperation::reserve;
        placed_save_begin(std::move(proof),time);
        return true;
    }
    return false;
}

static void placed_claim_legacy_step(void* pawn,double time){
    auto& state=placed_claims;
    if(time<state.next_credit||native_checkpoint.stage!=CheckpointStage::idle||rollback_busy()||world_scene_busy())return;
    for(auto& transfer:placed_item_transfers){
        if(transfer.state!=Placed::Custody::pending||transfer.ticket||transfer.winner.empty())continue;
        state.next_credit=time+.45;
        if(placed_claim_import(pawn,transfer,time))return;
    }
}

static void placed_claims_step(void* pawn,double time){
    if(!active_peer_mask||!world_progress_ready()||!local_life_pose.generation)return;
    if(host_role){
        placed_claim_legacy_step(pawn,time);
        if(!native_placed_item_save_pending())placed_claim_grants(time);
        return;
    }
    auto& state=placed_claims;
    if(!placed_legacy_identity.empty()&&time>=state.next_alias){
        state.next_alias=time+.65;
        placed_claim_send({placed_legacy_identity,1,local_life_pose.generation,Placed::ClaimOp::alias},1);
    }
    if(state.request.ticket){
        const auto record=placed_find_transfer(state.request.id);
        if(!record||record->state!=Placed::Custody::ground||time>=state.request_until)state.request={};
        else if(time>=state.next_request){
            state.next_request=time+.2;
            state.request.generation=local_life_pose.generation;
            placed_claim_send(state.request,1);
        }
    }
    if(!state.reply.ticket&&std::ranges::none_of(placed_item_transfers,[](const auto& transfer){
        return transfer.state==Placed::Custody::pending&&transfer.ticket;
    }))return;
    const auto identity=placed_claim_identity(medic_identity());
    if(identity.empty())return;
    if(state.reply.ticket){
        const auto record=placed_find_transfer(state.reply.id);
        if(!record||!placed_claim_pending(*record,identity,state.reply.ticket))state.reply={};
        else if(time>=state.next_reply){
            state.next_reply=time+.2;
            state.reply.generation=local_life_pose.generation;
            placed_claim_send(state.reply,1);
        }
        if(state.reply.ticket)return;
    }
    if(time<state.next_credit||native_checkpoint.stage!=CheckpointStage::idle||rollback_busy()||world_scene_busy())return;
    const auto award=std::ranges::find_if(placed_item_transfers,[&](const auto& transfer){
        return placed_claim_pending(transfer,identity,transfer.ticket)&&std::ranges::any_of(state.grants,[&](const auto& grant){
            return grant.expires>time&&grant.claim.ticket==transfer.ticket&&grant.claim.generation==local_life_pose.generation&&
                WP::name_equal(grant.claim.id,transfer.drop.id);
        });
    });
    if(award==placed_item_transfers.end())return;
    state.next_credit=time+.35;
    const auto& binding=medic_runtime.save;
    const auto save=medic_active(binding,pawn);
    const auto item=placed_item_data(award->drop.item);
    const auto receipt=placed_pickup_name("QDCoopPersonal~got~"+award->drop.id);
    const auto decline=placed_pickup_name("QDCoopPersonal~no~"+award->drop.id+"~"+std::to_string(award->ticket));
    int previous{};
    if(!save||!item||!receipt.index||!decline.index)return;
    PlacedSaveProof proof;
    const bool has_receipt=binding.find(binding.markers.address(save),binding.markers.field,&receipt,&previous);
    const bool declined=medic_marker(binding,save,decline)==1;
    const int quantity=medic_count(binding,item);
    if(declined||(!has_receipt&&(quantity==INT_MAX||(!placed_stackable(item)&&quantity>0)))){
        if(has_receipt)return;
        const std::array markers{PlacedSaveMarker{decline,1}};
        if(!placed_save_prepare(pawn,markers,{},-1,proof))return;
        medic_set_marker(binding,save,decline,1);
        placed_operation=PlacedOperation::decline;
    }else{
        if(!placed_credit(pawn,award->drop,false,proof))return;
        placed_operation=PlacedOperation::credit;
    }
    state.operation=*award;
    placed_save_begin(std::move(proof),time);
}
