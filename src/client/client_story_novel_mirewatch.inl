// Novel scenes use the existing, tested card boards as compact retellings.
// Juniper's local rescue is a game scene; named novel events cite P1 chapters.
void reviseMirewatchNovel(std::vector<StoryMission>& missions)
{
    novelScene(missions, "mw01_river_teeth", "Teeth in the River", "Chapter 1",
        "Help Reed and his friends reach the riverbank.", {
        panel("Narrator", "Reed is eighteen and hiding from the people who killed his family. He rides toward Mirewatch in the merchant Telos's boat. A crocodile grabs his boot and pulls him overboard!", "cards/reedBaelstone.png", "water"),
        panel("Narrator", "Erevan, a half-elf scout, catches Reed's shirt. On the bank, a gnome healer named Donella raises her staff as another crocodile closes in.", "cards/donellaOfTheMarsh.png"),
        panel("Erevan", "Reed! Take my hand. Donella, can you stop the other one?", "cards/erevanTheShadow.png"),
        panel("Donella", "Bring the boat to the bank. I can raise a root beneath its jaws!", "cards/donellaOfTheMarsh.png")}, {
        panel("Narrator", "They reach shore together. Donella treats Reed's leg and joins the boat. Ahead, Mirewatch's crowded docks wait beneath the flags of the Blackthorn company.", "cards/donellaOfTheMarsh.png")});

    novelScene(missions, "mw03_town_under_company", "Trouble on the Docks", "Chapters 1-3",
        "Help the families find shelter in Mirewatch.", {
        panel("Narrator", "Mirewatch rises above the water on wooden posts. Families fleeing soldiers crowd its docks. Blackthorn guards are taking their belongings.", "cards/mirewatchInformant.png"),
        panel("Reed", "Those people need their food and blankets. Let them through!", "cards/reedBaelstone.png"),
        panel("Joni", "Bring them to my inn. We can make room. But keep your eyes open: a company wagon just passed with someone locked inside.", "cards/joniPumpernickel.png")}, {
        panel("Narrator", "The families reach Joni's inn. Across the channel, a frightened child calls from the boatyard. Erevan turns toward the sound.", "cards/erevanTheShadow.png", "creak")});

    novelScene(missions, "mw04_watcher_protects", "Juniper Holds the Line", "Mirewatch game scene / alongside Chapters 1-3",
        "Help Juniper and Erevan bring the children to safety.", {
        panel("Narrator", "At the boatyard, two children hide under loose timber. Juniper, a local rescuer, reaches them just as company guards arrive.", "cards/juniperFlash.png"),
        panel("Juniper", "Crawl toward my voice. I have you. Erevan, keep those guards away!", "cards/juniperFlash.png"),
        panel("Erevan", "Use your Spark from here. I'll stop the one coming close.", "cards/erevanTheShadow.png")}, {
        panel("Narrator", "Juniper carries the smaller child to the boat. Back at the inn, an orange cat floats through the doorway. He has come looking for help.", "cards/princeVesper.png")});

    novelScene(missions, "s01_hospitality", "The Cat Who Lost His Past", "Chapters 3, 6 and 9",
        "Learn why Vesper must recover his stolen Mirror.", {
        panel("Vesper", "Prince Vesper, if you please. I need help finding a thief named Briar. I trusted him. He stole my Mirror of Nine Lives.", "cards/princeVesper.png"),
        panel("Donella", "What happens if you cannot get it back?", "cards/donellaOfTheMarsh.png"),
        panel("Vesper", "I forget the lives I lived before this one. I must look into it each day. There are people I love whose faces are already fading.", "cards/princeVesper.png"),
        panel("Narrator", "A dock worker saw Briar take the Mirror to the old counting house. Erevan goes with Vesper. Birdie, a local scout, keeps watch on the prisoner wagon.", "cards/erevanTheShadow.png")});

    auto& handover = novelScene(missions, "mw05_watched_office", "The Stolen Mirror", "Chapter 7",
        "Slip past the guards and help Vesper escape.", {
        panel("Narrator", "Briar has set a table for supper. Beside the plates sits Vesper's Mirror. A Blackthorn boat waits below the steps.", "cards/princeVesper.png"),
        panel("Vesper", "You promised supper. You took my memories instead. Give it back.", "cards/princeVesper.png"),
        panel("Narrator", "Briar lifts the case to leave. Erevan grabs one corner while Vesper catches the handle. They nearly have it! Then a guard traps Vesper in a sack.", "cards/erevanTheShadow.png")}, {
        panel("Erevan", "Hold on, Vesper!", "cards/erevanTheShadow.png"),
        panel("Narrator", "Erevan releases the case and cuts his friend free. Briar escapes with the Mirror to Thaeron Baelstone's house. Thaeron is Reed's uncle.", "cards/erevanTheShadow.png")});
    for (auto& entry : handover.briefing)
        if (entry.speaker == "Coach")
            entry.text = "Use Erevan's shadow actions to pass the guards and push the last one away.";

    novelScene(missions, "mw06_cost_seen", "Break Open the Wagon", "Chapters 6, 8 and 9",
        "Stop the prisoner wagon and bring all four captives home.", {
        panel("Birdie", "The wagon leaves at dawn. Scooter, stay close. We need the road clear and the boats ready.", "cards/birdieTheWise.png"),
        panel("Narrator", "Scooter is Birdie's otter. Together they scout the water's edge. Erevan has found the weak joint in the wagon's iron guardian.", "cards/scooter.png"),
        panel("Vesper", "Nettle is inside! She is a fairy from the queen's court. They have hurt her wings. We must get her out.", "cards/princeVesper.png")}, {
        panel("Narrator", "The wagon stops. Erevan strikes the iron guardian's weak joint. Scooter opens the back latch. Nettle, her winged snail Zippy, and two other prisoners reach Joni's inn. The Mirror is still with Reed's uncle.", "cards/nettleStarbright.png")});

    novelScene(missions, "mw08_lesson_night", "The Uncle Behind the Door", "Chapters 10-14",
        "Follow Reed's escape from Thaeron's house.", {
        panel("Narrator", "Thaeron offers to discuss the Mirror if Reed visits. Reed goes alone. He hopes the uncle he loves can explain who destroyed his family.", "cards/reedBaelstone.png"),
        panel("Thaeron", "Your father was going to ruin everything. I knew your mother was there. I chose to preserve what I could.", "cards/thaeronBaelstone.png"),
        panel("Reed", "You knew my mother was in the house. You let it happen to save the company?", "cards/reedBaelstone.png"),
        panel("Narrator", "A guard wounds Reed. Thaeron pins him with magic. Reed knocks over a lamp, breaks free, and jumps from the window into the dark water.", "cards/thaeronBaelstone.png"),
        panel("Birdie", "Scooter found him! Reed, hold the oar. Vesper, bring Donella!", "cards/birdieTheWise.png"),
        panel("Narrator", "Friends carry Reed to Maggie, an old healer whose house rides on a giant gator named Mangletooth. While Reed recovers, they trace the Mirror's carrier south to a prison: the Shrouded Vault.", "cards/maggieMudroot.png")});

    novelScene(missions, "mw10_beautiful_plan", "Open the Shrouded Vault", "Chapters 18-21",
        "Free the prisoners and recover Vesper's Mirror.", {
        panel("Narrator", "The Vault stands beside an old river lock. Vesper meets Pavo, a captive satyr, through a gap beneath the landing. Pavo has seen the Mirror inside.", "cards/pavoQuickstep.png"),
        panel("Vanya", "I'm Vanya. I help the people Blackthorn drives from their homes. I've found boats and shelter for whoever you bring out.", "cards/vanyaBluewater.png"),
        panel("Birdie", "We have boats, rowers, and blankets. Open the cells first. Keep the way back clear.", "cards/birdieTheWise.png"),
        panel("Narrator", "Iven, another prisoner, asks them to save a living branch stolen from her grove. It can help damaged roots grow together. Donella promises to carry it when the guards are out of the way.", "cards/donellaOfTheMarsh.png")}, {
        panel("Narrator", "Donella carries the branch out with Iven, and Erevan retrieves the Mirror. The branch can heal only its home grove. Then a hidden hand pulls a cord. Iron shutters slam down. Water floods the hall.", "cards/erevanTheShadow.png")});

    novelScene(missions, "mw11_no_plan_saves_all", "The Last Boat", "Chapters 21-22 / rescue board adaptation",
        "Protect the escape route while the boats pull free.", {
        panel("Narrator", "Water rises beneath the landing. The boats cannot leave while people are still trapped inside. Reed and Birdie hold the escape route open.", "cards/reedBaelstone.png"),
        panel("Birdie", "Keep moving toward the boats! We will hold the path.", "cards/birdieTheWise.png"),
        panel("Coach", "This card battle retells the rescue with your team. Clear the paths and use Juniper's Intercept to protect the runner.", "cards/juniperFlash.png")}, {
        panel("Narrator", "As Reed gets the living branch aboard, a post falls and crushes his right arm. Birdie ropes him to a safe piling. The rower pulls the post clear, and together they lift him into the boat.", "cards/birdieTheWise.png"),
        panel("Narrator", "Birdie frees Scooter too. Donella tries to open the deeper cells with magic. An iron trap turns the spell back, burning her hands and badly hurting Nettle. The last boat escapes with the Mirror, but people remain trapped behind the iron wall.", "cards/donellaOfTheMarsh.png")});

    novelScene(missions, "s03_published_mystery", "What the Mirror Remembers", "Chapters 23-25",
        "Help the survivors rest and hear Vesper's returning memories.", {
        panel("Narrator", "Maggie splints Reed's arm. Donella asks before helping Nettle again. Nobody can promise a quick recovery. Vesper finally opens his Mirror.", "cards/maggieMudroot.png"),
        panel("Vesper", "Queen Caltheriel! I remember her face. She was taken prisoner. I was searching for her when I lost the Mirror.", "cards/princeVesper.png"),
        panel("Vesper", "And Sylvara, our friend in the forest. Her life is tied to her tree. If the company hurts it, she will suffer too. These are my old memories. They cannot tell me what has happened since.", "cards/princeVesper.png"),
        panel("Narrator", "Iven stays with Maggie to heal and tend her living branch. It belongs to another grove, so it cannot repair Sylvara's tree. The friends need to find Sylvara herself.", "cards/donellaOfTheMarsh.png"),
        panel("Coach", "Later battles use the full card team to retell events. A card's normal moves do not mean Reed's injured arm has healed.", "cards/reedBaelstone.png")});

    novelScene(missions, "s04_wounds_that_vote", "Elliot's Empty Workshop", "Chapters 11, 16, 23 and 25",
        "Follow the clues left by Rowan's missing friend.", {
        panel("Rowan", "I'm Rowan. I'm looking for my friend Elliot, a woodcarver. He helped Victor Greyshard, the company's expedition leader, reach Sylvara's home. Then he tried to stop them. Now he is missing.", "cards/rowanLeafbound.png"),
        panel("Narrator", "Elliot's workshop is empty. His notebook holds a drawing of Sylvara, paths through the trees, and a little stone house. Vesper recognizes the house.", "cards/elliotGreentide.png"),
        panel("Vesper", "I visited them there. Elliot served my tea in a bowl. If the house still stands, I think I can find it.", "cards/princeVesper.png"),
        panel("Narrator", "Victor has taken workers and empty cages east through a crossing into the Feywild. Rowan wants to find Elliot. The others want to learn what is happening to Sylvara and the tree.", "cards/rowanLeafbound.png")});

    novelScene(missions, "mw14_public_lie", "A Door They Must Leave", "Chapters 14 and 26-28",
        "Prepare to leave Mirewatch and follow Victor east.", {
        panel("Narrator", "Blackthorn gave Joni a choice: surrender her inn or return the freed prisoners. She chose to keep the people safe. Now the company takes her building, and friends carry out what they can.", "cards/joniPumpernickel.png"),
        panel("Reed", "My uncle did this. I cannot make it right by promising money I do not have. Tell me what I can carry.", "cards/reedBaelstone.png"),
        panel("Joni", "Start with your own pack. Take care of one another. Come home when you can.", "cards/joniPumpernickel.png"),
        panel("Narrator", "Maggie stays with those who need care. Birdie checks the boats. With Vesper's Mirror packed safely, the search party sets out for the eastern crossing.", "cards/birdieTheWise.png")});

    auto& departure = novelScene(missions, "mw15_title_follows_burden", "Open the Road East", "Chapters 27-29 / departure board adaptation",
        "Clear a path for the party to reach the eastern crossing.", {
        panel("Narrator", "Company patrols watch the roads out of Mirewatch. The party needs a clear route to the crossing before it can follow Victor's wagons.", "cards/birdieTheWise.png"),
        panel("Donella", "Stay together. We have wounded friends and a long road ahead.", "cards/donellaOfTheMarsh.png"),
        panel("Coach", "Use the team on this battle board to open the departure route. Follow the guide and keep the required allies safe.", "cards/joniPumpernickel.png")}, {
        panel("Narrator", "The path opens. Beyond the crossing, the forest feels strange and watchful. Ahead lie two searches: news of the captured queen, and the trail to Sylvara and Elliot.", "cards/princeVesper.png")});
    departure.script[2].heading = "THE FIRST PATH IS CLEAR";
    departure.script[3].heading = "KEEP THE ROAD OPEN";
    departure.script[3].instruction = "Your allies cross the first cleared path. The wounded Veteran still needs help.";
    departure.script[9].instruction = "The others keep watch while Donella reaches the wounded Veteran.";

    novelScene(missions, "mw18_allies_dishonestly", "Wake and Keep Moving", "Chapters 30-35 / forest escape board adaptation",
        "Help the team break through danger on the forest road.", {
        panel("Narrator", "In a forest dream, Reed is home. His parents are alive. His hand is whole. He reaches for them, wishing he could stay. Then a friend's voice breaks through.", "cards/reedBaelstone.png"),
        panel("Vesper", "Reed! Wake up. Follow my voice!", "cards/princeVesper.png"),
        panel("Narrator", "Reed wakes beside his friends, his arm still in its splint. An escaped worker leads them toward Blackthorn's camp, but guards block the road. They must fight their way through.", "cards/mirewatchInformant.png")}, {
        panel("Narrator", "Past the guards, the worker points out Elliot's stone house. A roar shakes the trees as the friends approach its door.", "cards/elliotGreentide.png")});

    novelScene(missions, "mw20_monster_rules", "The Face Beneath the Monster", "Chapters 36-39",
        "Discover who Ashenfang is and what the engine is doing.", {
        panel("Narrator", "At Elliot's house, a wounded creature called Ashenfang charges from the trees. Vesper flies toward her, then stops in shock.", "cards/Ashenfang.png"),
        panel("Vesper", "Sylvara? It is you! What have they done to you?", "cards/princeVesper.png"),
        panel("Narrator", "Her tree has been cut. A machine forces its dead treetop to stay joined to the living stump. Every pull hurts Sylvara. Her own tree is tearing her apart.", "cards/Sylvara.png"),
        panel("Rowan", "The company guard who buried Elliot told me the truth. Victor drowned him. I cannot save my friend. But we can stop this machine and help the people still here.", "cards/rowanLeafbound.png")});

    auto& camp = novelScene(missions, "mw24_agent_not_heir", "Open the Workers' Camp", "Chapters 39-40 / camp rescue board adaptation",
        "Defeat Grask and open the workers' escape route.", {
        panel("Narrator", "Grask, the company's brutal guard, blocks the camp's escape route. An escaped worker returns to help. Mog, a company soldier, decides she will open the way out.", "cards/Mog.png"),
        panel("Mog", "I'll cut the fence. Keep Grask away while they pass through!", "cards/Mog.png"),
        panel("Narrator", "The rescuers split the work: free the camp, stop the engine, and give Donella room to reach Sylvara. Each group needs the others to succeed.", "cards/donellaOfTheMarsh.png")}, {
        panel("Narrator", "As Grask falls, workers escape through Mog's gap in the fence. Erevan and two workers stop the engine. Near the tree, Donella starts cutting Sylvara free. Victor runs toward the engine's controls.", "cards/Mog.png")});
    camp.objective = "Defeat Grask. Keep Reed, Vanya, and Joni alive until the battle ends.";
    camp.hint = "Clear a path to Grask. Use ranged attacks, healing, and your allies' abilities. Other enemies may remain when Grask falls.";
    for (auto& entry : camp.briefing)
        if (entry.speaker == "Coach")
            entry.text = "Defeat Grask. Reed, Vanya, and Joni must survive. This battle board gathers your card team for the camp rescue.";

    novelScene(missions, "mw21_four_losses", "The Engine Starts Again", "Chapter 41",
        "Stop Victor's engine before it tears the living roots apart.", {
        panel("Narrator", "Victor restarts the engine. Chains pull tight. The dead treetop drags against the living stump, and Sylvara cries out.", "cards/victorGreyshard.png"),
        panel("Erevan", "The engine has to stop. Show me which part releases it!", "cards/erevanTheShadow.png"),
        panel("Narrator", "Erevan and the workers break the engine's support. It crashes to a stop. But Fizzlewick, the company's engineer, rides a huge metal walker into the clearing. Its spinning saw swings toward Donella.", "cards/fizzlewickGearwright.png"),
        panel("Birdie", "Get down! I can stop that saw. Give me a clear shot.", "cards/birdieTheWise.png")});

    novelScene(missions, "mw22_clean_shot", "An Arrow for the Saw", "Chapter 41",
        "Help Birdie protect Sylvara from the machine.", {
        panel("Narrator", "Birdie steadies her bow. Sylvara moves in pain beside the engine. The saw leaves only a narrow opening for a shot.", "cards/birdieTheWise.png"),
        panel("Birdie", "Wait. Keep clear. Now!", "cards/birdieTheWise.png"),
        panel("Narrator", "Her arrow jams the saw's moving joint. Reed and Nettle use a lever to lift fallen iron off one small living root. Donella has room to work again.", "cards/reedBaelstone.png"),
        panel("Donella", "Sylvara, I can cut away the dead treetop. I need you to let it go. I will wait for you.", "cards/donellaOfTheMarsh.png")});

    novelScene(missions, "mw23_choice_not_cure", "Let the Dead Branches Go", "Chapter 41",
        "Help Sylvara free her living tree from its dead treetop.", {
        panel("Vesper", "Sylvara, you do not have to keep holding it. We are here.", "cards/princeVesper.png"),
        panel("Narrator", "Sylvara reaches toward a branch that will never move again. Then she lets the living wound open beneath Donella's hand. She is ready.", "cards/Sylvara.png"),
        panel("Donella", "Hold it steady, Nettle. One more cut. There!", "cards/donellaOfTheMarsh.png"),
        panel("Narrator", "The dead treetop comes free. Its branches stay dead. Below them, the roots Reed and Nettle saved grow into the machine's joints. Metal cracks. Fizzlewick's walker falls.", "cards/donellaOfTheMarsh.png"),
        panel("Narrator", "Sylvara's own face begins to show beneath the bark. Donella stays beside her, holding the wound. Victor turns toward them with his sword.", "cards/Sylvara.png")});

    novelScene(missions, "mw24a_victor_in_custody", "Victor's Last Attack", "Chapter 41 / conclusion of the tree battle",
        "See how the fight ends and who escapes.", {
        panel("Narrator", "Victor runs at Donella with his sword raised. Her hands are still holding Sylvara's wound. Pavo knocks the blade aside, but Victor shoves him down.", "cards/victorGreyshard.png"),
        panel("Erevan", "Donella!", "cards/erevanTheShadow.png"),
        panel("Narrator", "Erevan reaches Victor from behind and strikes with his sword. Victor falls dead. Donella is safe, but the others are hurt, and the camp is still burning.", "cards/erevanTheShadow.png"),
        panel("Narrator", "Fizzlewick escapes through a hatch in his fallen machine. The freed workers gather their wounded. The guard who buried Elliot survives too.", "cards/fizzlewickGearwright.png"),
        panel("Rowan", "We came looking for Elliot. Now we know where he is. I am bringing my friend home.", "cards/rowanLeafbound.png")});

    novelScene(missions, "mw25_deed_own_hand", "Bring Elliot Home", "Chapters 43-44",
        "Return to Maggie with Elliot and the wounded.", {
        panel("Narrator", "The journey home is slow. Reed's arm still needs care. Rowan brings Elliot's body to Maggie, who has brought others back from death.", "cards/rowanLeafbound.png"),
        panel("Narrator", "Maggie brings Elliot back as undead. His fingers close around Rowan's hand. He has no pulse, and speaking takes effort. Rowan stays beside him through the night.", "cards/theGhostOfElliotGreentide.png"),
        panel("Elliot", "Rowan?", "cards/theGhostOfElliotGreentide.png"),
        panel("Rowan", "I'm here. Sylvara is alive. We found her.", "cards/rowanLeafbound.png"),
        panel("Narrator", "At dawn, Elliot spots a leak above the window. He struggles to say, 'Roof.' Donella puts a bowl beneath it and tells him the work can wait.", "cards/donellaOfTheMarsh.png")});

    novelScene(missions, "s06_town_owns_itself", "A New Kitchen, an Open Road", "Chapter 45",
        "Rejoin the friends and see what remains ahead.", {
        panel("Narrator", "Joni's old inn belongs to the company now. In a new kitchen, friends share meals and make room for one another. Reed is still recovering. Everyone has something to carry.", "cards/joniPumpernickel.png"),
        panel("Narrator", "Vesper, Pavo, and Nettle keep the Mirror safe as they continue their search for Queen Caltheriel. Back in Mirewatch, a new invitation arrives from Reed's uncle.", "cards/princeVesper.png"),
        panel("Reed", "He wants another private meeting. I will not go. If he has something to say, he can say it where other people can hear. I'm staying with my friends.", "cards/reedBaelstone.png"),
        panel("Narrator", "Thaeron is still free, and Fizzlewick escaped. Iven tends her living branch. Sylvara has saved a seed. There are people and growing things left to care for.", "cards/Sylvara.png"),
        panel("Joni", "There is food on the stove. Sit down while it is warm.", "cards/joniPumpernickel.png")});
}
