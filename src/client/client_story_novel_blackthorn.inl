// Adapted from FANTASY_GAUNTLET_RUN_2026_10_15/manuscript/P1.
// Dialogue is condensed for this adaptation. Optional boards teach card rules.
void reviseBlackthornNovel(std::vector<StoryMission>& missions)
{
    novelScene(missions, "bt01_harness_hunger", "The Guard at the Dock",
        "Novel chapters 1-2", "Follow the prison wagon.",
        {
            panel("Narrator", "Mog pulls Reed onto the dock. The other Blackthorn guards roll a cage past him. A girl grips his sleeve through the bars.", "cards/Mog.png"),
            panel("Reed", "Hang on! Where's that wagon going?", "cards/reedBaelstone.png"),
            panel("Mog", "Walk with me, Reed. We can catch it at the bridge.", "cards/Mog.png")
        });

    novelScene(missions, "bt02_customs_bell", "The Loose Knee",
        "Novel chapters 5-6", "Find a way past the wagon guard.",
        {
            panel("Narrator", "A metal guard walks beside the wagon. Its left knee gives a squeak and buckles.", "cards/gearjaw.png"),
            panel("Mog", "Hear that squeak, Reed? Hit that knee. I'll make sure the driver stops.", "cards/Mog.png"),
            panel("Nettle", "Reed? Down here. When you open the cage, give my snail room to turn.", "cards/nettleStarbright_mounted.png")
        });

    novelScene(missions, "bt03_sanctuary_debt", "A Prince in the Way",
        "Novel chapters 7-10", "Get Vesper out of danger.",
        {
            panel("Narrator", "On the way to the bridge, Reed sees a winged cat tugging a mirror from a thief. A guard drops a sack over the cat. The thief jumps into a boat.", "cards/princeVesper.png"),
            panel("Vesper", "My mirror! I need it every morning to get my memories back. Somebody stop that boat!", "cards/princeVesper.png")
        });

    novelScene(missions, "bt04_terms_conditions", "One Pair of Hands",
        "Novel chapter 7", "Free the prince before the wagon comes.",
        {
            panel("Erevan", "Hold still, little cat. There's a knot by your ear.", "cards/erevanTheShadow.png"),
            panel("Narrator", "Reed's friend Erevan cuts the sack open. By the time Vesper pokes his head out, the thief has rowed away. Wagon wheels rattle overhead.", "cards/princeVesper.png"),
            panel("Vesper", "Thanks. Is that someone calling from the wagon?", "cards/princeVesper.png")
        });

    novelScene(missions, "s01_hospitality", "Four Passengers Home",
        "Novel chapters 8-9", "Free the captives at the bridge.",
        {
            panel("Mog", "Halt! There's a rope across the bridge!", "cards/Mog.png"),
            panel("Narrator", "The driver pulls up. Reed strikes the loose knee. As the guard falls, Birdie's otter pops the latch. Three prisoners run for the boat. Nettle rides her snail down the ramp.", "cards/nettleStarbright_mounted.png"),
            panel("Mog", "If my brother asks which boat you took, I was busy fixing a knee.", "cards/Mog.png")
        });

    novelScene(missions, "bt05_freight_office", "Behind the Blackthorn Door",
        "Novel chapters 10-12", "Find out where Blackthorn takes its prisoners.",
        {
            panel("Narrator", "Reed's uncle Thaeron runs Blackthorn. He sends Reed a letter about the stolen mirror. He bought it. Reed can have it if he comes alone.", "cards/thaeronBaelstone.png"),
            panel("Reed", "He taught me to swim off this dock. I'll go. Maybe he'll let the other prisoners go too.", "cards/reedBaelstone.png")
        });

    novelScene(missions, "bt08_north_lock", "The Open Window",
        "Novel chapters 12-15", "Help Reed escape his uncle.",
        {
            panel("Thaeron", "You came for a piece of glass. You dare ask me for my prisoners too? Lock the door.", "cards/thaeronBaelstone.png"),
            panel("Narrator", "Reed kicks a lamp under the guard's boots. As the guard stamps out the fire, Reed jumps through the window. Mog catches him in Birdie's boat.", "cards/reedBaelstone.png", "fire"),
            panel("Mog", "The guards took the mirror into the river prison. Vanya, take my keys. Reed, get the boat to the steps.", "cards/Mog.png")
        });

    novelScene(missions, "bt07_debtor_prison", "Water at the Door",
        "Novel chapters 16-23", "Reach the boats before the prison floods.",
        {
            panel("Narrator", "The guards leave the mirror on a cart. A prisoner grabs it through the bars. Vanya unlocks his cell, and he hands Vesper the mirror. A floodgate opens. Water rushes down the steps.", "cards/vanyaBluewater.png"),
            panel("Narrator", "The last prisoner jumps aboard. A post falls across Reed's arm and pins him to the dock. The prisoners lift it together. Birdie pulls Reed into the boat.", "cards/reedBaelstone.png", "water"),
            panel("Mog", "Leave the oars to me, Reed. Can you still feel my hand?", "cards/Mog.png")
        });

    novelScene(missions, "s03_published_mystery", "What the Mirror Remembers",
        "Novel chapters 23-28", "Find the forest where the other workers were taken.",
        {
            panel("Narrator", "Maggie sets Reed's broken arm. Beside him, Vesper looks into the mirror. He remembers a friend among the trees.", "cards/princeVesper.png", "crystal"),
            panel("Vesper", "My queen, Caltheriel! Sylvara knows the woods where they took her. That's where the guards took the workers too.", "cards/princeVesper.png")
        });

    novelScene(missions, "bt10_stolen_road", "The Workers' Trail",
        "Novel chapters 25-31", "Reach the timber camp.",
        {
            panel("Narrator", "A woman from the prison draws three trees in the mud. Beyond them she marks the timber camp where the guards took her sister.", "cards/mirewatchInformant.png"),
            panel("Mog", "I know that bend. I marched your sister down that trail. I'll bring her home.", "cards/Mog.png")
        });

    novelScene(missions, "bt12_break_charter", "The Sleeping Grove",
        "Novel chapters 29-34", "Wake Reed and cross the woods.",
        {
            panel("Reed", "Mum? I thought I'd never see you again.", "cards/reedBaelstone.png"),
            panel("Narrator", "The grove makes Reed dream while he walks. He reaches for his mother. Above him, a branch breaks. Donella turns into a bear and catches it while Mog pulls Reed clear.", "cards/donellaOfTheMarsh.png"),
            panel("Mog", "Reed, open your eyes. Hear that saw? The camp's close.", "cards/Mog.png")
        });

    novelScene(missions, "bt13a_poisoned_root_road", "Teeth in the Workshop",
        "Novel chapters 35-36", "Free the worker caught in the machine.",
        {
            panel("Narrator", "At the camp, a worker's sleeve catches between two rollers. A brass dog guards the switch that stops them.", "cards/gearjaw.png"),
            panel("Mog", "Fetch!", "cards/Mog.png"),
            panel("Narrator", "Mog throws her glove. The dog chases it. Reed stops the rollers, and the worker pulls free. A saw still whines across the yard.", "cards/gearjaw.png")
        });

    novelScene(missions, "bt15_monster_rules", "The Face Under the Bark",
        "Novel chapters 37-39", "Learn why Sylvara attacks.",
        {
            panel("Narrator", "A creature with burnt bark for skin bursts from the trees. Vesper folds his wings over his head. One claw stops just short of him.", "cards/Ashenfang.png"),
            panel("Vesper", "Sylvara? It's Vesper. Easy now. You can put that claw down.", "cards/princeVesper.png"),
            panel("Mog", "Watch the chain on her tree. She cries out every time the saw pulls it tight.", "cards/Mog.png")
        });

    auto& fence = novelScene(missions, "bt16_clean_shot", "A Hole in the Fence",
        "Novel chapters 39-41", "Get the workers past the guards.",
        {
            panel("Narrator", "Workers crowd the fence. Mog's brother Grask bars the gate. Behind him, Victor, the camp boss, guards the saw.", "cards/Mog.png"),
            panel("Mog", "Grask, it's me. Open up.", "cards/Mog.png"),
            panel("Grask", "Walk away, Mog. I'm keeping this gate shut.", "cards/Grask.png")
        });

    fence.lesson = "MOVE ONE SQUARE";
    fence.objective = "Get Mog to the loose wire at C4.";
    fence.hint = "Drag Mog onto C4. Keyboard: select Mog with Enter, then press Enter on C4.";
    fence.objectiveSpec.kind = StoryObjectiveKind::Scripted;
    fence.pieces = {piece("mog", "Mog", 1, 3, 1), piece("grask", "Grask", 2, 2, 5)};
    fence.masteryRules = {"normal-activation"};
    fence.requiredSurvivorRoles = {"mog"};
    fence.script = {scriptedPieceAction(StoryActionKind::Move, 1, "mog", 3, 2,
        "Axe Swing", "REACH THE FENCE", "Move Mog to C4 while Grask watches the gate.",
        "Drag Mog from B4 to C4, or select her with Enter and confirm C4.")};
    fence.aftermath = {panel("Narrator", "Mog cuts the wire. The workers squeeze through and shut off the saw. Victor shoves his way to the switch and starts it again.", "cards/Mog.png")};

    novelScene(missions, "bt17c_victor_reckoning", "Let the Tree Go",
        "Novel chapters 41-42", "Free Sylvara while the workers escape.",
        {
            panel("Narrator", "The saw rides on a machine with metal legs. Birdie fires an arrow into its gears. While the saw is stuck, Reed and the workers ease the chain off a living root.", "cards/birdieTheWise.png"),
            panel("Donella", "See that green shoot, Sylvara? It'll live. Let go of the dead treetop so I can cut it free.", "cards/donellaOfTheMarsh.png"),
            panel("Narrator", "Sylvara lets go. Donella cuts the treetop free. The roots break the chain and tip the machine on its side. Victor rushes at Donella. Erevan kills him before he can reach her.", "cards/erevanTheShadow.png")
        });

    novelScene(missions, "bt18_deed_own_hand", "The First Green Leaf",
        "Novel chapters 42-43", "Lead the workers back to the river.",
        {
            panel("Narrator", "One green leaf unfolds on the stump. The workers head for the boats. Mog waits beside the last empty seat.", "cards/Sylvara.png"),
            panel("Reed", "Mog! Who do you think we saved that seat for? Give me your hand.", "cards/reedBaelstone.png")
        });

    novelScene(missions, "s06_town_owns_itself", "Supper on the Dock",
        "Novel chapters 44-45", "Bring the survivors home.",
        {
            panel("Narrator", "The woman who drew the map runs down the dock to meet her sister. That night, the sisters make room for Mog at supper.", "cards/maggieMudroot.png"),
            panel("Reed", "Uncle Thaeron is still free. He wants me to come alone again. I won't go. Pass the bread, Mog.", "cards/reedBaelstone.png"),
            panel("Mog", "I'll bring my tools tomorrow. Those cages can come off the boats.", "cards/Mog.png"),
            panel("Sylvara", "Vesper, see this road on my map? I saw them take Caltheriel through Nyxara's gates.", "cards/Sylvara.png"),
            panel("Nettle", "Save my snail a board for a ramp. Vesper and I have a queen to find.", "cards/nettleStarbright_mounted.png")
        });
}
