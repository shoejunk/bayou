// Novel scenes use the existing, tested card boards as compact retellings.
// Juniper's local rescue is a game scene; named novel events cite P1 chapters.
void reviseMirewatchNovel(std::vector<StoryMission>& missions)
{
    novelScene(missions, "mw01_river_teeth", "Teeth in the River", "Chapter 1",
        "Help Reed and his friends reach the riverbank.", {
        panel("Narrator", "A gator grabs Reed's boot and pulls him off the boat.", "cards/reedBaelstone.png", "water"),
        panel("Erevan", "Reed! Grab my hand!", "cards/erevanTheShadow.png")}, {
        panel("Donella", "There's the dock. Reed, lean on me. Easy now.", "cards/donellaOfTheMarsh.png")});

    novelScene(missions, "mw03_town_under_company", "Trouble on the Docks", "Chapters 1-3",
        "Help the families find shelter in Mirewatch.", {
        panel("Narrator", "Blackthorn guards block the dock. Families are stuck between the guards and the river.", "cards/mirewatchInformant.png"),
        panel("Joni", "My inn has room. Help me get these people inside.", "cards/joniPumpernickel.png")}, {
        panel("Reed", "Wait. I hear a kid calling from the boatyard.", "cards/reedBaelstone.png", "creak")});

    novelScene(missions, "mw04_watcher_protects", "Juniper Holds the Line", "Mirewatch game scene / alongside Chapters 1-3",
        "Help Juniper and Erevan bring the children to safety.", {
        panel("Narrator", "Two children hide under a boat. Juniper kneels to pull them out. The guards are coming back.", "cards/juniperFlash.png"),
        panel("Juniper", "Take my hand. Erevan, keep them away from us!", "cards/juniperFlash.png")}, {
        panel("Juniper", "Both kids are safe. Let's get them to Joni's inn.", "cards/juniperFlash.png")});

    novelScene(missions, "s01_hospitality", "The Cat and the Empty Case", "Chapters 3, 6 and 9",
        "Find out why Vesper needs his mirror back.", {
        panel("Narrator", "Back at the inn, a cat floats up to Reed with an empty case tucked under one paw.", "cards/princeVesper.png"),
        panel("Vesper", "Every morning that mirror gives me my memories back. A man named Briar took it. Did you see him?", "cards/princeVesper.png"),
        panel("Erevan", "Something silver just flashed by the river steps. Come on!", "cards/erevanTheShadow.png")});

    auto& handover = novelScene(missions, "mw05_watched_office", "The Stolen Mirror", "Chapter 7",
        "Slip past the guards and help Vesper escape.", {
        panel("Narrator", "Briar lowers the mirror into a boat. Vesper hangs on to its handle until a guard drops a sack over his head.", "cards/princeVesper.png"),
        panel("Erevan", "Let go of the mirror! I'll get you out.", "cards/erevanTheShadow.png")}, {
        panel("Narrator", "Erevan cuts Vesper free. Briar gets away by boat, but Birdie sees where he's headed. Reed's uncle lives on that bend.", "cards/erevanTheShadow.png")});
    for (auto& entry : handover.briefing)
        if (entry.speaker == "Coach")
            entry.text = "Slip past the guards with Erevan. Push the last guard away.";

    novelScene(missions, "mw06_cost_seen", "Break Open the Wagon", "Chapters 6, 8 and 9",
        "Stop the prisoner wagon and bring all four captives home.", {
        panel("Birdie", "Hear that banging? Someone's locked in that wagon.", "cards/birdieTheWise.png"),
        panel("Vesper", "I'd know Nettle's voice anywhere! Birdie, can your otter reach the latch?", "cards/princeVesper.png")}, {
        panel("Narrator", "Scooter pops the wagon latch. Nettle tumbles out with three other prisoners. Once they're safe, Reed heads for his uncle's house.", "cards/scooter.png")});

    novelScene(missions, "mw08_lesson_night", "The Uncle Behind the Door", "Chapters 10-14",
        "Follow Reed's escape from Thaeron's house.", {
        panel("Reed", "Uncle, I came for the mirror. Why are your guards drawing swords?", "cards/reedBaelstone.png"),
        panel("Thaeron", "I let your parents die to save this company. What makes you think I'll spare you?", "cards/thaeronBaelstone.png"),
        panel("Narrator", "Reed dives through a window. Mog, a guard riding in Birdie's boat, catches him. Across the river, the guards carry the mirror into a prison.", "cards/birdieTheWise.png")});

    novelScene(missions, "mw10_beautiful_plan", "Open the Shrouded Vault", "Chapters 18-21",
        "Free the prisoners and recover Vesper's mirror.", {
        panel("Narrator", "The guards leave the mirror on a cart outside a cell. A prisoner reaches through the bars and grabs its handle.", "cards/vanyaBluewater.png"),
        panel("Vanya", "Keep that mirror. I'm getting you out first.", "cards/vanyaBluewater.png")}, {
        panel("Narrator", "Vanya pulls the cell doors wide. The prisoner hands Vesper his mirror just as a floodgate opens. Water pours down the stairs.", "cards/princeVesper.png")});

    novelScene(missions, "mw11_no_plan_saves_all", "The Last Boat", "Chapters 21-22 / rescue board adaptation",
        "Protect the escape route while the boats pull free.", {
        panel("Birdie", "Hold that boat! Our runner is still on the landing with the children.", "cards/birdieTheWise.png"),
        panel("Juniper", "I'll stay beside him. If that guard swings, I'll take the hit.", "cards/juniperFlash.png")}, {
        panel("Narrator", "The runner makes it. A post traps Reed's arm. The prisoners lift it and Birdie hauls him aboard. The boat carries the wounded home.", "cards/birdieTheWise.png")});

    novelScene(missions, "s03_published_mystery", "What the Mirror Remembers", "Chapters 23-25",
        "Find out who Vesper remembers.", {
        panel("Narrator", "Maggie sets Reed's broken arm. Beside his bed, Vesper looks into the mirror.", "cards/maggieMudroot.png"),
        panel("Vesper", "I remember who I was looking for! They took my queen, Caltheriel. My friend Sylvara knows that part of the forest.", "cards/princeVesper.png")});

    novelScene(missions, "s04_wounds_that_vote", "Footprints on the East Road", "Chapters 11, 16, 23 and 25",
        "Follow the workers into the forest.", {
        panel("Narrator", "Weeks later, Reed can draw his bow again. Rowan meets him at the dock.", "cards/reedBaelstone.png"),
        panel("Rowan", "Blackthorn marched more workers past my door this morning. My friend Elliot went after them. Come with me. I know the east road.", "cards/rowanLeafbound.png"),
        panel("Vesper", "The east road goes right past Sylvara's tree. Grab your coat, Rowan.", "cards/princeVesper.png")});

    novelScene(missions, "mw14_public_lie", "Bread for the Road", "Chapters 14 and 26-28",
        "Follow Rowan east to find the missing workers.", {
        panel("Narrator", "Guards seize Joni's inn for sheltering the prisoners. Outside, she hands Reed a warm loaf.", "cards/joniPumpernickel.png"),
        panel("Joni", "Feed the first hungry soul you meet. I'll find us another kitchen.", "cards/joniPumpernickel.png")});

    auto& departure = novelScene(missions, "mw15_title_follows_burden", "Open the Road East", "Chapters 27-29 / departure board adaptation",
        "Clear a path for the party to reach the eastern crossing.", {
        panel("Narrator", "The wounded Veteran limps onto the east road. Blackthorn guards close in from both sides.", "cards/birdieTheWise.png"),
        panel("Donella", "Hold on to that spear. I'm coming to get you.", "cards/donellaOfTheMarsh.png")}, {
        panel("Narrator", "Everyone makes it across. A worker stumbles out of the trees, his wrists raw from rope.", "cards/mirewatchInformant.png")});
    departure.script[2].heading = "THE FIRST PATH IS CLEAR";
    departure.script[3].heading = "KEEP THE ROAD OPEN";
    departure.script[3].instruction = "Your allies cross the first cleared path. The wounded Veteran still needs help.";
    departure.script[9].instruction = "Birdie keeps watch while Donella checks the Veteran's wound.";

    novelScene(missions, "mw18_allies_dishonestly", "Four Guards on the Road", "Chapters 30-35 / forest escape board adaptation",
        "Get past the four guards blocking the road to the camp.", {
        panel("Narrator", "The worker eats half of Joni's loaf before he can speak. Four guards block the road back to his friends.", "cards/mirewatchInformant.png"),
        panel("Reed", "Keep the rest. We'll get your friends out.", "cards/reedBaelstone.png")}, {
        panel("Narrator", "On the way to camp, the worker tells Rowan how Elliot died trying to free the prisoners. Rowan stops. Up ahead, something huge limps out of the trees.", "cards/mirewatchInformant.png")});

    novelScene(missions, "mw20_monster_rules", "The Face Beneath the Monster", "Chapters 36-39",
        "Find out why the creature is hurt.", {
        panel("Vesper", "Sylvara? Those are your eyes. What did they do to you?", "cards/princeVesper.png"),
        panel("Narrator", "A chain runs from Sylvara's tree to a machine inside the camp. When the chain snaps tight, Sylvara cries out.", "cards/Ashenfang.png"),
        panel("Rowan", "Is that what's hurting her? Show me the way in.", "cards/rowanLeafbound.png")});

    auto& camp = novelScene(missions, "mw24_agent_not_heir", "Open the Workers' Camp", "Chapters 39-40 / camp rescue board adaptation",
        "Defeat Grask and open the workers' escape route.", {
        panel("Narrator", "Grask guards the gate. Mog slips her knife under the camp fence, right by a worker's outstretched hand.", "cards/Mog.png"),
        panel("Mog", "I helped lock these people in. I'll cut the fence. Keep Grask away from me!", "cards/Mog.png")}, {
        panel("Narrator", "As Grask falls, Mog cuts the wire. Two workers stay behind long enough to shut off the machine. The others run for the gap.", "cards/Mog.png")});
    camp.objective = "Defeat Grask. Keep Reed, Vanya, and Joni alive until the battle ends.";
    camp.hint = "Clear a path to Grask. Use ranged attacks, healing, and your allies' abilities. Other enemies may remain when Grask falls.";
    for (auto& entry : camp.briefing)
        if (entry.speaker == "Coach")
            entry.text = "Defeat Grask. Keep Reed, Vanya, and Joni alive.";

    novelScene(missions, "mw21_four_losses", "The Engine Starts Again", "Chapter 41",
        "Stop Victor's engine before it tears the living roots apart.", {
        panel("Narrator", "Victor, the camp boss, throws the switch. The chains bite into Sylvara's tree again. Erevan and the workers shove the engine off its stand.", "cards/victorGreyshard.png"),
        panel("Birdie", "Look up! The saw on that metal walker is still running!", "cards/birdieTheWise.png")});

    novelScene(missions, "mw22_clean_shot", "An Arrow for the Saw", "Chapter 41",
        "Help Birdie protect Sylvara from the machine.", {
        panel("Birdie", "Donella! Get down!", "cards/birdieTheWise.png"),
        panel("Narrator", "Birdie's arrow jams the saw. Donella reaches Sylvara's tree. One cut will free its roots, if Sylvara can let the dead top fall.", "cards/birdieTheWise.png")});

    novelScene(missions, "mw23_choice_not_cure", "Let the Dead Branches Go", "Chapter 41",
        "Help Sylvara free her living tree from its dead treetop.", {
        panel("Sylvara", "I remember climbing those branches. Cut here. The roots are still alive.", "cards/Sylvara.png"),
        panel("Narrator", "Donella cuts the dead treetop free. The living roots burst through the walker. It tips over with a crash.", "cards/donellaOfTheMarsh.png")});

    novelScene(missions, "mw24a_victor_in_custody", "Victor's Last Attack", "Chapter 41 / conclusion of the tree battle",
        "Get the wounded workers home.", {
        panel("Narrator", "Donella still has both hands on a root when Victor draws his sword on her. Erevan strikes first and kills him. The workers carry their wounded through the gate.", "cards/erevanTheShadow.png"),
        panel("Rowan", "My boat's down here. Who needs a hand getting in?", "cards/rowanLeafbound.png")});

    novelScene(missions, "mw25_deed_own_hand", "The Last Boat Home", "Chapters 43-44",
        "Bring the last workers home.", {
        panel("Narrator", "Rowan steers the last boat into Mirewatch. The two children Juniper saved are waiting on the dock.", "cards/rowanLeafbound.png"),
        panel("Juniper", "Did you get them all?", "cards/juniperFlash.png"),
        panel("Rowan", "Every last worker. Give me a hand with the rope?", "cards/rowanLeafbound.png")});

    novelScene(missions, "s06_town_owns_itself", "A New Kitchen, an Open Road", "Chapter 45",
        "Join the freed workers for supper.", {
        panel("Narrator", "The freed workers crowd Joni's new kitchen. Vesper sets his mirror beside his bowl. Sylvara draws him a map on the back of Joni's menu.", "cards/joniPumpernickel.png"),
        panel("Sylvara", "See this road, Vesper? I saw them take Caltheriel through Nyxara's gates.", "cards/Sylvara.png"),
        panel("Reed", "Uncle Thaeron is still free. He wants me to visit him alone. I won't go.", "cards/reedBaelstone.png"),
        panel("Joni", "Good. Grab a spoon. Your soup's getting cold.", "cards/joniPumpernickel.png")});
}
