# Integrated sound effects

The 41-source ElevenLabs library is integrated into the native client. The runtime loads the 41 PCM WAV masters and 36 short contact/attack slices. Existing music remains on its separate bus. No server deployment or card-rule change is required.

## Runtime

- `src/client/client_audio_data.inl` is generated from `assets/audio/sfx/manifest.json` and `cue-map.json`. It contains all 97 reviewed character profiles and their named action recipes. Regenerate it with `node tools/sfx-generate-runtime.mjs` after changing the JSON mappings.
- `client_audio_plan.cpp` compares visible before/after gameplay state. Local requests carry the selected action index, target, card or ability until a real state change confirms it. Remote actions use structured action resolution and visible outcomes; legacy status text supplements ability identification. This works with the current deployed snapshot format, without a new wire-protocol capability.
- Movement schedules alternating contact samples at roughly 0.02 and 0.49 seconds. Attacks schedule short layers at roughly 0.06 seconds, followed by victim material impacts at 0.18 seconds, within the existing 0.42-second attack animation. Source slice metadata retains its transient offset.
- Character identity offsets come from the reviewed profiles. Combat layers add up to ±0.25 semitone pitch and ±1.2 dB volume variation. The Choir's small/medium/large sprite forms add stable pitch differences. Warning and result cues remain stable.
- Source and victim materials stay separate: blade, bow, gun, bright/dark magic, creature bite, clockwork, wood/bone, flesh/cloth, wingbeat and crystal layers can be combined without extra source recordings.
- Healing requires an actual health increase, status-only attacks do not need damage, Lifedrain restoration is conditional, and mass healing/resources are aggregated. Initial state, unchanged snapshots and clock ticks do not replay combat. Hidden opponent actors do not reveal character casts. Placement-phase hero disclosure does not become a false summon.
- Undefined Spell effects produce rejection feedback and no cast. The ten effect-art recipes are available once those cards have supported effects; this integration does not invent gameplay for artwork.

## UI and environment

Hover is audible on target entry for shared controls, collection/editor rows, dropdown items, visible pieces and hand cards. Clicks use a firmer cue and take precedence in the same frame. Navigation, popups, inspection panels, Conquest subpages and editor menus use opening/closing paper variations. Drag pickup, drop, slider detents and changed list offsets also sound. Typing, stationary hover and redraws stay silent. See SOUND_EFFECTS_INTERACTION_COVERAGE.md for the revised event table.

Login success, verified shop completion/reveal, saves, validation failures, match-ready and clock warnings use their corresponding cues. Card editor and Conquest controls share the same dispatcher. Conquest phase/turn and existing battle-readiness changes alert once on state change, rather than every refresh. Story panels can explicitly author a `soundEffect`; five current river, gate, fire/flood and Mirror beats use it. Dialogue words are never searched at runtime to trigger foley.

The bayou bed loops quietly in story/campaign/Conquest scenes. The SFX and master controls affect it, while music mute affects music alone. Muting or setting a bus to zero stops current effects and drops delayed requests; unmuting does not replay old actions. Playback is bounded to 24 active voices and 64 queued layers; alerts can evict low-priority work. Repeated copies of a cue from duplicate UI paths are coalesced for 75 ms within their bus. Hover has a separate bus so an immediate click remains audible.

## Build and verification

Run `debug_audio_test.bat` from the repository root. It builds the Debug client and regression targets, then runs:

1. `audiotest`: all source/profile references, hover entry/re-entry/modal/disabled/click precedence checks, plus real GameEngine attack/healing fixtures and routing regressions for status-only effects, hidden actors, movement materials, replay suppression, undefined effects, hand swaps and stale rejected intents.
2. `gametest --movement-only`: the existing offline rules/logic regression suite.
3. `storymodetest`: the existing authored story, card/mechanic and objective regression suite.
4. `SteamTactics.exe --audio-smoke-test=output/audio-integration/playback-smoke.json`: validates loading all runtime buffers, starting every master on the audio device, one-shot hover and immediate click playback, clearing pending/current effects on mute or zero volume, and independent music/SFX controls.

The GUI capture harness disables audio and music. Representative UI captures can therefore run without flooding the audio device or changing account state. Build/test logs and the playback result are under `output/audio-integration/`.

The tests validate deterministic routing, assets and actual playback startup. They do not replace a subjective listening/mix pass through every multiplayer scenario. The current snapshot protocol does not carry event IDs or a complete action log; a coalesced remote update can only be reconstructed from the visible results it contains. A future explicit event stream could improve those edge cases without changing the sound library.

## Initial integration verification

- Debug client build passed (`build/Debug/SteamTactics.exe`).
- 858 audio mapping/routing checks passed.
- Offline `gametest --movement-only` and Story Mode suites passed.
- All 41 masters and 36 slices loaded; all 41 masters started on the audio device.
- Master/SFX mute, zero-volume cancellation, independent music mute, no stale replay and preservation of match-ready alerts passed.
- Ten 1280×720 UI captures completed with matching build/source/asset content snapshots; audio-options and action-choice captures were visually inspected.
- Full machine-readable result: `output/audio-integration/verification.json`. Capture manifest: `output/ui-review/audio-integration-final/capture-manifest.json`.

No live multiplayer match was started; the gameplay regression suites run offline.

## Interaction revision

The revision adds audible hover, explicit panel transitions, drag/drop, slider detents and list scrolling, and raises interface playback levels. Movement has fallback recipes for empty/unknown profiles, flight, teleport and tunneling. Accepted casts against unchanged targets remain audible without claiming a healing result. Revised build and playback reports are under `output/audio-integration/revised-*`.

Revision verification passed: the Release client built successfully; 1,472 audio checks and the offline gameplay/Story Mode suites passed. Both Debug and Release playback checks loaded all 77 buffers and started all 41 masters, with one-shot hover and immediate click playback confirmed. Six 1280x720 captures completed with matching build/source/asset snapshots; the action-choice and editor-dropdown captures were visually inspected. Full result: `output/audio-integration/revised-verification.json`. No live multiplayer match was run.
