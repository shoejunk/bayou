-- Seeds the card library with a playable set for Steam Tactics.
-- Attribute conventions read by the game engine (see src/shared/game_data.hpp):
--   int   cost      steam cost to play (units / spells)
--   int   heroCost  hero-budget contribution (heroes only)
--   int   health    hit points
--   int   attack    damage dealt per attack
--   int   range     attack range (Chebyshev distance)
--   int   move      movement steps per action
--   int   attackingMove  1 lets movement target enemies; 0/missing disables it
--   int   power     spell magnitude
--   str   movement  ortho | diag | omni | jump
--   str   effect    damage | heal | steam (spells)
--   str   target    enemy | ally | none (spells)
--   int   canControl 0 prevents a piece from claiming/influencing squares
--   int   growTurns owner turns before a summoned piece can act
--   str   ability   transform | dematerialize | dig | summon
--   str   summon   unit card title created by the summon ability
--   int   abilityUses limited uses for abilities such as dig; negative means unlimited
--   ref   actions   ordered references in card_actions to reusable action records
--   list  abilityLabels labels indexed by the active action state

BEGIN TRANSACTION;

CREATE TABLE IF NOT EXISTS actions (
  name TEXT PRIMARY KEY NOT NULL,
  state INTEGER NOT NULL,
  kind TEXT NOT NULL,
  pattern TEXT NOT NULL,
  min_range INTEGER NOT NULL,
  max_range INTEGER NOT NULL,
  damage INTEGER NOT NULL,
  can_move INTEGER NOT NULL,
  can_attack INTEGER NOT NULL,
  pass_through INTEGER NOT NULL,
  line_of_sight INTEGER NOT NULL,
  status_turns INTEGER NOT NULL,
  cooldown_turns INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS card_actions (
  title TEXT NOT NULL,
  action_name TEXT NOT NULL,
  item_index INTEGER NOT NULL,
  PRIMARY KEY(title, item_index)
);

DELETE FROM card_actions;
DELETE FROM actions;
DELETE FROM card_string_lists;
DELETE FROM card_string_values;
DELETE FROM card_integer_values;
DELETE FROM card_keywords;
DELETE FROM cards;

-- ---- Heroes -------------------------------------------------------------
INSERT INTO cards (title, type, image_path) VALUES ('Steam Baron', 'Hero', 'cards/steam-baron.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Steam Baron', 'heroCost', 100), ('Steam Baron', 'health', 30),
  ('Steam Baron', 'attack', 8), ('Steam Baron', 'range', 1), ('Steam Baron', 'move', 1);
INSERT INTO card_string_values (title, key, value) VALUES ('Steam Baron', 'movement', 'omni');
INSERT INTO card_string_values (title, key, value) VALUES ('Steam Baron', 'WalkAnim', 'animations/steam-baron-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Iron Sentinel', 'Hero', 'cards/clockwork-rook.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Iron Sentinel', 'heroCost', 60), ('Iron Sentinel', 'health', 26),
  ('Iron Sentinel', 'attack', 6), ('Iron Sentinel', 'range', 1), ('Iron Sentinel', 'move', 2);
INSERT INTO card_string_values (title, key, value) VALUES ('Iron Sentinel', 'movement', 'ortho');
INSERT INTO card_string_values (title, key, value) VALUES ('Iron Sentinel', 'WalkAnim', 'animations/clockwork-rook-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Marsh Witch', 'Hero', 'cards/marsh-witch.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Marsh Witch', 'heroCost', 50), ('Marsh Witch', 'health', 16),
  ('Marsh Witch', 'attack', 6), ('Marsh Witch', 'range', 3), ('Marsh Witch', 'move', 2);
INSERT INTO card_string_values (title, key, value) VALUES ('Marsh Witch', 'movement', 'diag');
INSERT INTO card_string_values (title, key, value) VALUES ('Marsh Witch', 'WalkAnim', 'animations/marsh-witch-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Gear Knight', 'Hero', 'cards/clockwork-rook.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Gear Knight', 'heroCost', 40), ('Gear Knight', 'health', 18),
  ('Gear Knight', 'attack', 6), ('Gear Knight', 'range', 1), ('Gear Knight', 'move', 1);
INSERT INTO card_string_values (title, key, value) VALUES ('Gear Knight', 'movement', 'jump');
INSERT INTO card_string_values (title, key, value) VALUES ('Gear Knight', 'WalkAnim', 'animations/clockwork-rook-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Bayou Scout', 'Hero', 'cards/marsh-witch.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Bayou Scout', 'heroCost', 30), ('Bayou Scout', 'health', 12),
  ('Bayou Scout', 'attack', 4), ('Bayou Scout', 'range', 2), ('Bayou Scout', 'move', 3);
INSERT INTO card_string_values (title, key, value) VALUES ('Bayou Scout', 'movement', 'omni');
INSERT INTO card_string_values (title, key, value) VALUES ('Bayou Scout', 'WalkAnim', 'animations/marsh-witch-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Cog Tinker', 'Hero', 'cards/steam-baron.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Cog Tinker', 'heroCost', 20), ('Cog Tinker', 'health', 9),
  ('Cog Tinker', 'attack', 3), ('Cog Tinker', 'range', 1), ('Cog Tinker', 'move', 1);
INSERT INTO card_string_values (title, key, value) VALUES ('Cog Tinker', 'movement', 'omni');
INSERT INTO card_string_values (title, key, value) VALUES ('Cog Tinker', 'WalkAnim', 'animations/steam-baron-walk.png');

-- ---- Units --------------------------------------------------------------
INSERT INTO cards (title, type, image_path) VALUES ('Brass Pawn', 'Unit', 'cards/clockwork-rook.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Brass Pawn', 'cost', 10), ('Brass Pawn', 'health', 4),
  ('Brass Pawn', 'attack', 2), ('Brass Pawn', 'range', 1), ('Brass Pawn', 'move', 1);
INSERT INTO card_string_values (title, key, value) VALUES ('Brass Pawn', 'movement', 'ortho');
INSERT INTO card_string_values (title, key, value) VALUES ('Brass Pawn', 'WalkAnim', 'animations/clockwork-rook-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Rifleman', 'Unit', 'cards/clockwork-rook.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Rifleman', 'cost', 30), ('Rifleman', 'health', 6),
  ('Rifleman', 'attack', 4), ('Rifleman', 'range', 3), ('Rifleman', 'move', 1);
INSERT INTO card_string_values (title, key, value) VALUES ('Rifleman', 'movement', 'ortho');
INSERT INTO card_string_values (title, key, value) VALUES ('Rifleman', 'WalkAnim', 'animations/clockwork-rook-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Clockwork Rook', 'Unit', 'cards/clockwork-rook.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Clockwork Rook', 'cost', 40), ('Clockwork Rook', 'health', 12),
  ('Clockwork Rook', 'attack', 5), ('Clockwork Rook', 'range', 1), ('Clockwork Rook', 'move', 7);
INSERT INTO card_string_values (title, key, value) VALUES ('Clockwork Rook', 'movement', 'ortho');
INSERT INTO card_string_values (title, key, value) VALUES ('Clockwork Rook', 'WalkAnim', 'animations/clockwork-rook-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Steam Bishop', 'Unit', 'cards/clockwork-rook.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Steam Bishop', 'cost', 40), ('Steam Bishop', 'health', 9),
  ('Steam Bishop', 'attack', 5), ('Steam Bishop', 'range', 1), ('Steam Bishop', 'move', 7);
INSERT INTO card_string_values (title, key, value) VALUES ('Steam Bishop', 'movement', 'diag');
INSERT INTO card_string_values (title, key, value) VALUES ('Steam Bishop', 'WalkAnim', 'animations/clockwork-rook-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Automaton Knight', 'Unit', 'cards/clockwork-rook.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Automaton Knight', 'cost', 30), ('Automaton Knight', 'health', 9),
  ('Automaton Knight', 'attack', 4), ('Automaton Knight', 'range', 1), ('Automaton Knight', 'move', 1);
INSERT INTO card_string_values (title, key, value) VALUES ('Automaton Knight', 'movement', 'jump');
INSERT INTO card_string_values (title, key, value) VALUES ('Automaton Knight', 'WalkAnim', 'animations/clockwork-rook-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Dredger', 'Unit', 'cards/clockwork-rook.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Dredger', 'cost', 50), ('Dredger', 'health', 16),
  ('Dredger', 'attack', 6), ('Dredger', 'range', 1), ('Dredger', 'move', 1);
INSERT INTO card_string_values (title, key, value) VALUES ('Dredger', 'movement', 'omni');
INSERT INTO card_string_values (title, key, value) VALUES ('Dredger', 'WalkAnim', 'animations/clockwork-rook-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Spark Drone', 'Unit', 'cards/smoke-bomb-v2.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Spark Drone', 'cost', 20), ('Spark Drone', 'health', 4),
  ('Spark Drone', 'attack', 3), ('Spark Drone', 'range', 2), ('Spark Drone', 'move', 2);
INSERT INTO card_string_values (title, key, value) VALUES ('Spark Drone', 'movement', 'omni');
INSERT INTO card_string_values (title, key, value) VALUES ('Spark Drone', 'WalkAnim', 'animations/clockwork-rook-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Gearwright', 'Unit', 'cards/tinkeringTom.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Gearwright', 'cost', 20), ('Gearwright', 'health', 5),
  ('Gearwright', 'attack', 2), ('Gearwright', 'range', 1), ('Gearwright', 'move', 2);
INSERT INTO card_string_values (title, key, value) VALUES ('Gearwright', 'movement', 'omni');
INSERT INTO card_string_values (title, key, value) VALUES ('Gearwright', 'WalkAnim', 'animations/steam-baron-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Brass Medic', 'Unit', 'cards/steam-baron.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Brass Medic', 'cost', 30), ('Brass Medic', 'health', 7),
  ('Brass Medic', 'attack', 2), ('Brass Medic', 'range', 1), ('Brass Medic', 'move', 2);
INSERT INTO card_string_values (title, key, value) VALUES ('Brass Medic', 'movement', 'diag');
INSERT INTO card_string_values (title, key, value) VALUES ('Brass Medic', 'WalkAnim', 'animations/steam-baron-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Boiler Imp', 'Unit', 'cards/smoke-bomb-v2.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Boiler Imp', 'cost', 10), ('Boiler Imp', 'health', 3),
  ('Boiler Imp', 'attack', 3), ('Boiler Imp', 'range', 1), ('Boiler Imp', 'move', 2);
INSERT INTO card_string_values (title, key, value) VALUES ('Boiler Imp', 'movement', 'omni');
INSERT INTO card_string_values (title, key, value) VALUES ('Boiler Imp', 'WalkAnim', 'animations/clockwork-rook-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Railgunner', 'Unit', 'cards/clockwork-rook.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Railgunner', 'cost', 50), ('Railgunner', 'health', 7),
  ('Railgunner', 'attack', 7), ('Railgunner', 'range', 4), ('Railgunner', 'move', 1);
INSERT INTO card_string_values (title, key, value) VALUES ('Railgunner', 'movement', 'ortho');
INSERT INTO card_string_values (title, key, value) VALUES ('Railgunner', 'WalkAnim', 'animations/clockwork-rook-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Swamp Skiff', 'Unit', 'cards/marsh-witch.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Swamp Skiff', 'cost', 20), ('Swamp Skiff', 'health', 6),
  ('Swamp Skiff', 'attack', 2), ('Swamp Skiff', 'range', 1), ('Swamp Skiff', 'move', 3);
INSERT INTO card_string_values (title, key, value) VALUES ('Swamp Skiff', 'movement', 'ortho');
INSERT INTO card_string_values (title, key, value) VALUES ('Swamp Skiff', 'WalkAnim', 'animations/marsh-witch-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Arc Lantern', 'Unit', 'cards/smoke-bomb-v2.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Arc Lantern', 'cost', 30), ('Arc Lantern', 'health', 5),
  ('Arc Lantern', 'attack', 4), ('Arc Lantern', 'range', 2), ('Arc Lantern', 'move', 1);
INSERT INTO card_string_values (title, key, value) VALUES ('Arc Lantern', 'movement', 'diag');
INSERT INTO card_string_values (title, key, value) VALUES ('Arc Lantern', 'WalkAnim', 'animations/clockwork-rook-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Sprocket Swarm', 'Unit', 'cards/clockwork-rook.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Sprocket Swarm', 'cost', 20), ('Sprocket Swarm', 'health', 5),
  ('Sprocket Swarm', 'attack', 3), ('Sprocket Swarm', 'range', 1), ('Sprocket Swarm', 'move', 2);
INSERT INTO card_string_values (title, key, value) VALUES ('Sprocket Swarm', 'movement', 'jump');
INSERT INTO card_string_values (title, key, value) VALUES ('Sprocket Swarm', 'WalkAnim', 'animations/clockwork-rook-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Chain Harpoon', 'Unit', 'cards/clockwork-rook.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Chain Harpoon', 'cost', 40), ('Chain Harpoon', 'health', 8),
  ('Chain Harpoon', 'attack', 5), ('Chain Harpoon', 'range', 2), ('Chain Harpoon', 'move', 1);
INSERT INTO card_string_values (title, key, value) VALUES ('Chain Harpoon', 'movement', 'ortho');
INSERT INTO card_string_values (title, key, value) VALUES ('Chain Harpoon', 'WalkAnim', 'animations/clockwork-rook-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Mudslide', 'Unit', 'cards/marsh-witch.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Mudslide', 'cost', 40), ('Mudslide', 'health', 11),
  ('Mudslide', 'attack', 4), ('Mudslide', 'range', 1), ('Mudslide', 'move', 2);
INSERT INTO card_string_values (title, key, value) VALUES ('Mudslide', 'movement', 'diag');
INSERT INTO card_string_values (title, key, value) VALUES ('Mudslide', 'WalkAnim', 'animations/marsh-witch-walk.png');

-- ---- On The Battlefield imports ----------------------------------------
-- Playable-card costs and hero-budget values are stored directly on the
-- game's 10x energy scale.

INSERT INTO cards (title, type, image_path) VALUES ('Automatick', 'Unit', 'cards/automatick.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Automatick', 'cost', 30), ('Automatick', 'health', 1),
  ('Automatick', 'attack', 1), ('Automatick', 'range', 1),
  ('Automatick', 'move', 2),
  ('Automatick', 'canControl', 0);
INSERT INTO card_string_values (title, key, value) VALUES
  ('Automatick', 'movement', 'jump'),
  ('Automatick', 'detail', 'Automatick jumps over other pieces in an L-shape.');
INSERT INTO card_keywords (title, keyword) VALUES ('Automatick', 'mechanical');

INSERT INTO cards (title, type, image_path) VALUES ('Bramble Drone', 'Unit', 'cards/bramble-drone.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Bramble Drone', 'cost', 20), ('Bramble Drone', 'health', 2),
  ('Bramble Drone', 'attack', 2), ('Bramble Drone', 'range', 1),
  ('Bramble Drone', 'move', 1), ('Bramble Drone', 'growTurns', 3);
INSERT INTO card_string_values (title, key, value) VALUES
  ('Bramble Drone', 'movement', 'omni'),
  ('Bramble Drone', 'detail', 'Bramble Drone grows for three turns, then moves or attacks one square in any direction.');
INSERT INTO card_keywords (title, keyword) VALUES ('Bramble Drone', 'bio');

INSERT INTO cards (title, type, image_path) VALUES ('Choking Blossom', 'Unit', 'cards/choking-blossom.jpg');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Choking Blossom', 'cost', 100), ('Choking Blossom', 'health', 2),
  ('Choking Blossom', 'attack', 1), ('Choking Blossom', 'range', 2),
  ('Choking Blossom', 'move', 2);
INSERT INTO card_string_values (title, key, value) VALUES
  ('Choking Blossom', 'movement', 'omni'),
  ('Choking Blossom', 'detail', 'Moves or attacks up to two squares in any direction. Its attack deals damage and disables the target.');
INSERT INTO card_keywords (title, keyword) VALUES
  ('Choking Blossom', 'bio'), ('Choking Blossom', 'riffraff');

INSERT INTO cards (title, type, image_path) VALUES ('Curious Spirit', 'Unit', 'cards/curious-spirit.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Curious Spirit', 'cost', 80), ('Curious Spirit', 'health', 1),
  ('Curious Spirit', 'attack', 0), ('Curious Spirit', 'range', 1),
  ('Curious Spirit', 'move', 7);
INSERT INTO card_string_values (title, key, value) VALUES
  ('Curious Spirit', 'movement', 'diag'),
  ('Curious Spirit', 'detail', 'Glides diagonally across the board without attacking.');
INSERT INTO card_keywords (title, keyword) VALUES ('Curious Spirit', 'occult');

INSERT INTO cards (title, type, image_path) VALUES ('Delving Daphodilus', 'Unit', 'cards/delving-daphodilus.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Delving Daphodilus', 'cost', 120), ('Delving Daphodilus', 'health', 2),
  ('Delving Daphodilus', 'attack', 2), ('Delving Daphodilus', 'range', 1),
  ('Delving Daphodilus', 'move', 7), ('Delving Daphodilus', 'abilityUses', -1);
INSERT INTO card_string_values (title, key, value) VALUES
  ('Delving Daphodilus', 'movement', 'diag'),
  ('Delving Daphodilus', 'ability', 'dig'),
  ('Delving Daphodilus', 'detail', 'Glides diagonally, can dig one hole, and can travel between any two holes.');
INSERT INTO card_keywords (title, keyword) VALUES
  ('Delving Daphodilus', 'bio'), ('Delving Daphodilus', 'riffraff');

INSERT INTO cards (title, type, image_path) VALUES ('Elias Tiberion', 'Hero', 'cards/elias-tiberion.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Elias Tiberion', 'heroCost', 40), ('Elias Tiberion', 'health', 2),
  ('Elias Tiberion', 'attack', 3), ('Elias Tiberion', 'range', 1),
  ('Elias Tiberion', 'move', 2);
INSERT INTO card_string_values (title, key, value) VALUES
  ('Elias Tiberion', 'movement', 'ortho'),
  ('Elias Tiberion', 'ability', 'transform'),
  ('Elias Tiberion', 'detail', 'Move up to two squares with the gun lowered, or raise it to make a stationary short-range attack.');
INSERT INTO card_keywords (title, keyword) VALUES ('Elias Tiberion', 'riffraff');
INSERT INTO card_string_lists (title, key, item_index, value) VALUES
  ('Elias Tiberion', 'abilityLabels', 0, 'Ready Gun'),
  ('Elias Tiberion', 'abilityLabels', 1, 'Lower Gun');

INSERT INTO cards (title, type, image_path) VALUES ('Gentle Bot', 'Unit', 'cards/gentle-bot.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Gentle Bot', 'cost', 0), ('Gentle Bot', 'health', 1),
  ('Gentle Bot', 'attack', 0), ('Gentle Bot', 'range', 1),
  ('Gentle Bot', 'move', 1), ('Gentle Bot', 'canControl', 0);
INSERT INTO card_string_values (title, key, value) VALUES
  ('Gentle Bot', 'movement', 'omni'),
  ('Gentle Bot', 'detail', 'Gentle Bot moves one square in any direction and cannot attack.');
INSERT INTO card_keywords (title, keyword) VALUES ('Gentle Bot', 'mechanical');

INSERT INTO cards (title, type, image_path) VALUES ('Gloom Fairy', 'Unit', 'cards/gloom-fairy.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Gloom Fairy', 'cost', 90), ('Gloom Fairy', 'health', 1),
  ('Gloom Fairy', 'attack', 1), ('Gloom Fairy', 'range', 1),
  ('Gloom Fairy', 'move', 7);
INSERT INTO card_string_values (title, key, value) VALUES
  ('Gloom Fairy', 'movement', 'omni'),
  ('Gloom Fairy', 'detail', 'Flutters up to seven squares in any direction and deals one damage.');
INSERT INTO card_keywords (title, keyword) VALUES ('Gloom Fairy', 'occult');

INSERT INTO cards (title, type, image_path) VALUES ('Hired Gun', 'Unit', 'cards/hired-gun.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Hired Gun', 'cost', 120), ('Hired Gun', 'health', 2),
  ('Hired Gun', 'attack', 3), ('Hired Gun', 'range', 1),
  ('Hired Gun', 'move', 4);
INSERT INTO card_string_values (title, key, value) VALUES
  ('Hired Gun', 'movement', 'ortho'),
  ('Hired Gun', 'ability', 'transform'),
  ('Hired Gun', 'detail', 'Move two to four squares with the gun lowered, or raise it to make a stationary short-range attack.');
INSERT INTO card_keywords (title, keyword) VALUES ('Hired Gun', 'riffraff');
INSERT INTO card_string_lists (title, key, item_index, value) VALUES
  ('Hired Gun', 'abilityLabels', 0, 'Ready Gun'),
  ('Hired Gun', 'abilityLabels', 1, 'Lower Gun');

INSERT INTO cards (title, type, image_path) VALUES ('Hop Bot', 'Unit', 'cards/hop-bot.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Hop Bot', 'cost', 5), ('Hop Bot', 'health', 1),
  ('Hop Bot', 'attack', 1), ('Hop Bot', 'range', 1),
  ('Hop Bot', 'move', 2), ('Hop Bot', 'canControl', 0);
INSERT INTO card_string_values (title, key, value) VALUES
  ('Hop Bot', 'movement', 'jump'),
  ('Hop Bot', 'detail', 'Hops over an adjacent piece to an empty square, damaging it if it is an enemy.');
INSERT INTO card_keywords (title, keyword) VALUES ('Hop Bot', 'mechanical');

INSERT INTO cards (title, type, image_path) VALUES ('Lady Worthington', 'Unit', 'cards/lady-worthington.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Lady Worthington', 'cost', 160), ('Lady Worthington', 'health', 1),
  ('Lady Worthington', 'attack', 0), ('Lady Worthington', 'range', 1),
  ('Lady Worthington', 'move', 7);
INSERT INTO card_string_values (title, key, value) VALUES
  ('Lady Worthington', 'movement', 'omni'),
  ('Lady Worthington', 'ability', 'dematerialize'),
  ('Lady Worthington', 'detail', 'Dematerialize to become hidden from the opponent and move through pieces; materialize to become visible.');
INSERT INTO card_keywords (title, keyword) VALUES ('Lady Worthington', 'occult');
INSERT INTO card_string_lists (title, key, item_index, value) VALUES
  ('Lady Worthington', 'abilityLabels', 0, 'Dematerialize'),
  ('Lady Worthington', 'abilityLabels', 1, 'Materialize');

INSERT INTO cards (title, type, image_path) VALUES ('Lesser Demon', 'Unit', 'cards/lesser-demon.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Lesser Demon', 'cost', 160), ('Lesser Demon', 'health', 2),
  ('Lesser Demon', 'attack', 2), ('Lesser Demon', 'range', 1),
  ('Lesser Demon', 'move', 4);
INSERT INTO card_string_values (title, key, value) VALUES
  ('Lesser Demon', 'movement', 'ortho'),
  ('Lesser Demon', 'detail', 'Charges up to four squares orthogonally and deals two damage.');
INSERT INTO card_keywords (title, keyword) VALUES ('Lesser Demon', 'occult');

INSERT INTO cards (title, type, image_path) VALUES ('Patrol Bot', 'Unit', 'cards/patrol-bot.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Patrol Bot', 'cost', 10), ('Patrol Bot', 'health', 1),
  ('Patrol Bot', 'attack', 2), ('Patrol Bot', 'range', 1),
  ('Patrol Bot', 'move', 1), ('Patrol Bot', 'canControl', 0);
INSERT INTO card_string_values (title, key, value) VALUES
  ('Patrol Bot', 'movement', 'horizontal'),
  ('Patrol Bot', 'detail', 'Moves one square horizontally and attacks one square diagonally.');
INSERT INTO card_keywords (title, keyword) VALUES ('Patrol Bot', 'mechanical');

INSERT INTO cards (title, type, image_path) VALUES ('Professor Glumpkin', 'Hero', 'cards/professor-glumpkin.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Professor Glumpkin', 'heroCost', 70), ('Professor Glumpkin', 'health', 1),
  ('Professor Glumpkin', 'attack', 1), ('Professor Glumpkin', 'range', 1),
  ('Professor Glumpkin', 'move', 1);
INSERT INTO card_string_values (title, key, value) VALUES
  ('Professor Glumpkin', 'movement', 'omni'),
  ('Professor Glumpkin', 'ability', 'summon'),
  ('Professor Glumpkin', 'summon', 'Gentle Bot'),
  ('Professor Glumpkin', 'detail', 'Moves or attacks one square in any direction. Summons a Gentle Bot into the space in front.');
INSERT INTO card_keywords (title, keyword) VALUES ('Professor Glumpkin', 'occult');

INSERT INTO cards (title, type, image_path) VALUES ('River Dweller', 'Unit', 'cards/river-dweller.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('River Dweller', 'cost', 80), ('River Dweller', 'health', 1),
  ('River Dweller', 'attack', 0), ('River Dweller', 'range', 1),
  ('River Dweller', 'move', 7);
INSERT INTO card_string_values (title, key, value) VALUES
  ('River Dweller', 'movement', 'diag'),
  ('River Dweller', 'detail', 'Glides diagonally across the board without attacking.');
INSERT INTO card_keywords (title, keyword) VALUES ('River Dweller', 'occult');

INSERT INTO cards (title, type, image_path) VALUES ('Rustbucket', 'Unit', 'cards/rustbucket.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Rustbucket', 'cost', 50), ('Rustbucket', 'health', 1),
  ('Rustbucket', 'attack', 1), ('Rustbucket', 'range', 2),
  ('Rustbucket', 'move', 1), ('Rustbucket', 'canControl', 0);
INSERT INTO card_string_values (title, key, value) VALUES
  ('Rustbucket', 'movement', 'omni'),
  ('Rustbucket', 'detail', 'Moves one square or fires up to two squares away; firing disables it for its next turn.');
INSERT INTO card_keywords (title, keyword) VALUES ('Rustbucket', 'mechanical');

INSERT INTO cards (title, type, image_path) VALUES ('Scarlett Glumpkin', 'Hero', 'cards/scarlett-glumpkin.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Scarlett Glumpkin', 'heroCost', 20), ('Scarlett Glumpkin', 'health', 2),
  ('Scarlett Glumpkin', 'attack', 0), ('Scarlett Glumpkin', 'range', 2),
  ('Scarlett Glumpkin', 'move', 2);
INSERT INTO card_string_values (title, key, value) VALUES
  ('Scarlett Glumpkin', 'movement', 'omni'),
  ('Scarlett Glumpkin', 'detail', 'Moves up to two squares in any direction and can disable an enemy for two turns without dealing damage.');
INSERT INTO card_keywords (title, keyword) VALUES ('Scarlett Glumpkin', 'bio');

INSERT INTO cards (title, type, image_path) VALUES ('Sentroid', 'Unit', 'cards/sentroid.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Sentroid', 'cost', 10), ('Sentroid', 'health', 1),
  ('Sentroid', 'attack', 1), ('Sentroid', 'range', 1),
  ('Sentroid', 'move', 1), ('Sentroid', 'canControl', 0);
INSERT INTO card_string_values (title, key, value) VALUES
  ('Sentroid', 'movement', 'vertical'),
  ('Sentroid', 'detail', 'Moves one square vertically and attacks one square diagonally.');
INSERT INTO card_keywords (title, keyword) VALUES ('Sentroid', 'mechanical');

INSERT INTO cards (title, type, image_path) VALUES ('Stingy', 'Unit', 'cards/stingy.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Stingy', 'cost', 30), ('Stingy', 'health', 1),
  ('Stingy', 'attack', 1), ('Stingy', 'range', 1),
  ('Stingy', 'move', 7), ('Stingy', 'canControl', 0);
INSERT INTO card_string_values (title, key, value) VALUES
  ('Stingy', 'movement', 'diag'),
  ('Stingy', 'detail', 'Dashes across the board diagonally and deals one damage.');
INSERT INTO card_keywords (title, keyword) VALUES ('Stingy', 'mechanical');

INSERT INTO cards (title, type, image_path) VALUES ('Sweetykins', 'Unit', 'cards/sweetykins.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Sweetykins', 'cost', 50), ('Sweetykins', 'health', 1),
  ('Sweetykins', 'attack', 1), ('Sweetykins', 'range', 1),
  ('Sweetykins', 'move', 7), ('Sweetykins', 'canControl', 0);
INSERT INTO card_string_values (title, key, value) VALUES
  ('Sweetykins', 'movement', 'ortho'),
  ('Sweetykins', 'detail', 'Charges across the board orthogonally and deals one damage.');
INSERT INTO card_keywords (title, keyword) VALUES ('Sweetykins', 'mechanical');

INSERT INTO cards (title, type, image_path) VALUES ('Telematron', 'Unit', 'cards/telematron.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Telematron', 'cost', 100), ('Telematron', 'health', 1),
  ('Telematron', 'attack', 1), ('Telematron', 'range', 1),
  ('Telematron', 'move', 7), ('Telematron', 'canControl', 0);
INSERT INTO card_string_values (title, key, value) VALUES
  ('Telematron', 'movement', 'omni'),
  ('Telematron', 'detail', 'Teleports to any empty square or attacks an adjacent enemy.');
INSERT INTO card_keywords (title, keyword) VALUES ('Telematron', 'mechanical');

INSERT INTO cards (title, type, image_path) VALUES ('Tinkering Tom', 'Hero', 'cards/tinkering-tom.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Tinkering Tom', 'heroCost', 30), ('Tinkering Tom', 'health', 1),
  ('Tinkering Tom', 'attack', 1), ('Tinkering Tom', 'range', 1),
  ('Tinkering Tom', 'move', 1), ('Tinkering Tom', 'canControl', 0);
INSERT INTO card_string_values (title, key, value) VALUES
  ('Tinkering Tom', 'movement', 'omni'),
  ('Tinkering Tom', 'detail', 'Moves or attacks one square in any direction.');
INSERT INTO card_keywords (title, keyword) VALUES ('Tinkering Tom', 'mechanical');

-- Imported team-colored board tokens and walking sprite sheets.
INSERT INTO card_string_values (title, key, value) VALUES
  ('Automatick', 'TokenBlue', 'characters/blue/automatick.png'),
  ('Automatick', 'TokenRed', 'characters/red/automatick.png'),
  ('Automatick', 'WalkAnimBlue', 'animations/blue/automatick-walk.png'),
  ('Automatick', 'WalkAnimRed', 'animations/red/automatick-walk.png'),
  ('Bramble Drone', 'TokenBlue', 'characters/blue/brambleDrone.png'),
  ('Bramble Drone', 'TokenRed', 'characters/red/brambleDrone.png'),
  ('Bramble Drone', 'WalkAnimBlue', 'animations/blue/brambleDrone-walk.png'),
  ('Bramble Drone', 'WalkAnimRed', 'animations/red/brambleDrone-walk.png'),
  ('Choking Blossom', 'TokenBlue', 'characters/blue/chokingBlossom.png'),
  ('Choking Blossom', 'TokenRed', 'characters/red/chokingBlossom.png'),
  ('Choking Blossom', 'WalkAnimBlue', 'animations/blue/chokingBlossom-walk.png'),
  ('Choking Blossom', 'WalkAnimRed', 'animations/red/chokingBlossom-walk.png'),
  ('Curious Spirit', 'TokenBlue', 'characters/blue/curiousSpirit.png'),
  ('Curious Spirit', 'TokenRed', 'characters/red/curiousSpirit.png'),
  ('Curious Spirit', 'WalkAnimBlue', 'animations/blue/curiousSpirit-walk.png'),
  ('Curious Spirit', 'WalkAnimRed', 'animations/red/curiousSpirit-walk.png'),
  ('Delving Daphodilus', 'TokenBlue', 'characters/blue/delvingDaphodilus.png'),
  ('Delving Daphodilus', 'TokenRed', 'characters/red/delvingDaphodilus.png'),
  ('Delving Daphodilus', 'WalkAnimBlue', 'animations/blue/delvingDaphodilus-walk.png'),
  ('Delving Daphodilus', 'WalkAnimRed', 'animations/red/delvingDaphodilus-walk.png'),
  ('Elias Tiberion', 'Token', 'characters/eliasTiberion.png'),
  ('Elias Tiberion', 'WalkAnim', 'animations/eliasTiberion-walk.png'),
  ('Elias Tiberion', 'PieceArtStyle', 'separateBase'),
  ('Elias Tiberion', 'PieceBaseBlue', 'characters/bases/piece-base-blue.png'),
  ('Elias Tiberion', 'PieceBaseRed', 'characters/bases/piece-base-red.png'),
  ('Gentle Bot', 'WalkAnimBlue', 'animations/blue/gentleBot-walk.png'),
  ('Gentle Bot', 'WalkAnimRed', 'animations/red/gentleBot-walk.png'),
  ('Gloom Fairy', 'Token', 'characters/gloomFairy.png'),
  ('Gloom Fairy', 'WalkAnim', 'animations/GT_animation_gloomFairy_idle.png'),
  ('Gloom Fairy', 'IdleAnim', 'animations/GT_animation_gloomFairy_idle.png'),
  ('Gloom Fairy', 'PieceArtStyle', 'separateBase'),
  ('Gloom Fairy', 'PieceBaseBlue', 'characters/bases/piece-base-blue.png'),
  ('Gloom Fairy', 'PieceBaseRed', 'characters/bases/piece-base-red.png'),
  ('Hired Gun', 'TokenBlue', 'characters/blue/hiredGun.png'),
  ('Hired Gun', 'TokenRed', 'characters/red/hiredGun.png'),
  ('Hired Gun', 'WalkAnimBlue', 'animations/blue/hiredGun-walk.png'),
  ('Hired Gun', 'WalkAnimRed', 'animations/red/hiredGun-walk.png'),
  ('Hop Bot', 'WalkAnimBlue', 'animations/blue/hopBot-walk.png'),
  ('Hop Bot', 'WalkAnimRed', 'animations/red/hopBot-walk.png'),
  ('Lady Worthington', 'TokenBlue', 'characters/blue/ladyWorthington.png'),
  ('Lady Worthington', 'TokenRed', 'characters/red/ladyWorthington.png'),
  ('Lady Worthington', 'WalkAnimBlue', 'animations/blue/ladyWorthington-walk.png'),
  ('Lady Worthington', 'WalkAnimRed', 'animations/red/ladyWorthington-walk.png'),
  ('Lesser Demon', 'TokenBlue', 'characters/blue/lesserDemon.png'),
  ('Lesser Demon', 'TokenRed', 'characters/red/lesserDemon.png'),
  ('Lesser Demon', 'WalkAnimBlue', 'animations/blue/lesserDemon-walk.png'),
  ('Lesser Demon', 'WalkAnimRed', 'animations/red/lesserDemon-walk.png'),
  ('Patrol Bot', 'TokenBlue', 'characters/blue/patrolBot.png'),
  ('Patrol Bot', 'TokenRed', 'characters/red/patrolBot.png'),
  ('Patrol Bot', 'WalkAnimBlue', 'animations/blue/patrolBot-walk.png'),
  ('Patrol Bot', 'WalkAnimRed', 'animations/red/patrolBot-walk.png'),
  ('Professor Glumpkin', 'TokenBlue', 'characters/blue/professorGlumpkin.png'),
  ('Professor Glumpkin', 'TokenRed', 'characters/red/professorGlumpkin.png'),
  ('Professor Glumpkin', 'WalkAnimRed', 'animations/red/professorGlumpkin-walk.jpg'),
  ('River Dweller', 'TokenBlue', 'characters/blue/riverDweller.png'),
  ('River Dweller', 'TokenRed', 'characters/red/riverDweller.png'),
  ('River Dweller', 'WalkAnimBlue', 'animations/blue/riverDweller-walk.png'),
  ('River Dweller', 'WalkAnimRed', 'animations/red/riverDweller-walk.png'),
  ('Rustbucket', 'TokenBlue', 'characters/blue/rustbucket.png'),
  ('Rustbucket', 'TokenRed', 'characters/red/rustbucket.png'),
  ('Rustbucket', 'WalkAnimBlue', 'animations/blue/rustbucket-walk.png'),
  ('Rustbucket', 'WalkAnimRed', 'animations/red/rustbucket-walk.png'),
  ('Scarlett Glumpkin', 'TokenBlue', 'characters/blue/scarlettGlumpkin.png'),
  ('Scarlett Glumpkin', 'TokenRed', 'characters/red/scarlettGlumpkin.png'),
  ('Scarlett Glumpkin', 'WalkAnimBlue', 'animations/blue/scarlettGlumpkin-walk.png'),
  ('Scarlett Glumpkin', 'WalkAnimRed', 'animations/red/scarlettGlumpkin-walk.png'),
  ('Sentroid', 'TokenBlue', 'characters/blue/sentroid.png'),
  ('Sentroid', 'TokenRed', 'characters/red/sentroid.png'),
  ('Sentroid', 'WalkAnimBlue', 'animations/blue/sentroid-walk.png'),
  ('Sentroid', 'WalkAnimRed', 'animations/red/sentroid-walk.png'),
  ('Stingy', 'TokenBlue', 'characters/blue/stingy.png'),
  ('Stingy', 'TokenRed', 'characters/red/stingy.png'),
  ('Stingy', 'WalkAnimBlue', 'animations/blue/stingy-walk.png'),
  ('Stingy', 'WalkAnimRed', 'animations/red/stingy-walk.png'),
  ('Sweetykins', 'TokenBlue', 'characters/blue/sweetykins.png'),
  ('Sweetykins', 'TokenRed', 'characters/red/sweetykins.png'),
  ('Sweetykins', 'WalkAnimBlue', 'animations/blue/sweetykins-walk.png'),
  ('Sweetykins', 'WalkAnimRed', 'animations/red/sweetykins-walk.png'),
  ('Telematron', 'TokenBlue', 'characters/blue/telematron.png'),
  ('Telematron', 'TokenRed', 'characters/red/telematron.png'),
  ('Telematron', 'WalkAnimBlue', 'animations/blue/telematron-walk.png'),
  ('Telematron', 'WalkAnimRed', 'animations/red/telematron-walk.png'),
  ('Tinkering Tom', 'TokenBlue', 'characters/blue/tinkeringTom.png'),
  ('Tinkering Tom', 'TokenRed', 'characters/red/tinkeringTom.png'),
  ('Tinkering Tom', 'WalkAnimBlue', 'animations/blue/tinkeringTom-walk.png'),
  ('Tinkering Tom', 'WalkAnimRed', 'animations/red/tinkeringTom-walk.png');

INSERT INTO card_integer_values (title, key, value) VALUES
  ('Automatick', 'WalkAnimFrames', 7),
  ('Bramble Drone', 'WalkAnimFrames', 8),
  ('Gentle Bot', 'WalkAnimFrames', 4),
  ('Gloom Fairy', 'WalkAnimFrames', 12),
  ('Gloom Fairy', 'IdleAnimFrames', 12),
  ('Hop Bot', 'WalkAnimFrames', 6),
  ('Patrol Bot', 'WalkAnimFrames', 6),
  ('Rustbucket', 'WalkAnimFrames', 5),
  ('Scarlett Glumpkin', 'WalkAnimFrames', 5),
  ('Stingy', 'WalkAnimFrames', 6),
  ('Sweetykins', 'WalkAnimFrames', 6),
  ('Telematron', 'WalkAnimFrames', 10),
  ('Tinkering Tom', 'WalkAnimFrames', 81);

-- ---- Spells -------------------------------------------------------------
INSERT INTO cards (title, type, image_path) VALUES ('Smoke Bomb', 'Spell', 'cards/smoke-bomb-v2.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Smoke Bomb', 'cost', 20), ('Smoke Bomb', 'power', 4);
INSERT INTO card_string_values (title, key, value) VALUES
  ('Smoke Bomb', 'effect', 'damage'), ('Smoke Bomb', 'target', 'enemy');

INSERT INTO cards (title, type, image_path) VALUES ('Cannon Blast', 'Spell', 'cards/smoke-bomb-v2.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Cannon Blast', 'cost', 40), ('Cannon Blast', 'power', 8);
INSERT INTO card_string_values (title, key, value) VALUES
  ('Cannon Blast', 'effect', 'damage'), ('Cannon Blast', 'target', 'enemy');

INSERT INTO cards (title, type, image_path) VALUES ('Repair Kit', 'Spell', 'cards/steam-baron.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Repair Kit', 'cost', 20), ('Repair Kit', 'power', 6);
INSERT INTO card_string_values (title, key, value) VALUES
  ('Repair Kit', 'effect', 'heal'), ('Repair Kit', 'target', 'ally');

INSERT INTO cards (title, type, image_path) VALUES ('Overpressure', 'Spell', 'cards/smoke-bomb-v2.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Overpressure', 'cost', 10), ('Overpressure', 'power', 3);
INSERT INTO card_string_values (title, key, value) VALUES
  ('Overpressure', 'effect', 'steam'), ('Overpressure', 'target', 'none');

-- ---- Rarity ------------------------------------------------------------
-- Shop odds: common 70%, rare 25%, legendary 5%.
-- After rarity is selected, each card in that rarity is equally likely.
INSERT INTO card_string_values (title, key, value) VALUES
  ('Bayou Scout', 'rarity', 'common'),
  ('Boiler Imp', 'rarity', 'common'),
  ('Brass Pawn', 'rarity', 'common'),
  ('Cog Tinker', 'rarity', 'common'),
  ('Gearwright', 'rarity', 'common'),
  ('Overpressure', 'rarity', 'common'),
  ('Repair Kit', 'rarity', 'common'),
  ('Rifleman', 'rarity', 'common'),
  ('Smoke Bomb', 'rarity', 'common'),
  ('Spark Drone', 'rarity', 'common'),
  ('Sprocket Swarm', 'rarity', 'common'),
  ('Swamp Skiff', 'rarity', 'common'),
  ('Arc Lantern', 'rarity', 'rare'),
  ('Automaton Knight', 'rarity', 'rare'),
  ('Brass Medic', 'rarity', 'rare'),
  ('Cannon Blast', 'rarity', 'rare'),
  ('Chain Harpoon', 'rarity', 'rare'),
  ('Gear Knight', 'rarity', 'rare'),
  ('Iron Sentinel', 'rarity', 'rare'),
  ('Marsh Witch', 'rarity', 'rare'),
  ('Mudslide', 'rarity', 'rare'),
  ('Steam Bishop', 'rarity', 'rare'),
  ('Clockwork Rook', 'rarity', 'legendary'),
  ('Dredger', 'rarity', 'legendary'),
  ('Railgunner', 'rarity', 'legendary'),
  ('Steam Baron', 'rarity', 'legendary');

INSERT INTO card_string_values (title, key, value) VALUES
  ('Automatick', 'rarity', 'common'),
  ('Bramble Drone', 'rarity', 'common'),
  ('Gentle Bot', 'rarity', 'common'),
  ('Hop Bot', 'rarity', 'common'),
  ('Patrol Bot', 'rarity', 'common'),
  ('Scarlett Glumpkin', 'rarity', 'common'),
  ('Sentroid', 'rarity', 'common'),
  ('Stingy', 'rarity', 'common'),
  ('Tinkering Tom', 'rarity', 'common'),
  ('Choking Blossom', 'rarity', 'rare'),
  ('Curious Spirit', 'rarity', 'rare'),
  ('Elias Tiberion', 'rarity', 'rare'),
  ('Hired Gun', 'rarity', 'rare'),
  ('Professor Glumpkin', 'rarity', 'rare'),
  ('River Dweller', 'rarity', 'rare'),
  ('Rustbucket', 'rarity', 'rare'),
  ('Sweetykins', 'rarity', 'rare'),
  ('Telematron', 'rarity', 'rare'),
  ('Delving Daphodilus', 'rarity', 'legendary'),
  ('Lady Worthington', 'rarity', 'legendary'),
  ('Lesser Demon', 'rarity', 'legendary');

-- Reusable action objects and ordered card references.
INSERT INTO actions (
  name, state, kind, pattern, min_range, max_range, damage,
  can_move, can_attack, pass_through, line_of_sight, status_turns, cooldown_turns
) VALUES
  ('KnightMove1', 0, 'slide', 'jump', 1, 2, 1, 1, 1, 0, 0, 0, 0),
  ('KingMove2', 0, 'slide', 'omni', 1, 1, 2, 1, 1, 0, 0, 0, 0),
  ('QueenHex1', 0, 'slide', 'omni', 1, 2, 1, 1, 1, 0, 0, 1, 0),
  ('BishopMove0', 0, 'slide', 'diag', 1, 7, 0, 1, 0, 0, 0, 0, 0),
  ('BishopMove2', 0, 'slide', 'diag', 1, 7, 2, 1, 1, 0, 0, 0, 0),
  ('TunnelMove0', 0, 'tunnel', 'none', 1, 7, 0, 1, 0, 0, 0, 0, 0),
  ('ShortRookMove0', 0, 'slide', 'ortho', 1, 2, 0, 1, 0, 0, 0, 0, 0),
  ('KingAttack3', 1, 'ranged', 'omni', 1, 1, 3, 0, 1, 0, 1, 0, 0),
  ('BishopStep1', 0, 'slide', 'diag', 1, 1, 1, 1, 1, 0, 0, 0, 0),
  ('KingMove0', 0, 'slide', 'omni', 1, 1, 0, 1, 0, 0, 0, 0, 0),
  ('LongRookMove0', 0, 'slide', 'ortho', 2, 4, 0, 1, 0, 0, 0, 0, 0),
  ('VaultMove1', 0, 'hop', 'none', 2, 2, 1, 1, 1, 0, 0, 0, 0),
  ('QueenMove0', 0, 'slide', 'omni', 1, 7, 0, 1, 0, 0, 0, 0, 0),
  ('GhostQueenMove0', 1, 'slide', 'omni', 1, 7, 0, 1, 0, 1, 0, 0, 0),
  ('RookMove2', 0, 'slide', 'ortho', 1, 4, 2, 1, 1, 0, 0, 0, 0),
  ('HorizontalMove0', 0, 'slide', 'horizontal', 1, 1, 0, 1, 0, 0, 0, 0, 0),
  ('BishopAttack2', 0, 'capture', 'diag', 1, 1, 2, 1, 1, 0, 0, 0, 0),
  ('KingMove1', 0, 'slide', 'omni', 1, 1, 1, 1, 1, 0, 0, 0, 0),
  ('ShortQueenAttack1', 0, 'ranged', 'omni', 1, 2, 1, 0, 1, 0, 0, 0, 1),
  ('QueenStun0', 0, 'slide', 'omni', 1, 2, 0, 1, 1, 0, 0, 2, 0),
  ('VerticalMove0', 0, 'slide', 'vertical', 1, 1, 0, 1, 0, 0, 0, 0, 0),
  ('BishopAttack1', 0, 'capture', 'diag', 1, 1, 1, 1, 1, 0, 0, 0, 0),
  ('BishopStep0', 0, 'slide', 'diag', 1, 1, 0, 1, 0, 0, 0, 0, 0),
  ('KingAttack1', 0, 'ranged', 'omni', 1, 1, 1, 0, 1, 0, 0, 0, 0),
  ('KingStrike1', 0, 'slide', 'omni', 1, 1, 1, 0, 1, 0, 0, 0, 0),
  ('BishopMove1', 0, 'slide', 'diag', 1, 7, 1, 1, 1, 0, 0, 0, 0),
  ('RookMove1', 0, 'slide', 'ortho', 1, 7, 1, 1, 1, 0, 0, 0, 0),
  ('TeleportMove0', 0, 'teleport', 'none', 1, 7, 0, 1, 0, 0, 0, 0, 0);

INSERT INTO card_actions (title, action_name, item_index) VALUES
  ('Automatick', 'KnightMove1', 0),
  ('Bramble Drone', 'KingMove2', 0),
  ('Choking Blossom', 'QueenHex1', 0),
  ('Curious Spirit', 'BishopMove0', 0),
  ('Delving Daphodilus', 'BishopMove2', 0),
  ('Delving Daphodilus', 'TunnelMove0', 1),
  ('Elias Tiberion', 'ShortRookMove0', 0),
  ('Elias Tiberion', 'KingAttack3', 1),
  ('Gentle Bot', 'KingMove0', 0),
  ('Hired Gun', 'LongRookMove0', 0),
  ('Hired Gun', 'KingAttack3', 1),
  ('Hop Bot', 'VaultMove1', 0),
  ('Lady Worthington', 'QueenMove0', 0),
  ('Lady Worthington', 'GhostQueenMove0', 1),
  ('Lesser Demon', 'RookMove2', 0),
  ('Patrol Bot', 'HorizontalMove0', 0),
  ('Patrol Bot', 'BishopAttack2', 1),
  ('Professor Glumpkin', 'KingMove1', 0),
  ('River Dweller', 'BishopMove0', 0),
  ('Rustbucket', 'KingMove0', 0),
  ('Rustbucket', 'ShortQueenAttack1', 1),
  ('Scarlett Glumpkin', 'QueenStun0', 0),
  ('Sentroid', 'VerticalMove0', 0),
  ('Sentroid', 'BishopAttack1', 1),
  ('Stingy', 'BishopMove1', 0),
  ('Sweetykins', 'RookMove1', 0),
  ('Telematron', 'TeleportMove0', 0),
  ('Telematron', 'KingStrike1', 1),
  ('Tinkering Tom', 'KingMove1', 0);

COMMIT;
