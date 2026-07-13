# Agent Notes

This project is Windows-primary. Build with the `.bat` scripts and CMake;
the game coordinator spawns per-match processes via `CreateProcessA` (see the
`_WIN32` branch of `spawnGameProcess` in `src/gameserver/main.cpp`).

## Build

- `build_debug.bat` / `build_release.bat` — build all targets (client, accounts,
  matchmaking, gameserver, gametest, cardserver) from the CMake `build/` tree.

## Run the services

- `debug_services.bat` starts the accounts, game, and matchmaking services
  (plus the Stripe coin service when `.env.stripe` is present); card data comes
  from the configured authoritative card server. `debug_client.bat` launches
  the client. Release equivalents: `release_services.bat`,
  `release_client.bat`.

## Tests

- `debug_gametest.bat` runs the end-to-end integration test. The movement/logic
  checks run standalone; the matchmaking-and-game portion needs the accounts,
  matchmaking, and game services running and `BAYOU_TEST_PASSWORD` (or
  `BAYOU_SEED_PASSWORD`) set. The test creates its `alpha`/`bravo` accounts if
  they do not exist and submits each account's saved starter deck.
