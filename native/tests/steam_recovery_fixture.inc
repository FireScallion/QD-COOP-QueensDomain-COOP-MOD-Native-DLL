#include <deque>
#include <mutex>
#include <unordered_map>

namespace RecoveryFixture {
namespace Wire=E::enrollment;
constexpr SteamId lobby=108086391056891905ull;
constexpr SteamId host=76561197960265729ull,a=host+1,b=host+2,c=host+3,replacement=host+4;

struct Message {
    SteamMessagePrefix prefix{};
    E::Packet packet{};
};
static_assert(offsetof(Message,prefix)==0);

struct Fixture {
    std::mutex mutex;
    std::unordered_map<std::string,std::string> metadata;
    std::deque<Message*> pending;
    SteamId owner=host,self=host;
    std::array<SteamId,4> members{host,a,b,c};
    unsigned count=4;
    bool open{};
    unsigned channel_closes{},game_sent{};

    ~Fixture(){for(auto message:pending)delete message;}

    static Fixture& get(void* context){return *static_cast<Fixture*>(context);}
    static void identity(SteamIdentity* result,SteamId id){std::memcpy(result->bytes,&id,sizeof(id));}
    static SteamId identity_of(const SteamIdentity* value){
        SteamId id{};
        std::memcpy(&id,value->bytes,sizeof(id));
        return id;
    }

    void queue(SteamId sender,const E::Packet& packet){
        auto message=new Message{};
        message->packet=packet;
        message->prefix.data=message->packet.data.data();
        message->prefix.size=static_cast<int>(packet.size);
        identity(&message->prefix.identity,sender);
        pending.push_back(message);
    }
    void inject(SteamId sender,const E::Packet& packet){
        const std::lock_guard lock(mutex);
        queue(sender,packet);
    }
    void set_roster(std::array<SteamId,4> next,unsigned size,SteamId authority=host){
        const std::lock_guard lock(mutex);
        members=next;
        count=size;
        owner=authority;
    }
    void set_epoch(const SR::Epoch& epoch){
        const std::lock_guard lock(mutex);
        metadata["qd_epoch"]=SR::encode_epoch(epoch);
    }
    SR::Epoch read_epoch(){
        const std::lock_guard lock(mutex);
        SR::Epoch result{};
        SR::decode_epoch(metadata["qd_epoch"],owner,result);
        return result;
    }
    bool joinable(){
        const std::lock_guard lock(mutex);
        return open;
    }
    unsigned closed_channels(){
        const std::lock_guard lock(mutex);
        return channel_closes;
    }
    unsigned game_packets(){
        const std::lock_guard lock(mutex);
        return game_sent;
    }

    SteamApi api(){
        SteamApi api{};
        api.user=api.matchmaking=api.messages=this;
        api.logged_on=[](void*){return true;};
        api.release_thread=[](){};
        api.owner=[](void* context,SteamId){
            auto& fixture=get(context);
            const std::lock_guard lock(fixture.mutex);
            return fixture.owner;
        };
        api.count=[](void* context,SteamId){
            auto& fixture=get(context);
            const std::lock_guard lock(fixture.mutex);
            return static_cast<int>(fixture.count);
        };
        api.limit=[](void*,SteamId){return 4;};
        api.member=[](void* context,SteamId,int index){
            auto& fixture=get(context);
            const std::lock_guard lock(fixture.mutex);
            return index>=0&&static_cast<unsigned>(index)<fixture.count?
                fixture.members[static_cast<unsigned>(index)]:SteamId{};
        };
        api.set_data=[](void* context,SteamId,const char* key,const char* value){
            auto& fixture=get(context);
            const std::lock_guard lock(fixture.mutex);
            if(fixture.owner!=fixture.self)return false;
            fixture.metadata[key]=value;
            return true;
        };
        api.get_data=[](void* context,SteamId,const char* key){
            auto& fixture=get(context);
            const std::lock_guard lock(fixture.mutex);
            static thread_local std::string value;
            const auto found=fixture.metadata.find(key);
            value=found==fixture.metadata.end()?std::string{}:found->second;
            return value.c_str();
        };
        api.joinable=[](void* context,SteamId,bool open){
            auto& fixture=get(context);
            const std::lock_guard lock(fixture.mutex);
            if(fixture.owner!=fixture.self)return false;
            fixture.open=open;
            return true;
        };
        api.visibility=[](void*,SteamId,int){return true;};
        api.set_identity=&identity;
        api.get_identity=&identity_of;
        api.send_message=[](void* context,const SteamIdentity* identity,const void* bytes,unsigned size,int,int){
            auto& fixture=get(context);
            const std::lock_guard lock(fixture.mutex);
            E::Packet packet{};
            if(size>packet.data.size())return 0;
            packet.size=size;
            std::memcpy(packet.data.data(),bytes,size);
            const auto nonce=E::animation::wire::get<SteamId>(packet,16);
            SR::Control control{};
            if(SR::decode_control(packet,lobby,nonce,control)){
                if(control==SR::Control::hello){
                    const auto peer=identity_of(identity);
                    fixture.queue(peer,SR::control(lobby,nonce,SR::Control::hello));
                    fixture.queue(peer,SR::control(lobby,nonce,SR::Control::ack));
                }
            }else{
                ++fixture.game_sent;
                const auto world=E::animation::wire::get<std::uint64_t>(packet,24);
                const E::Envelope scope{lobby,nonce,world,1};
                Wire::Offer offer{};
                if(Wire::decode(scope,packet,false,offer)){
                    E::Packet ack{};
                    if(Wire::encode(scope,offer,true,ack))fixture.queue(identity_of(identity),ack);
                }
            }
            return 1;
        };
        api.receive=[](void* context,int,SteamMessagePrefix** batch,int capacity){
            auto& fixture=get(context);
            const std::lock_guard lock(fixture.mutex);
            int count=0;
            while(count<capacity&&!fixture.pending.empty()){
                batch[count++]=&fixture.pending.front()->prefix;
                fixture.pending.pop_front();
            }
            return count;
        };
        api.release_message=[](SteamMessagePrefix* message){delete reinterpret_cast<Message*>(message);};
        api.close_channel=[](void* context,const SteamIdentity*,int){
            auto& fixture=get(context);
            const std::lock_guard lock(fixture.mutex);
            ++fixture.channel_closes;
            return true;
        };
        return api;
    }
};

}
