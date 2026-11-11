using System.Runtime.InteropServices;

// Flat Steamworks ABI; the DLL is supplied by the user's Steam game, never bundled.
sealed class SteamApi : IDisposable {
    readonly IntPtr library;
    readonly Dictionary<string,Delegate> functions=[];
    public readonly IntPtr User,Utils,Friends,Matchmaking,Messages,Networking;
    public readonly int Pipe;
    bool initialized;
    [StructLayout(LayoutKind.Sequential)] public struct Callback {public int User,Id;public IntPtr Data;public int Size;}
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate int IntP(IntPtr p);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate int Int0();
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate IntPtr Ptr0();
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void Void0();
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void VoidP(IntPtr p);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void VoidI(int p);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate byte BoolP(IntPtr p);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate uint UIntP(IntPtr p);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate ulong ULongP(IntPtr p);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate byte Next(int pipe,out Callback callback);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate byte CallResult(int pipe,ulong call,IntPtr data,int size,int expected,out byte failed);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate ulong Create(IntPtr p,int type,int limit);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate ulong Join(IntPtr p,ulong lobby);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void Leave(IntPtr p,ulong lobby);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate int Count(IntPtr p,ulong lobby);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate ulong Member(IntPtr p,ulong lobby,int index);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate ulong Owner(IntPtr p,ulong lobby);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate byte SetData(IntPtr p,ulong lobby,[MarshalAs(UnmanagedType.LPUTF8Str)]string key,[MarshalAs(UnmanagedType.LPUTF8Str)]string value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate IntPtr GetData(IntPtr p,ulong lobby,[MarshalAs(UnmanagedType.LPUTF8Str)]string key);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate byte Joinable(IntPtr p,ulong lobby,byte enabled);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate byte Presence(IntPtr p,[MarshalAs(UnmanagedType.LPUTF8Str)]string key,[MarshalAs(UnmanagedType.LPUTF8Str)]string? value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate int Send(IntPtr p,IntPtr identity,IntPtr data,uint size,int flags,int channel);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate int Receive(IntPtr p,int channel,[Out]IntPtr[] messages,int maximum);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate byte Session(IntPtr p,IntPtr identity);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void IdentitySet(IntPtr p,ulong user);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate ulong IdentityGet(IntPtr p);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate int Availability(IntPtr p,IntPtr status);
    public T F<T>(string name) where T:Delegate {
        if(!functions.TryGetValue(name,out var value)){value=Marshal.GetDelegateForFunctionPointer<T>(NativeLibrary.GetExport(library,name));functions.Add(name,value);}return (T)value;
    }
    IntPtr Interface(string prefix,int max){for(int i=max;i>0;i--)if(NativeLibrary.TryGetExport(library,prefix+i.ToString("000"),out var f)){var p=Marshal.GetDelegateForFunctionPointer<Ptr0>(f)();if(p!=IntPtr.Zero)return p;}throw new InvalidOperationException("Steam interface unavailable: "+prefix);}
    public SteamApi(string dll){
        Environment.SetEnvironmentVariable("SteamAppId","1799840");Environment.SetEnvironmentVariable("SteamGameId","1799840");
        library=NativeLibrary.Load(Path.GetFullPath(dll));var error=Marshal.AllocHGlobal(1024);
        try{
            Marshal.WriteByte(error,0);int result=F<IntP>("SteamAPI_InitFlat")(error);
            if(result!=0)throw new InvalidOperationException(Marshal.PtrToStringUTF8(error)??"Steam initialization failed");initialized=true;
            User=Interface("SteamAPI_SteamUser_v",30);Utils=Interface("SteamAPI_SteamUtils_v",20);Friends=Interface("SteamAPI_SteamFriends_v",30);Matchmaking=Interface("SteamAPI_SteamMatchmaking_v",15);Messages=Interface("SteamAPI_SteamNetworkingMessages_SteamAPI_v",5);Networking=Interface("SteamAPI_SteamNetworkingUtils_SteamAPI_v",10);
            if(F<UIntP>("SteamAPI_ISteamUtils_GetAppID")(Utils)!=1799840||F<BoolP>("SteamAPI_ISteamUser_BLoggedOn")(User)==0)throw new InvalidOperationException("Steam is not online for Queen's Domain");
            if(F<BoolP>("SteamAPI_ISteamApps_BIsSubscribed")(Interface("SteamAPI_SteamApps_v",15))==0)throw new InvalidOperationException("This Steam account does not own Queen's Domain");
            Pipe=F<Int0>("SteamAPI_GetHSteamPipe")();F<Void0>("SteamAPI_ManualDispatch_Init")();
        }catch{if(initialized)F<Void0>("SteamAPI_Shutdown")();NativeLibrary.Free(library);throw;}finally{Marshal.FreeHGlobal(error);}
    }
    public ulong Self=>F<ULongP>("SteamAPI_ISteamUser_GetSteamID")(User);
    public string LobbyData(ulong lobby,string key)=>Marshal.PtrToStringUTF8(F<GetData>("SteamAPI_ISteamMatchmaking_GetLobbyData")(Matchmaking,lobby,key))??"";
    public void SetLobbyData(ulong lobby,string key,string value){if(F<SetData>("SteamAPI_ISteamMatchmaking_SetLobbyData")(Matchmaking,lobby,key,value)==0)throw new IOException("Could not publish room metadata");}
    public ulong LobbyOwner(ulong lobby)=>F<Owner>("SteamAPI_ISteamMatchmaking_GetLobbyOwner")(Matchmaking,lobby);
    public HashSet<ulong> Members(ulong lobby){var r=new HashSet<ulong>();int count=F<Count>("SteamAPI_ISteamMatchmaking_GetNumLobbyMembers")(Matchmaking,lobby);for(int i=0;i<Math.Min(count,2);i++)r.Add(F<Member>("SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex")(Matchmaking,lobby,i));return r;}
    public bool SetPresence(string key,string? value)=>F<Presence>("SteamAPI_ISteamFriends_SetRichPresence")(Friends,key,value)!=0;
    public IntPtr Identity(ulong peer){var p=Marshal.AllocHGlobal(136);Marshal.Copy(new byte[136],0,p,136);F<IdentitySet>("SteamAPI_SteamNetworkingIdentity_SetSteamID64")(p,peer);return p;}
    public void Dispose(){if(initialized){F<Void0>("SteamAPI_Shutdown")();initialized=false;}NativeLibrary.Free(library);}
}
