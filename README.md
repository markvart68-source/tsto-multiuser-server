# TSTO Multi-User Server

A clean C++20 server foundation for multi-user, multi-device accounts and synchronized towns.

## Included

- Account, device, session, and town domain models
- SQLite schema and idempotent migrations
- Password authentication with salted PBKDF2-HMAC-SHA256-compatible interface placeholder
- Hashed bearer sessions with expiration
- Device registration and account linking
- Town load/save with optimistic revision checking
- Initial route table for register, login, device registration, and town load/save
- Automatic town backups before successful writes

This is a new server foundation and does not include proprietary game protocol compatibility or copyrighted game assets.

## Build

Dependencies:

- CMake 3.20+
- C++20 compiler
- SQLite3 development package

```bash
cmake -S . -B build
cmake --build build
./build/tsto-server
```

On Windows, use Visual Studio or Ninja with the same CMake commands.

## API routes

The transport adapter should map HTTP requests to `ApiRouter::handle`:

| Method | Route | Authentication |
|---|---|---|
| POST | `/v1/auth/register` | No |
| POST | `/v1/auth/login` | No |
| POST | `/v1/devices` | Bearer session |
| GET | `/v1/town` | Bearer session |
| PUT | `/v1/town` | Bearer session; body includes `expected_revision` |

A stale town write returns HTTP 409. The client must reload the latest town and retry.

## Configuration

Copy `config/server.example.json` to `config/server.json` and set a writable database and town directory. Secrets should be supplied through environment variables in production.

## Storage

SQLite stores account metadata, devices, sessions, towns, and backup metadata. Town payloads are opaque blobs so the game-specific protobuf format can be added later without changing account ownership or synchronization logic.
