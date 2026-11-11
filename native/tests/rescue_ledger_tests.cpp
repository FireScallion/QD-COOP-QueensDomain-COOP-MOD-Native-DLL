#include "qdcoop/rescue_store_windows.hpp"
#include "qdcoop/rescue_wire.hpp"
#include <cstdio>

namespace R=qdcoop::rescue;
static unsigned failed{};
#define CHECK(expr) do { if(!(expr)){std::printf("FAIL %u: %s\n",__LINE__,#expr);++failed;} } while(false)

static bool advance(R::Store& store,R::Ledger value){
    ++value.generation;
    return store.commit(value);
}
static constexpr qdcoop::Id target{2,17};
static R::Attempt first_attempt(const R::Ledger& value){
    return {value.identity,target,1,95,1};
}
static int crash_after_intent(const wchar_t* slot){
    R::Store store;
    R::SaveProof primary;
    if(!store.open(slot,primary))return 2;
    auto value=store.latest();
    const auto attempt=first_attempt(value);
    if(!value.offer(attempt)||!advance(store,value))return 3;
    value=store.latest();
    if(!value.intend(attempt,11)||!advance(store,value))return 4;
    TerminateProcess(GetCurrentProcess(),57);
    return 5;
}
static bool run_crashing_donor(const std::wstring& path){
    wchar_t exe[2048]{};
    const auto size=GetModuleFileNameW(nullptr,exe,static_cast<DWORD>(std::size(exe)));
    if(!size||size>=std::size(exe))return false;
    auto command=L"\""+std::wstring(exe)+L"\" --crash-intent \""+path+L"\"";
    STARTUPINFOW info{};
    info.cb=sizeof(info);
    PROCESS_INFORMATION process{};
    if(!CreateProcessW(exe,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&info,&process))
        return false;
    R::FileHandle child(process.hProcess),thread(process.hThread);
    DWORD exit{};
    return WaitForSingleObject(child.value,10000)==WAIT_OBJECT_0&&GetExitCodeProcess(child.value,&exit)&&exit==57;
}
int wmain(int argc,wchar_t** argv){
    if(argc==3&&std::wstring_view(argv[1])==L"--crash-intent")return crash_after_intent(argv[2]);
    wchar_t temp[MAX_PATH]{};
    qdcoop::Id random{};
    if(!GetTempPathW(MAX_PATH,temp)||!R::rescue_random(random))return 2;
    const auto directory=std::wstring(temp)+L"qd-rescue-test-"+R::rescue_suffix(random);
    if(!CreateDirectoryW(directory.c_str(),nullptr))return 2;
    const auto slot=directory+L"\\World1.sav";
    {
        R::FileHandle file(CreateFileW(slot.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));
        const char data[]{"native slot anchor"};
        DWORD count{};
        CHECK(file&&WriteFile(file.value,data,sizeof(data),&count,nullptr)&&count==sizeof(data));
    }
    CHECK(run_crashing_donor(slot));
    R::SaveProof anchor{};
    {
        R::Store store,duplicate;
        CHECK(store.open(slot,anchor)&&anchor.valid());
        R::SaveProof unused{};
        CHECK(!duplicate.open(slot,unused));
        auto donor=store.latest();
        const auto a=first_attempt(donor);
        CHECK(donor.outgoing.phase==R::Debit::intended&&donor.outgoing.attempt==a);
        CHECK((donor.inventory[0]==R::Inventory{1,10,1}));
        CHECK(!donor.intend(a,10)&&!donor.cancel(a));
        CHECK(!donor.settle(a,R::Receipt::rejected,10));
        CHECK(donor.debit_saved(a,anchor)&&advance(store,donor));
        donor=store.latest();
        CHECK(donor.settle(a,R::Receipt::rejected,10)&&advance(store,donor));
        donor=store.latest();
        CHECK((donor.inventory[0]==R::Inventory{2,11,1}));
        CHECK(!donor.settle(a,R::Receipt::rejected,11));
        CHECK(donor.refund_saved(a)&&advance(store,donor));
        donor=store.latest();
        CHECK(donor.outgoing==R::Outgoing{});
        R::Ledger receiver{};
        receiver.identity=target;
        receiver.anchor=anchor;
        receiver.generation=1;
        CHECK(receiver.prepare(a)&&!receiver.prepare(a));
        auto competing=a;competing.donor={4,19};
        CHECK(!receiver.prepare(competing));
        CHECK(receiver.receive(a,R::Receipt::applying));
        CHECK(!receiver.receive(a,R::Receipt::applied));
        CHECK(receiver.receive(a,R::Receipt::applied,anchor));
        CHECK(!receiver.receive(a,R::Receipt::rejected));
        CHECK(receiver.acknowledge(a)&&!receiver.prepare(a));
        CHECK(receiver.prepare(competing));
        R::Blob blob{};
        R::Ledger copy{};
        CHECK(R::encode_ledger(receiver,blob)&&R::decode_ledger(blob,copy)&&copy==receiver);
        blob[81]^=1;
        CHECK(!R::decode_ledger(blob,copy));
        qdcoop::enemy::Packet packet{};
        const qdcoop::enemy::Envelope room{4,7,93,1};
        R::Message decoded{};
        CHECK(R::encode_message(room,{a,R::Op::debit},packet));
        CHECK(R::decode_message(room,packet,decoded)&&decoded.attempt==a&&decoded.op==R::Op::debit);
        auto wrong=room;++wrong.world;
        CHECK(!R::decode_message(wrong,packet,decoded));
        auto authenticated=a;
        authenticated.donor_account=0x0110000100000011;
        CHECK(!R::encode_message(room,{authenticated,R::Op::debit},packet));
        authenticated.target_account=0x0110000100000012;
        CHECK(R::encode_message(room,{authenticated,R::Op::debit},packet)&&
              R::decode_message(room,packet,decoded)&&decoded.attempt==authenticated);
    }
    {
        R::Store reopened;
        R::SaveProof primary{};
        R::FileHandle stock_writer(CreateFileW(slot.c_str(),GENERIC_WRITE,
            FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
        CHECK(stock_writer);
        CHECK(reopened.open(slot,primary)&&primary==anchor);
        CHECK((reopened.latest().inventory[0]==R::Inventory{2,11,1}));
    }
    DeleteFileW((slot+L".qdrescue-native-v1").c_str());
    DeleteFileW(slot.c_str());
    RemoveDirectoryW(directory.c_str());
    if(failed)return 1;
    std::puts("PASS rescue ledger: process death after durable intent, exclusive ownership, single debit/refund, receipt replay and unchanged primary");
    return 0;
}
