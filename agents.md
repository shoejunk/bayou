# Agent Notes

This project is Windows-primary. Build with the `.bat` scripts and CMake;
the game coordinator spawns per-match processes via `CreateProcessA` (see the
`_WIN32` branch of `spawnGameProcess` in `src/gameserver/main.cpp`).

## Build

- `build_debug.bat` / `build_release.bat` — build all targets (client, accounts,
  matchmaking, gameserver, gametest, cardserver) from the CMake `build/` tree.
- `build_debug_client.bat` — build only the client (Debug).

## Run the services

- `debug_services.bat` starts the accounts, game, and matchmaking services
  (plus the Stripe coin service when `.env.stripe` is present); card data comes
  from the configured authoritative card server. `debug_client.bat` launches
  the client. Release equivalents: `release_services.bat`,
  `release_client.bat`.

## Reviewing client screens

- Reaching most screens normally requires the accounts, matchmaking and card
  services. `--ui-capture` skips all of that: the client fabricates the account
  state those services supply, walks a list of screens, and writes one PNG per
  screen before exiting. Run it from the repository root so `assets/` resolves.

  ```
  build\Debug\SteamTactics.exe --ui-capture=output\ui-review\round1 ^
      --ui-capture-screens=main-menu,game --ui-capture-size=1920x1080
  ```

  Omit `--ui-capture-screens` for every screen. The seed data and screen list
  live in `src/client/client_ui_capture.{hpp,cpp}` and the `seedCaptureState` /
  `applyCaptureScreen` lambdas in `src/client/main.cpp`; add a screen key there
  when a state is worth reviewing.
- `tools/crop_png.ps1` crops and magnifies a region of a capture so fine detail
  can be inspected without the whole-frame downscale.
- The interface is laid out in an 800x600 logical space that scales to the
  window, so screen coordinates are logical units, not pixels. `drawCrispText`
  in `client_ui.hpp` rasterizes glyphs at device resolution; prefer it over
  `window.draw(text)` so text does not soften under the view transform.

## Tests

- `debug_gametest.bat` runs the end-to-end integration test. The movement/logic
  checks run standalone; the matchmaking-and-game portion needs the accounts,
  matchmaking, and game services running and `BAYOU_TEST_PASSWORD` (or
  `BAYOU_SEED_PASSWORD`) set. The test creates its `alpha`/`bravo` accounts if
  they do not exist and submits each account's saved starter deck.

## Deploy

- For repeated Oracle deployments, use
  `deploy/deploy-cached-servers-linux.sh` with the full `DEPLOY_COMMIT` from
  `origin/main`. Do not create a fresh per-run clone and CMake tree: the helper
  verifies the exact remote tip, protects and cleans only its private managed
  checkout, reuses dependency/build caches, and delegates TLS validation,
  restart, health checks, and rollback to `install-servers-linux.sh`.
