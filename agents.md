# Agent Notes

This project is Windows-primary. Build with the `.bat` scripts and CMake;
the game coordinator spawns per-match processes via `CreateProcessA` (see the
`_WIN32` branch of `spawnGameProcess` in `src/gameserver/main.cpp`).

## Build

- `build_debug.bat` / `build_release.bat` — build all targets (client, accounts,
  matchmaking, gameserver, gametest, cardserver) from the CMake `build/` tree.

## Run the services

- `debug_services.bat` starts the accounts, card, game, and matchmaking services
  (plus the Stripe coin service when `.env.stripe` is present). `debug_client.bat`
  launches the client. Release equivalents: `release_services.bat`,
  `release_client.bat`.
- After each client change, run `debug_services.bat`, then run `debug_client.bat`
  once.

## Tests

- `debug_gametest.bat` runs the end-to-end integration test. The movement/logic
  checks run standalone; the matchmaking-and-game portion needs the accounts,
  matchmaking, and game services running and `BAYOU_TEST_PASSWORD` (or
  `BAYOU_SEED_PASSWORD`) set. The test creates its `alpha`/`bravo` accounts if
  they do not exist and submits each account's saved starter deck.

## Project-local delegation

The interactive Codex session remains the main agent. It understands the
request, resolves ambiguity, chooses the overall strategy, makes architectural
and cross-cutting decisions, defines delegated work precisely, integrates and
verifies agent output, resolves conflicts, performs final validation, and
communicates the result.

Delegation is optional and conservative. Prefer the main agent alone when work
is small or localized, relevant files are already known, only one or two
straightforward locations are affected, context would be duplicated, writers
would overlap, or coordination would cost more than it saves. Do not spawn an
agent merely to appear parallel. Future built-in routing may be used instead
when it is demonstrably more appropriate.

Available project-local agents:

- `local-explorer` uses Luna with extra-high reasoning for broad, read-only
  investigation of unfamiliar code, unknown symbols, or several independent
  areas. Do not use it to locate one obvious file.
- `local-test-analyzer` uses Luna with extra-high reasoning when lengthy build,
  test, or log output needs focused independent analysis or when distinguishing
  preexisting failures from regressions. Do not use it for trivial validation.
- `local-implementer` uses Sol with medium reasoning only for a substantial,
  clearly separable implementation workstream with explicit file or component
  ownership. Do not use it automatically for small edits.
- `local-reviewer` uses Sol with medium reasoning for independent review of
  substantial, risky, security-sensitive, architectural, persistence,
  concurrency, authentication, authorization, data-integrity, deployment, or
  public-API changes. Do not invoke it for every trivial edit.

Every delegation must state the exact objective, relevant paths or components,
whether editing is allowed, ownership boundaries, constraints, expected output,
required validation, and definition of done. Never let writing agents edit
overlapping files concurrently. Parallel read-only investigation is allowed
when useful. The main agent must verify important conclusions before relying on
them.

These are removable project-local routing preferences, not replacements for
built-in agents or the selected session model. To restore default behavior,
delete `.codex/agents/local-explorer.toml`,
`.codex/agents/local-test-analyzer.toml`,
`.codex/agents/local-implementer.toml`, and
`.codex/agents/local-reviewer.toml`, then remove this section.
