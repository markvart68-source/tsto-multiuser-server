# Player web panel

The static player panel is in `webpanel/` and is designed to be served at `/webpanel/` by the HTTP transport adapter.

It provides:

- account sign-in and sign-out;
- account summary;
- account-owned currency balances;
- revision-safe town load/save;
- multi-device-safe currency and town access through bearer sessions.

## Expected API endpoints

The panel expects these authenticated endpoints:

- `GET /v1/me`
- `GET /v1/currencies`
- `PUT /v1/currencies` with `{ "currency": "donuts", "amount": 100 }`
- existing `GET /v1/town` and `PUT /v1/town` endpoints

`POST /v1/auth/login` is used by the sign-in form. Currency balances belong to the account, not the device. The server must validate the requested amount and should record an audit event for administrative currency changes.

The `migrations/002_player_currencies.sql` migration adds the account-owned currency table. Apply it after migration 001 before enabling the currency endpoints.

For production, serve the panel over HTTPS, add CSRF protection if cookie authentication is introduced, and do not expose administrative currency editing to player tokens.
