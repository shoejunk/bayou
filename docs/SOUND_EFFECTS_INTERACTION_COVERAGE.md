# Interaction sound coverage

The client uses the existing 41 ElevenLabs recordings for active UI feedback, character movement, combat, effects and notifications. This revision makes hover audible and adds direct feedback to controls and panels that previously depended on a later status message.

| Interaction | Feedback and reuse |
| --- | --- |
| Enter a button, tab, checkbox, text field, slider, password icon, collection row, editor row, dropdown item, visible piece or hand card | A quieter, higher pitched `ui_tap` on entry. Moving within the target stays silent. Leaving and returning permits another cue. |
| Click or keyboard activation | A firmer `ui_tap`. Clicks take priority over hover in the same frame and have a separate playback channel so a preceding hover cannot swallow the click. |
| Change keyboard menu focus or board selection | A pitched selection tap. |
| Open a screen, popup, inspection panel or editor dropdown | A rising paper swish. Conquest subpages and editor instruction/unsaved panels also announce their transitions. |
| Close a popup, inspection panel or dropdown | A lower, reversed paper swish. |
| Drag a card, piece or volume slider | One paper pickup when dragging begins. A slider emits quiet taps at 5% detents and a release tap. |
| Drop a card or piece | A drop tap; dropping outside the board/target areas uses the closing swish. Accepted gameplay then supplies the actual action sound. |
| Scroll a collection, deck, hand or editor list | A quiet tap when the list offset changes. Scrolling against a limit stays silent. |
| Edit a deck, draw/discard/reveal cards, move between story panels | Paper movement, with reveal sparkle and outcome feedback where appropriate. |
| Walk, fly, swim, crawl or move a construct | The character's mapped feet, hooves, wings, water, roots or mechanism recipe, with alternating short samples and stable character pitch. Empty and unknown movement profiles have an audible fallback. |
| Teleport or tunnel | Teleport or digging sounds replace ordinary contacts for that action. Teleports have one arrival cue rather than two footsteps. |
| Attack, cast or use an ability | The named character/action recipe, followed by material hits and effects that actually occur. An accepted healing cast still has a soft casting sound when it restores no Health. |
| Heal, hex, control, infest, buff, summon, transform, die or return | The existing shared effect recipes, with character material and pitch differences. Healing success requires an actual Health increase. |
| Purchase, save, submit, reject, earn resources, start a turn, receive an alert or finish a match | Existing confirmation, rejection, coins, bell and victory/defeat cues. |

Only visible, drawn controls nominate hover targets. Disabled buttons, unfocused windows and automated UI captures cannot emit hover cues. Modal inspection, action-choice and confirmation panels clear underlying hover targets. Repeated snapshots, clock ticks, redraws, stationary highlights, typing and continuously recomputed auras stay silent. Combat requests require accepted state changes, so an invalid action cannot sound like a successful attack.

UI feedback is louder than in the initial integration; hover remains softer than activation. Existing SFX/master volume and mute controls apply to every added cue. Music retains its independent control. No additional recordings or generation credits are needed.

Run `debug_audio_test.bat` for routing, hover behavior, offline gameplay/story regressions and audio-device startup checks. The playback smoke test also checks one-shot hover and immediate click playback. Live multiplayer still uses snapshot-based reconstruction, so several remote actions combined into one update may not all be individually recoverable.
