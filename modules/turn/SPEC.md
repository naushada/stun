# TURN Relay — Design Spec

Status: draft (v1 scope). Drives a TDD implementation.

## 1. Goal

Add a minimal, standards-tracked **TURN** (Traversal Using Relays around NAT,
[RFC 8656](https://datatracker.ietf.org/doc/html/rfc8656)) relay server on top
of the existing STUN core. TURN messages are STUN messages with extra methods
and attributes, so the shared `modules/stun/core` message format is extended and
reused rather than duplicated.

The server allocates a relayed UDP transport address per client, relays
application data in both directions, and enforces per-peer permissions and
channel bindings.

## 2. Non-goals (deferred)

These are deliberate deferrals for v1, listed so they can be picked up later:

- **Authentication** — no long-term credentials, `USERNAME`/`REALM`/`NONCE`,
  `MESSAGE-INTEGRITY`, or `FINGERPRINT`. v1 is an unauthenticated relay, like the
  current STUN server. (RFC 8656 requires credential support; it is a follow-up.)
- **TCP / TLS / DTLS** — UDP relay only. `REQUESTED-TRANSPORT` is validated but
  only UDP (17) is accepted; anything else gets `442 Unsupported Transport Protocol`.
- **IPv6** — IPv4 only for v1. The codec layer already handles IPv6; only the
  socket layer and the `INADDR_ANY` bind are IPv4. Keep the door open for a
  dual-stack follow-up.
- **Allocation quotas / rate limits / `RESERVATION-TOKEN` (even port)** — one
  allocation per client 5-tuple, no global quota.
- **`DONT-FRAGMENT`, `EVEN-PORT`, multi-allocation per client, mobility** — out
  of scope.
- **STUN Binding on the same port** — the TURN port speaks TURN only; it does
  not answer plain Binding requests (a follow-up can add Binding on the TURN port).

## Guard rails

Project-wide rules that apply to this and every later module:

- **No redundant function definitions.** An operation is implemented exactly
  once. If the same helper would appear in two places (byte order, address
  conversion, transaction-id generation, …), extract and share it instead of
  copying it.
- **Lowest-level shared utilities live in `modules/framework`** (`inc/`, `src/`,
  its own `CMakeLists.txt`). Examples: big-endian `u16`/`u32` encode/decode
  (`framework::put_u16`, `put_u32`, `get_u16`, `get_u32`), and the event loop.
  The framework may depend only on external libraries (e.g. libevent), never on
  project types (`Endpoint`, `Message`, …).
- **Event loop via libevent.** Readiness notification, timers, and signals go
  through a thin `framework::EventLoop` wrapper over libevent, which is portable
  across epoll/kqueue/select/Winsock. Servers never call `poll`/`select`/`epoll`
  or a blocking recv loop directly. Socket creation and datagram `send`/`recv`
  stay in one place (libevent abstracts the loop, not UDP datagram I/O).
- **Logging API.** The shared `logging/` library exposes one
  `logging::log(level, module, …)` engine with two macro families: `STUN_LOG_*`
  for STUN code and `TURN_LOG_*` for TURN code. Pick the family matching the
  module; never hand-roll printf formatting outside the logger.
- **STUN-domain helpers live in `stun_core`**, not re-implemented per server.
  Examples: `Endpoint` ↔ `sockaddr` conversion (`net.h`), `random_transaction_id()`.
- **Layering:** `framework` → `stun_core` → `turn_core` → servers. A layer may
  depend only on the layers below it, and it must reuse what those export rather
  than re-deriving it.

## 3. Directory layout

Common utilities live in `modules/framework` (see [Guard rails](#guard-rails));
the TURN code mirrors the existing `modules/stun/{core,server}` split:

```
modules/turn/
├── SPEC.md              # this file
├── core/                # TURN message codecs + allocation state (no sockets)
│   ├── inc/turn.h
│   ├── src/turn.cpp
│   ├── test/turn_core_test.cpp
│   └── CMakeLists.txt           → static lib turn_core (links stun_core)
└── server/              # UDP relay server + CLI
    ├── inc/turn_server.h
    ├── src/turn_server.cpp      → lib turn_server (links turn_core, stun_log)
    ├── src/main.cpp             → executable turn-server
    ├── test/turn_server_test.cpp
    └── CMakeLists.txt
```

`modules/stun/core` is extended only with generic RFC 8489 machinery that TURN
needs (method/class helpers and `ERROR-CODE`). TURN-specific methods, attributes
and semantics live in `modules/turn/core`.

## 4. STUN core extensions (`modules/stun/core`)

These are RFC 8489 §6 / §14.8 concepts, not TURN-specific, so they belong in the
shared core and are reusable by the STUN server later.

- **`MessageClass`** enum: `Request=0x00`, `Indication=0x01`,
  `SuccessResponse=0x02`, `ErrorResponse=0x03`.
- **`make_type(method, class)`** — build the 16-bit type from a 12-bit method and
  2-bit class using the RFC 8489 bit-interleaving:
  `((M & 0x0F80) << 2) | ((M & 0x0070) << 1) | (M & 0x000F) | ((C & 2) << 7) | ((C & 1) << 4)`.
- **`method_of(type)`**, **`class_of(type)`** — inverse extraction.
- **`ErrorCode { uint16_t code; std::string reason; }`** with
  `encode_error_code()` / `decode_error_code()`. Wire format: 2 reserved bytes,
  1 byte class (`code/100`), 1 byte number (`code%100`), then a UTF-8 reason phrase.
- New attribute constant `kAttrErrorCode = 0x0009`.

## 5. TURN core (`modules/turn/core`)

Pure, socket-free, unit-testable code.

### 5.1 Methods (RFC 8656 §5)

| Method | Value | Class |
|--------|-------|-------|
| Allocate | `0x003` | request |
| Refresh | `0x004` | request |
| Send | `0x006` | indication |
| Data | `0x007` | indication |
| CreatePermission | `0x008` | request |
| ChannelBind | `0x009` | request |

### 5.2 Attributes (RFC 8656 §14)

| Attribute | Value | Codec |
|-----------|-------|-------|
| `CHANNEL-NUMBER` | `0x000C` | `encode/decode_channel_number` |
| `LIFETIME` | `0x000D` | `encode/decode_lifetime` (u32 big-endian seconds) |
| `XOR-PEER-ADDRESS` | `0x0012` | reuse core XOR address codec |
| `DATA` | `0x0013` | opaque bytes |
| `XOR-RELAYED-ADDRESS` | `0x0016` | reuse core XOR address codec |
| `REQUESTED-TRANSPORT` | `0x0019` | `encode/decode_requested_transport` (u8 protocol + 3 reserved) |

`XOR-PEER-ADDRESS` and `XOR-RELAYED-ADDRESS` reuse the existing
`encode_xor_mapped_address` / `decode_mapped_address(..., xor=true)` — all
`XOR-*` addresses use the magic-cookie + transaction-id mask.

### 5.3 `ChannelData` message (RFC 8656 §12)

Not a STUN message. 4-byte header (2-byte channel number `0x4000..0x7FFF`,
2-byte length) followed by application data. No padding. Demultiplexed from STUN
on the wire by the top two bits of the first byte: STUN = `00`, ChannelData =
`01` (`looks_like_channel_data`).

`encode()` / `decode()` (throws on malformed input).

### 5.4 Allocation state (socket-free)

Peer/client identity is represented by two keys derived from an `Endpoint`:

- `ip_key(endpoint)`  = `"family|address"` — **permission** key. Per RFC 8656
  §2.3, a permission is granted for a peer's IP address (port is ignored).
- `addr_key(endpoint)` = `"family|address|port"` — **channel-binding** key. Per
  RFC 8656 §11, a channel binds a peer's full transport address.

```cpp
struct Allocation {
    std::string client_key;   // addr_key of the client 5-tuple
    uint16_t relayed_port = 0;
    std::chrono::steady_clock::time_point expires_at;

    std::map<std::string, std::chrono::steady_clock::time_point> permissions; // ip_key -> expiry
    std::map<uint16_t, std::string> channels;                                  // channel -> addr_key
    std::map<std::string, uint16_t> peer_channels;                             // addr_key -> channel
};
```

Operations (methods over `Allocation`, taking `Endpoint` + a `now` time point,
so they are unit-testable without sockets):

- `permitted(peer, now)` — the peer's `ip_key` has an unexpired permission.
- `add_permission(peer, now, lifetime)` — insert/refresh a permission.
- `bind_channel(channel, peer, now, lifetime)` — channel ↔ full peer address,
  and it implicitly installs a permission for the peer's IP. Fails if the
  channel or peer is already bound elsewhere.
- `prune_expired_permissions(now)`.
- `peer_for_channel(channel)` / `channel_for_peer(peer)`.
- `expired(now)`.

**Timestamps:** all time is `std::chrono::steady_clock` (project rule: no
`uint64_t` epoch timestamps).

### 5.5 Default lifetimes

- Allocation: **600 s** (refreshable, capped at 3600 s; `LIFETIME=0` deletes).
- Permission: **300 s** (capped at 3600 s).

## 6. TURN server (`modules/turn/server`)

### 6.1 Sockets

- One **client-facing** UDP socket (the TURN server port, default `3478`), bound
  to `INADDR_ANY`.
- One **relay** UDP socket per allocation, bound to `INADDR_ANY:0` (ephemeral
  port). Its bound port is the relayed port. All peer traffic flows through it.
- Advertised relayed address = configured relay IP (default `127.0.0.1`) + the
  bound relay port.

### 6.2 Event loop

`poll()` over `[client_fd, relay_fds...]` with a 1 s timeout so expiry is checked
periodically even when idle. `expire_allocations()` runs each iteration; expired
allocations close their relay socket and are removed.

Demultiplex on the client socket by the first byte's top two bits:

- `00` → STUN message → decode → dispatch by `(class, method)`.
- `01` → `ChannelData` → look up the client's channel binding → relay to peer
  (else drop).

### 6.3 Message handling

| Message | Behaviour |
|---------|-----------|
| **Allocate** (req) | `REQUESTED-TRANSPORT` required; non-UDP → `442`; existing 5-tuple → `437 Allocation Mismatch`. Create relay socket + allocation; respond `XOR-RELAYED-ADDRESS` + `XOR-MAPPED-ADDRESS` + `LIFETIME`. |
| **Refresh** (req) | No allocation → `437`. `LIFETIME=0` → delete allocation, respond `LIFETIME=0`. Else refresh expiry, respond new `LIFETIME`. |
| **CreatePermission** (req) | No allocation → `437`. Add/refresh each `XOR-PEER-ADDRESS` permission. Respond success (no attributes). |
| **ChannelBind** (req) | No allocation → `437`. Parse `CHANNEL-NUMBER` + `XOR-PEER-ADDRESS`; validate channel range; bind. Respond success echoing `CHANNEL-NUMBER`. |
| **Send** (ind) | No allocation → drop (indications get no response). Peer must be permitted, else drop. Forward `DATA` to peer via the relay socket. |
| **Data from peer** | Peer must be permitted (or channel-bound), else drop. Channel-bound → send `ChannelData`; otherwise → `Data` indication with `XOR-PEER-ADDRESS` + `DATA` to the client. |
| **unknown** | Requests → `400 Bad Request` error response; indications → drop. |

### 6.4 Error responses

Error responses echo the request's transaction id and class
(`MessageClass::ErrorResponse`) and carry an `ERROR-CODE`. Codes used: `400`,
`437`, `442`, `500` (internal relay-socket failure). Build via a pure
`make_error_response(request, ErrorCode)` for testability.

## 7. CLI & configuration

```
turn-server [listen_port] [relay_address]
```

- `listen_port` — client-facing UDP port, default `3478`.
- `relay_address` — IP advertised in `XOR-RELAYED-ADDRESS`, default `127.0.0.1`.
- `STUN_LOG_BACKEND` — logging backend, as today (default `stderr`).
- Log module name: `"turn"`.

## 8. Build & run integration

- Add `add_subdirectory(modules/turn/core)` and
  `add_subdirectory(modules/turn/server)` to the top-level `CMakeLists.txt`.
- New test binaries: `turn-core-test`, `turn-server-test`, registered with
  `ctest` alongside the existing `stun-*` tests.
- `scripts/run.sh` gains a `turn` mode:
  `podman run --rm -p 3478:3478/udp stun:latest ./build/turn-server`.
- README updated with a "TURN" section.

## 9. TDD plan

Each cycle: write the failing test → implement the minimum to pass → refactor.
Ordered so every layer depends only on already-tested layers below it.

| # | Layer | Tests first |
|---|-------|-------------|
| C0 | STUN core | `make_type`/`method_of`/`class_of` round-trips against RFC 8489 vectors (Binding 0x0001/0x0101/0x0111, Allocate 0x0003/0x0103, Send 0x0016, Data 0x0017, CreatePermission 0x0008, ChannelBind 0x0009); `ERROR-CODE` encode/decode incl. reason phrase. |
| C1 | TURN core codecs | `REQUESTED-TRANSPORT`, `LIFETIME`, `CHANNEL-NUMBER` round-trips + malformed rejection; `ChannelData` round-trip + truncation rejection + `looks_like_channel_data`. |
| C2 | TURN core allocation | permission add/refresh/expiry, channel bind/lookup, channel-implies-permission, prune. |
| C3 | TURN server builders | success/error response type/class/attributes (Allocate, Refresh, CreatePermission, ChannelBind, error codes). |
| C4 | TURN server integration | socket helpers (`Endpoint`↔`sockaddr`), dispatch routing (mock-free, thin wrappers where feasible), end-to-end loop smoke test. |
| C5 | Build/run | full `ctest` in Podman; manual `run.sh turn` smoke. |

## 10. Testing & build environment

All compilation and tests run in the container via the repo's `Dockerfile`
(Podman first, Docker fallback). No compilers/CMake run on the macOS host.
