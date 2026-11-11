static bool verified_game(){
    const unsigned char expected[32]={0x79,0xcc,0xdc,0x6c,0xf9,0xe1,0xe8,0x55,0x12,0xd5,0x2d,0x2c,0xb1,0xc3,0xff,0xa9,0x63,0xd4,0x00,0xbe,0x15,0x55,0x05,0x1a,0x35,0x23,0xc9,0x82,0xd1,0x81,0xd1,0xe6};
    wchar_t name[32768]{};if(!GetModuleFileNameW(nullptr,name,32768))return false;
    HANDLE file=CreateFileW(name,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    BCRYPT_ALG_HANDLE alg{};BCRYPT_HASH_HANDLE hash{};bool ok=BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)==0;
    if(ok)ok=BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0)==0;
    std::array<unsigned char,65536> buffer{};DWORD bytes{};
    while(ok){if(!ReadFile(file,buffer.data(),static_cast<DWORD>(buffer.size()),&bytes,nullptr)){ok=false;break;}if(!bytes)break;ok=BCryptHashData(hash,buffer.data(),bytes,0)==0;}
    unsigned char result[32]{};if(ok)ok=BCryptFinishHash(hash,result,32,0)==0&&std::memcmp(result,expected,32)==0;
    if(hash)BCryptDestroyHash(hash);
    if(alg)BCryptCloseAlgorithmProvider(alg,0);
    CloseHandle(file);return ok;
}
