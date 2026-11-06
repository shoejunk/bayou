# Bayou Bonanza sound library

Open [preview.html](preview.html) to audition all 41 sounds, or play [audition-reel.mp3](audition-reel.mp3). The [sound list](SOUND_LIST.md) includes ordered reel timestamps.

- `masters/`: original ElevenLabs MP3 outputs (38 new + 3 reused).
- `wav/`: 44.1 kHz 16-bit mono PCM exports with -3 dBFS peak headroom.
- `slices/`: 0.36–0.42 second contact/attack extracts, selected by strongest energy transient; confirm exact synchronization while integrating.
- `demos/`: eight short examples made from existing layers.
- `manifest.json`: prompts, generation IDs, hashes, durations and source provenance.
- `cue-map.json`: every live card and action, all animation files, UI interaction families and rules.

The master WAV loop beds have a 150 ms wrap crossfade. Other one-shots have tiny edge fades. The native client now uses the reviewed profiles, timed slices, layered variations and independent audio controls. See [runtime integration](INTEGRATION.md). Technical and automated playback checks are supplemented by an in-game listening pass. See [the full audit](AUDIT.md).
