// Adapted from FANTASY_GAUNTLET_RUN_2026_10_15/manuscript/P1.
// Dialogue is condensed for this adaptation. Optional boards teach card rules.
void reviseBlackthornNovel(std::vector<StoryMission>& missions)
{
    novelScene(missions, "bt01_harness_hunger", "The Company Owns the Docks",
        "Novel chapters 1-2", "Discover who is being taken from Mirewatch.",
        {
            panel("Narrator", "At Mirewatch's docks, Blackthorn guards seize a family's boat. Reed arrives with Donella, a gnome healer, and Erevan, an elf scout. A cage moves among the freight.", "cards/reedBaelstone.png"),
            panel("Reed", "There is someone in that cage. They are shipping people?", "cards/reedBaelstone.png"),
            panel("Erevan", "Keep your voice down. Those guards work for the company. We need a way out before we try to reach the cage.", "cards/erevanTheShadow.png"),
            panel("Birdie", "My name is Birdie. I scout these waterways. Follow me before the guards notice what you saw.", "cards/birdieTheWise.png"),
            panel("Coach", "These are Blackthorn's cards. Try their moves on an optional practice board to learn what the company can do. The practice is separate from the story.", "cards/blackthornDebtCollector.png")
        },
        {panel("Narrator", "Birdie leads them away. They cannot open the cage here, but they can follow the shipment and find a chance to rescue its passengers.", "cards/birdieTheWise.png")});

    novelScene(missions, "bt02_customs_bell", "A Cage on the Wagon",
        "Novel chapters 5-6", "Find a way past the wagon's metal guard.",
        {
            panel("Narrator", "The wagon carries three prisoners and a winged snail. Nettle, a fairy with injured wings, keeps close to the snail, Zippy. Fizzlewick, the company's engineer, shows Reed the metal guardian watching their cage.", "cards/nettleStarbright_mounted.png"),
            panel("Fizzlewick", "Look closely, Reed. That joint must bear the weight every time it moves. Building a guardian like this takes real skill.", "cards/fizzlewickGearwright.png"),
            panel("Reed", "He was so busy showing off that he showed me a weak joint. If we halt the wagon at the road bridge, we can stop the guardian and reach the cage.", "cards/reedBaelstone.png"),
            panel("Erevan", "Then we need ropes, a clear escape route, and boats waiting. Nobody gets left beside the road.", "cards/erevanTheShadow.png")
        });

    novelScene(missions, "bt03_sanctuary_debt", "The Prince and the Mirror",
        "Novel chapters 7-10", "Learn why Vesper cannot lose the Mirror.",
        {
            panel("Narrator", "While the friends plan the wagon rescue, Prince Vesper brings another problem. The prince has wings, paws, and a cat's body. An elf named Briar holds his enchanted Mirror.", "cards/princeVesper.png"),
            panel("Vesper", "I need that glass. It brings back memories I cannot hold on to by myself. How can I find the people I have lost if I cannot remember them?", "cards/princeVesper.png"),
            panel("Erevan", "Briar has invited us to supper. I will go with you. Stay close, even if he seems friendly.", "cards/erevanTheShadow.png"),
            panel("Narrator", "Briar's welcome hides a sale. He plans to take the Mirror to Thaeron, the man behind Blackthorn. At supper, the trap closes around Vesper.", "cards/thaeronBaelstone.png")
        });

    novelScene(missions, "bt04_terms_conditions", "Save the Prince",
        "Novel chapter 7", "Get Vesper out of Briar's trap.",
        {
            panel("Narrator", "Vesper is caught in a sack. Erevan cannot hold the Mirror's case and cut him free at the same time. He lets the case go and draws his knife.", "cards/erevanTheShadow.png"),
            panel("Erevan", "Vesper! I have you. Keep still while I cut this open.", "cards/erevanTheShadow.png"),
            panel("Narrator", "The prince gets out. Briar escapes with the Mirror and takes it to Thaeron's house. Vesper's memories are still beyond his reach.", "cards/princeVesper.png"),
            panel("Erevan", "We will follow the glass. First we must stop that wagon. The people inside cannot wait for us to win everything at once.", "cards/erevanTheShadow.png")
        });

    novelScene(missions, "s01_hospitality", "Four Passengers Home",
        "Novel chapters 8-9", "Free the wagon's captives at the road bridge.",
        {
            panel("Narrator", "Ropes and roots halt the prison wagon at the road bridge. The metal guardian turns toward the rescuers. Reed remembers the weak joint Fizzlewick showed him.", "cards/reedBaelstone.png"),
            panel("Reed", "There! We can stop it. Get the cage open while it is down!", "cards/reedBaelstone.png"),
            panel("Narrator", "The guardian falls. Nettle, Zippy, and the two other prisoners reach the boats. The friends take all four to Joni's inn.", "cards/nettleStarbright_mounted.png"),
            panel("Joni", "Come inside. There is food and a place to rest. You have spent enough nights behind bars.", "cards/joniPumpernickel.png")
        },
        {panel("Vesper", "All four are safe. Now we need the Mirror back. Thaeron has sent an answer, Reed. It seems he wants you.", "cards/princeVesper.png")});

    novelScene(missions, "bt05_freight_office", "An Uncle's Invitation",
        "Novel chapters 10-12", "Follow Reed toward the meeting he hopes will end the danger.",
        {
            panel("Narrator", "Thaeron holds the Mirror and asks Reed to visit him alone. The company owner is Reed's uncle, the family he still longs to trust.", "cards/thaeronBaelstone.png"),
            panel("Reed", "If my uncle can order these men around, he can make them stop. I have to try to reach him.", "cards/reedBaelstone.png"),
            panel("Narrator", "Reed goes alone. The welcome is warm, but the doors and guards belong to Thaeron. Inside, Reed asks for more than the Mirror.", "cards/thaeronBaelstone.png"),
            panel("Reed", "People are being taken away. Tell me you can stop this.", "cards/reedBaelstone.png")
        });

    novelScene(missions, "bt08_north_lock", "The Welcome Turns Deadly",
        "Novel chapters 12-15", "Get Reed out of Thaeron's house.",
        {
            panel("Narrator", "Thaeron admits destroying Reed's family to protect his fortune. He knew Reed's mother was inside their home. When Reed refuses him, a guard stabs Reed and Thaeron traps his wrist with magic.", "cards/thaeronBaelstone.png"),
            panel("Reed", "You knew she was there. And you still call this protecting our family?", "cards/reedBaelstone.png"),
            panel("Narrator", "Reed throws the guard against the table, knocking over a lamp. Burning oil breaks Thaeron's focus and the spell lets go. Reed abandons the Mirror and escapes through a window. His friends reach him outside.", "cards/birdieTheWise.png"),
            panel("Maggie", "Bring him inside. Mangletooth can carry the house away while I care for him. Mind the steps when he moves.", "cards/maggieMudroot.png"),
            panel("Vesper", "An entire house on an undead alligator. Reed, you chose a remarkable place to recover. Rest. We still have to find where they took the Mirror.", "cards/princeVesper.png")
        });

    novelScene(missions, "bt07_debtor_prison", "The Flooded Vault",
        "Novel chapters 16-23", "Bring the prisoners and the Mirror out before the water rises.",
        {
            panel("Narrator", "The Mirror's trail leads south to the guarded Vault. Erevan slips through the guards' rear window. His friends overpower them, take their keys, and open the gate. A captive satyr named Pavo spots his chance.", "cards/erevanTheShadow.png"),
            panel("Pavo", "I can draw the next guard outside. Get his keys. There are people upstairs, and a mother with children beneath the stairs.", "cards/pavoQuickstep.png"),
            panel("Erevan", "The keys worked. Here is the Mirror, locked in this cupboard. Take the case to the boat. I will help bring the prisoners out.", "cards/erevanTheShadow.png"),
            panel("Donella", "Iven has been forced to care for this living branch for years. I can carry its frame outside. Help her reach the boat.", "cards/donellaOfTheMarsh.png"),
            panel("Narrator", "A hidden trap shuts the exits and floods the hall. Falling timber crushes Reed's arm. His friends pull him free. Iven survives, and her branch and the Mirror reach safety. Other prisoners remain behind a locked barrier.", "cards/reedBaelstone.png"),
            panel("Erevan", "I can still hear them knocking. We brought people out, but there are more. We cannot forget them.", "cards/erevanTheShadow.png")
        });

    novelScene(missions, "s03_published_mystery", "What the Mirror Remembers",
        "Novel chapters 23-28", "Learn why the rescued Mirror matters.",
        {
            panel("Narrator", "At Maggie's refuge, Vesper looks into the rescued Mirror. He must use it each morning to recover his memories. Today he remembers Queen Caltheriel and the life they shared.", "cards/princeVesper.png"),
            panel("Vesper", "Caltheriel is a prisoner. I love her, and I have to find her. Sylvara might know a way. She guards the forest across the eastern crossing; her life is tied to her own tree.", "cards/princeVesper.png"),
            panel("Donella", "Iven wants to stay here with the living branch while her hands heal. It needs its own homeland to do its work. We cannot promise to fix every wound with it.", "cards/donellaOfTheMarsh.png"),
            panel("Rowan", "My friend Elliot is still missing beyond the eastern crossing. He is a carpenter. His drawing may show us the way to him, and to the workers taken east.", "cards/rowanLeafbound.png")
        });

    novelScene(missions, "bt10_stolen_road", "The Road to Elliot",
        "Novel chapters 25-31", "Follow the missing workers into the east.",
        {
            panel("Narrator", "Joni refuses to help Thaeron take Reed back. Blackthorn takes her inn. She opens a kitchen in a crowded drying house while the company offers families new homes in the east.", "cards/joniPumpernickel.png"),
            panel("Joni", "The kitchen needs tables. Carry that end, will you? They have taken my building. People still need supper.", "cards/joniPumpernickel.png"),
            panel("Reed", "Those families think the company will give them a fresh start. We know what its cages look like. We need to find where the boats are taking them.", "cards/reedBaelstone.png"),
            panel("Rowan", "Elliot drew three trees beside a stream. They mark the crossing. I am afraid of that wood, but I am coming with you.", "cards/rowanLeafbound.png")
        },
        {panel("Donella", "Reed, let us carry the heavy packs. Your arm is still hurt. We need you to get there and come home again.", "cards/donellaOfTheMarsh.png")});

    novelScene(missions, "bt12_break_charter", "Do Not Follow the Dream",
        "Novel chapters 29-34", "Cross the sleeping grove together.",
        {
            panel("Narrator", "Between the three trees, a doorway opens into another land beneath a violet sky. The friends cross with the Mirror. On the way to Sylvara, a sleeping grove traps them in dreams they long to believe.", "cards/princeVesper.png"),
            panel("Reed", "My parents are here. My hand works again. Please, just let me stay a little longer.", "cards/reedBaelstone.png"),
            panel("Nettle", "Reed, listen to us! You are in the woods. We are here with you. Come back!", "cards/nettleStarbright.png"),
            panel("Donella", "That branch is falling! My druid magic lets me take a bear's shape. I can hold it up. Pull Reed clear while I do!", "cards/donellaOfTheMarsh.png"),
            panel("Narrator", "Fur spreads over Donella's shoulders. Her bear body catches the branch, giving the others room to rescue Reed. Vesper speaks to the frightened trees. They open a narrow way through.", "cards/donellaOfTheMarsh.png"),
            panel("Vesper", "Keep talking to one another. These trees are hurting. The saws beyond the ridge are cutting into their home.", "cards/princeVesper.png")
        });

    novelScene(missions, "bt13a_poisoned_root_road", "The Brass Dog at the Door",
        "Novel chapters 35-36", "Free the injured worker and escape the workshop.",
        {
            panel("Narrator", "Below the grove, workers are locked in a timber camp. Erevan and Pavo help one woman escape. Searching for shelter, they find Fizzlewick and his brass dog, Gearjaw.", "cards/gearjaw.png"),
            panel("Fizzlewick", "My engine keeps the wood straight so the company can sell it. A worker has caught his hand. Bring your healer, or Gearjaw will fetch the guards.", "cards/fizzlewickGearwright.png"),
            panel("Donella", "The engine is pulling the wood tight through his hand. Release it. I cannot help him while your machine keeps pulling.", "cards/donellaOfTheMarsh.png"),
            panel("Narrator", "Donella frees the hand when Fizzlewick releases the strain. Guards arrive and Gearjaw blocks the door. Reed drops a bench onto the dog. As it tears the bench apart, Fizzlewick pulls its stopping lever. The friends escape through a window.", "cards/gearjaw.png"),
            panel("Birdie", "Fizzlewick has started the timber engine again. He means to keep it working. Patrols are coming. Follow me into the trees. Elliot's house should be ahead.", "cards/birdieTheWise.png")
        });

    novelScene(missions, "bt15_monster_rules", "Ashenfang Has a Name",
        "Novel chapters 37-39", "Discover what happened to Sylvara and Elliot.",
        {
            panel("Narrator", "A creature of blackened bark attacks outside Elliot's empty house. The guards call her Ashenfang. Vesper calls her Sylvara. She strikes him down before the friends can pull him inside.", "cards/Ashenfang.png"),
            panel("Vesper", "I know her. I know how she holds her head when she listens. But she did not know me.", "cards/princeVesper.png"),
            panel("Rowan", "Elliot's notebooks say he and Sylvara loved each other. Her life is tied to her tree. The company cut it, then chained its dead treetop to the living stump. The engine keeps pulling on her wound.", "cards/rowanLeafbound.png"),
            panel("Narrator", "A dwarf knocks. Braun has come from Victor, the camp commander, to demand the workers' surrender. The friends keep the door barred. Rowan asks him what happened to Elliot.", "cards/braunStonefist.png"),
            panel("Braun", "Victor held him under the water. I buried him. And at dawn, Victor will burn this grove unless you surrender. I have told you what I know.", "cards/braunStonefist.png"),
            panel("Donella", "We need two teams. One frees the workers and stops the engine. Mine cuts the dead treetop away without harming the living root. We have until dawn.", "cards/donellaOfTheMarsh.png")
        });

    novelScene(missions, "bt16_clean_shot", "Open a Way Out",
        "Novel chapters 39-41", "Free the prisoners while the root team works.",
        {
            panel("Narrator", "The escaped woman guides Erevan and Pavo back into the camp. Freed workers open cages and carry the wounded. They stop the engine and signal Donella's team at Sylvara's stump.", "cards/pavoQuickstep.png"),
            panel("Erevan", "You with the axe! The fence is burning. They cannot all fit through that gate. Cut this post! Give them another way out!", "cards/erevanTheShadow.png"),
            panel("Narrator", "The half-orc guard, Mog, cuts the post. Workers push the fence down. Her brother Grask aims at the fleeing crowd, and she runs to stop him. Through the smoke, Fizzlewick's giant walker raises its saw.", "cards/Mog.png"),
            panel("Birdie", "I can see the moving joint beside the saw. Hold on. I can buy you a few seconds.", "cards/birdieTheWise.png"),
            panel("Erevan", "Victor has restarted the engine! The others are still working at the stump. Help me stop it before it tears through their hands!", "cards/erevanTheShadow.png")
        });

    novelScene(missions, "bt17c_victor_reckoning", "Save What Is Still Alive",
        "Novel chapters 41-42", "Release Sylvara's tree and protect the rescuers.",
        {
            panel("Narrator", "Birdie's arrow jams the saw. Erevan and two freed workers break the timber engine's support. At the stump, Reed and Nettle lift fallen iron off a small living root.", "cards/birdieTheWise.png"),
            panel("Donella", "Sylvara, I can take the dead wood away. But I need you to let this wound open. I cannot make the treetop live again.", "cards/donellaOfTheMarsh.png"),
            panel("Vesper", "You do not have to keep holding the dead treetop.", "cards/princeVesper.png"),
            panel("Narrator", "Sylvara lets go. Donella cuts the dead wood free. Living roots break the walker's leg; Fizzlewick escapes. Victor rushes at Donella with his sword, and Erevan kills him to protect her.", "cards/erevanTheShadow.png"),
            panel("Vesper", "Sylvara? It is me. Stay with us. Please.", "cards/princeVesper.png")
        },
        {panel("Donella", "She knows you. The bark is falling from her face. Keep talking to her while we help the wounded.", "cards/donellaOfTheMarsh.png")});

    novelScene(missions, "bt18_deed_own_hand", "A Seed for the Great Tree",
        "Novel chapters 42-43", "Help the grove begin to heal.",
        {
            panel("Rowan", "Sylvara, Elliot tried to stop Victor. Here are his notebooks. Take your time with them.", "cards/rowanLeafbound.png"),
            panel("Narrator", "Sylvara gives a glowing seed to the great tree at the forest's heart. It begins to heal, but the cut branches remain dead. She rests inside its bark and asks them to remove the remaining iron slowly.", "cards/Sylvara.png"),
            panel("Vesper", "Caltheriel is still a prisoner. Pavo, keep the Mirror safe. Nettle, we will search for her together while our friends bring the survivors home.", "cards/princeVesper.png"),
            panel("Narrator", "Grask dies in his fight with Mog. She survives and is kept under guard. Braun surrenders and agrees to show Rowan Elliot's grave. The others prepare the wounded for the journey home.", "cards/braunStonefist.png"),
            panel("Rowan", "Maggie brought Mangletooth back from the dead. Perhaps she can try for Elliot. We have to find him and ask her. I will not leave him here.", "cards/rowanLeafbound.png")
        });

    novelScene(missions, "s06_town_owns_itself", "A Place at the Table",
        "Novel chapters 44-45", "Bring the survivors home and face what comes next.",
        {
            panel("Narrator", "Rowan brings Elliot's body to Maggie. As Rowan reads his friend's notebook aloud, her magic calls Elliot back as undead. He has no pulse, but his eyes open and his hand closes around Rowan's.", "cards/maggieMudroot.png"),
            panel("Rowan", "He said my name. Then he asked about Sylvara. Now he is complaining about Maggie's leaking roof. I have never been so glad to hear a complaint.", "cards/rowanLeafbound.png"),
            panel("Reed", "My uncle wants another private meeting. I will not go. He can hear my account in the same room as the workers. We all get to speak.", "cards/reedBaelstone.png"),
            panel("Narrator", "At Joni's new kitchen, neighbors make room for returning families. Thaeron remains free and Fizzlewick is missing. Reed's hands still hurt, and Joni's old inn is still lost. Tonight, they have one another.", "cards/joniPumpernickel.png"),
            panel("Joni", "There is room at this table. Sit down before the fish gets cold. After supper, you can help me fold the cloths.", "cards/joniPumpernickel.png")
        });
}
