# Steam transport — 1.18.0-preview1

## Ownership and lifetime

The game-side `qdsteam.dll` initializes the game's Valve-signed Steam API off the game thread when necessary, captures lobby/rich-presence join callbacks, and opens the Steam friends or lobby invitation overlay only on a UI action. Its callbacks only enqueue a lobby ID. The normal game-thread menu tick dispatches pending Steam callbacks at 4 Hz; it does not wait for networking, lobby creation or connection results. The shim never shuts down a Steam API that the original game may also use.

`Native-Session.ps1` starts a persistent `QDCoopBridge.exe --steam-service` alongside the existing per-room bridge. The service is tied to the game's PID and creation time, survives title-screen travel, and leaves its lobby when the game exits. Initialization failure leaves LAN available. Only the Steam API supplied by the same game installation and signed by Valve is loaded; the mod does not redistribute or replace it.

## Rooms and joins

After the host selects a save and enters the world, the broker creates a two-member lobby, publishes mod/build/transport/owner metadata, changes it to FriendsOnly, and advertises a `connect` rich-presence string. Steam or LAN share the existing single guest slot. A full room is no longer advertised as joinable. Room ownership is never migrated to a guest: the world's existing host authority is preserved.

Incoming joins are captured from `GameLobbyJoinRequested_t`, `GameRichPresenceJoinRequested_t`, or `+connect_lobby` / `+qdcoop_lobby` startup arguments. The native save picker remains responsible for character selection. In-game joins first invoke the original return-to-title confirmation flow. Cancelling keeps the current world. Modal confirmation, pending callbacks and map changes are distinct states; old room requests are cancelled before a new one starts.

Steam owns the overlay UI. `Join friends` opens its ordinary friends list; the mod cannot make it a filtered list of QDCoop rooms. `Invite partner` opens the lobby's invitation dialog. No invitations are sent automatically.

## Data path

The existing shared-memory game/bridge mailbox and gameplay wire protocol are unchanged. A loopback UDP adapter forwards its bounded datagrams to `ISteamNetworkingMessages` on channel 17. Each datagram has a small lobby-scoped envelope; incoming messages must come from the expected member of the current lobby. The original message authentication, sequence handling, selective ACK/retry, bounded fragment reassembly and stale snapshot rules still apply.

Steam messages use unreliable, NoNagle, AutoRestartBrokenSession semantics. Reliable gameplay events retain the existing application acknowledgements, avoiding a second reliable stream that would delay newer movement behind old packets. Steam handles direct routing and relay fallback when available. This preview does not prove that every Internet route or account configuration supports relay operation.

The additional adapter runs in a background process, adds a local socket hop and a bounded receive-pump delay, and is not assumed to be free. No movement/animation sampling intervals were lowered. Control files change only for low-frequency room/UI requests and status; pose and combat payloads never use a file mailbox. CPU and end-to-end latency under actual two-account play still require measurement.

## References

- [Steam matchmaking and lobbies](https://partner.steamgames.com/doc/features/multiplayer/matchmaking)
- [ISteamFriends: overlay, rich presence and join callbacks](https://partner.steamgames.com/doc/api/ISteamFriends)
- [ISteamNetworkingMessages](https://partner.steamgames.com/doc/api/ISteamNetworkingMessages)
- [Steam API initialization and callback dispatch](https://partner.steamgames.com/doc/api/steam_api)
