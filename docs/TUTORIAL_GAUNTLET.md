# Tutorial revision gauntlet

Give each round a fresh output folder. From the game checkout, build the test program, export the compiled campaign catalog, and turn it into reading packets.

```powershell
cmake --build build --config Debug --target storymodetest
build\Debug\storymodetest.exe --export-tutorial output\tutorial-review\round1
python tools\story_review_packets.py output\tutorial-review\round1
```

1. Export a baseline before editing. Check Mirewatch, Blackthorn and Seelie. Each campaign has a story packet for skipping optional practices, a play packet for taking them, a post-story practice packet, and a recovery packet for hints and wrong moves. Mirewatch and Blackthorn tell the rescue from opposite sides and can be played in either order.
2. Give fresh readers just one text packet each. Ask them to retell what the characters want, what the player does to help, what changes, and why the next scene follows. Ask which line first lost them. Use separate readers for the two perspectives. Compare their accounts of shared events after both have replied.
3. Give builders the reader's exact examples. Use the write-prose skill for new wording and edit-prose-patterns to edit a second time. Compare adjacent scenes for repeated endings or voices. Read the board prompt beside the dialogue and keep each instruction where the player needs it. Cut or combine setup that interrupts play. Put the next move beside its reason and show its consequence in dialogue or on the board. Keep a list of blocked decisions and work on the rest.
4. Rebuild, export a fresh folder, and repeat with new readers. Compare panel counts, panel words, longest panel, average words per panel sentence, and guided inputs. Counts help locate slow stretches. A reader who can explain the rescue and make the next move supplies the comprehension check.
5. Run `build\Debug\storymodetest.exe` and exercise the affected inputs in the client. For each mandatory battle, check the instruction against the actual turn, unit, target, and outcome. For an optional drill, check that skipping it preserves the story. Repeat until fresh readers agree on the main goal and can follow it through both perspectives, the actions work, and no specific issue remains. If only blocked decisions remain, ask the outstanding questions together and resume after the reply.

Packets contain authored panel text, act and lesson labels, battle goals and authored action prompts in mission order. The catalog JSON also retains hints and conditional correction text. Dynamic HUD text, placement of overlays, timing, optional mistakes, and mouse/keyboard behavior need in-game verification. An open battle has no scripted input count.

## Comparisons for a new player

| Published tutorial | Question for the reader | Gloomthorn evidence to inspect |
|---|---|---|
| [Magic Foundations Beginner Box](https://magic.wizards.com/en/news/feature/foundations-beginner-box-contents) walks players through a first game turn by turn, then lets them play independently. | Can I make the first move, explain its result, and later choose a move myself? | Follow Reed from his marked step away from the gator to his open fight on the camp road. Check both the mouse and keyboard wording. |
| [Disney Lorcana Gateway](https://www.ravensburger.us/en-US/products/games/family-games/gateway--11098399) adds new card types as players progress through gates. | Do I encounter each rule when it helps me do something? Can I follow the rescue if I skip an advanced drill? | Read Blackthorn with its drills skipped, then with them enabled. Check that Mog still opens the way for the workers. Flag any rule introduced several scenes before use. |

These comparisons concern teaching order. Record observed player actions, the first unexplained term and the reason the next scene follows. Keep agent reviews separate from results of playtests with people.
