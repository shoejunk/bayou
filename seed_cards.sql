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

BEGIN TRANSACTION;

DELETE FROM card_string_lists;
DELETE FROM card_string_values;
DELETE FROM card_integer_values;
DELETE FROM card_keywords;
DELETE FROM cards;

-- ---- Heroes -------------------------------------------------------------
INSERT INTO cards (title, type, image_path) VALUES ('Steam Baron', 'Hero', 'cards/steam-baron.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Steam Baron', 'heroCost', 10), ('Steam Baron', 'health', 30),
  ('Steam Baron', 'attack', 8), ('Steam Baron', 'range', 1), ('Steam Baron', 'move', 1);
INSERT INTO card_string_values (title, key, value) VALUES ('Steam Baron', 'movement', 'omni');
INSERT INTO card_string_values (title, key, value) VALUES ('Steam Baron', 'WalkAnim', 'animations/steam-baron-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Iron Sentinel', 'Hero', 'cards/clockwork-rook.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Iron Sentinel', 'heroCost', 6), ('Iron Sentinel', 'health', 26),
  ('Iron Sentinel', 'attack', 6), ('Iron Sentinel', 'range', 1), ('Iron Sentinel', 'move', 2);
INSERT INTO card_string_values (title, key, value) VALUES ('Iron Sentinel', 'movement', 'ortho');
INSERT INTO card_string_values (title, key, value) VALUES ('Iron Sentinel', 'WalkAnim', 'animations/clockwork-rook-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Marsh Witch', 'Hero', 'cards/marsh-witch.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Marsh Witch', 'heroCost', 5), ('Marsh Witch', 'health', 16),
  ('Marsh Witch', 'attack', 6), ('Marsh Witch', 'range', 3), ('Marsh Witch', 'move', 2);
INSERT INTO card_string_values (title, key, value) VALUES ('Marsh Witch', 'movement', 'diag');
INSERT INTO card_string_values (title, key, value) VALUES ('Marsh Witch', 'WalkAnim', 'animations/marsh-witch-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Gear Knight', 'Hero', 'cards/clockwork-rook.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Gear Knight', 'heroCost', 4), ('Gear Knight', 'health', 18),
  ('Gear Knight', 'attack', 6), ('Gear Knight', 'range', 1), ('Gear Knight', 'move', 1);
INSERT INTO card_string_values (title, key, value) VALUES ('Gear Knight', 'movement', 'jump');
INSERT INTO card_string_values (title, key, value) VALUES ('Gear Knight', 'WalkAnim', 'animations/clockwork-rook-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Bayou Scout', 'Hero', 'cards/marsh-witch.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Bayou Scout', 'heroCost', 3), ('Bayou Scout', 'health', 12),
  ('Bayou Scout', 'attack', 4), ('Bayou Scout', 'range', 2), ('Bayou Scout', 'move', 3);
INSERT INTO card_string_values (title, key, value) VALUES ('Bayou Scout', 'movement', 'omni');
INSERT INTO card_string_values (title, key, value) VALUES ('Bayou Scout', 'WalkAnim', 'animations/marsh-witch-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Cog Tinker', 'Hero', 'cards/steam-baron.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Cog Tinker', 'heroCost', 2), ('Cog Tinker', 'health', 9),
  ('Cog Tinker', 'attack', 3), ('Cog Tinker', 'range', 1), ('Cog Tinker', 'move', 1);
INSERT INTO card_string_values (title, key, value) VALUES ('Cog Tinker', 'movement', 'omni');
INSERT INTO card_string_values (title, key, value) VALUES ('Cog Tinker', 'WalkAnim', 'animations/steam-baron-walk.png');

-- ---- Units --------------------------------------------------------------
INSERT INTO cards (title, type, image_path) VALUES ('Brass Pawn', 'Unit', 'cards/clockwork-rook.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Brass Pawn', 'cost', 1), ('Brass Pawn', 'health', 4),
  ('Brass Pawn', 'attack', 2), ('Brass Pawn', 'range', 1), ('Brass Pawn', 'move', 1);
INSERT INTO card_string_values (title, key, value) VALUES ('Brass Pawn', 'movement', 'ortho');
INSERT INTO card_string_values (title, key, value) VALUES ('Brass Pawn', 'WalkAnim', 'animations/clockwork-rook-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Rifleman', 'Unit', 'cards/clockwork-rook.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Rifleman', 'cost', 3), ('Rifleman', 'health', 6),
  ('Rifleman', 'attack', 4), ('Rifleman', 'range', 3), ('Rifleman', 'move', 1);
INSERT INTO card_string_values (title, key, value) VALUES ('Rifleman', 'movement', 'ortho');
INSERT INTO card_string_values (title, key, value) VALUES ('Rifleman', 'WalkAnim', 'animations/clockwork-rook-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Clockwork Rook', 'Unit', 'cards/clockwork-rook.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Clockwork Rook', 'cost', 4), ('Clockwork Rook', 'health', 12),
  ('Clockwork Rook', 'attack', 5), ('Clockwork Rook', 'range', 1), ('Clockwork Rook', 'move', 7);
INSERT INTO card_string_values (title, key, value) VALUES ('Clockwork Rook', 'movement', 'ortho');
INSERT INTO card_string_values (title, key, value) VALUES ('Clockwork Rook', 'WalkAnim', 'animations/clockwork-rook-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Steam Bishop', 'Unit', 'cards/clockwork-rook.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Steam Bishop', 'cost', 4), ('Steam Bishop', 'health', 9),
  ('Steam Bishop', 'attack', 5), ('Steam Bishop', 'range', 1), ('Steam Bishop', 'move', 7);
INSERT INTO card_string_values (title, key, value) VALUES ('Steam Bishop', 'movement', 'diag');
INSERT INTO card_string_values (title, key, value) VALUES ('Steam Bishop', 'WalkAnim', 'animations/clockwork-rook-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Automaton Knight', 'Unit', 'cards/clockwork-rook.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Automaton Knight', 'cost', 3), ('Automaton Knight', 'health', 9),
  ('Automaton Knight', 'attack', 4), ('Automaton Knight', 'range', 1), ('Automaton Knight', 'move', 1);
INSERT INTO card_string_values (title, key, value) VALUES ('Automaton Knight', 'movement', 'jump');
INSERT INTO card_string_values (title, key, value) VALUES ('Automaton Knight', 'WalkAnim', 'animations/clockwork-rook-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Dredger', 'Unit', 'cards/clockwork-rook.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Dredger', 'cost', 5), ('Dredger', 'health', 16),
  ('Dredger', 'attack', 6), ('Dredger', 'range', 1), ('Dredger', 'move', 1);
INSERT INTO card_string_values (title, key, value) VALUES ('Dredger', 'movement', 'omni');
INSERT INTO card_string_values (title, key, value) VALUES ('Dredger', 'WalkAnim', 'animations/clockwork-rook-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Spark Drone', 'Unit', 'cards/smoke-bomb-v2.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Spark Drone', 'cost', 2), ('Spark Drone', 'health', 4),
  ('Spark Drone', 'attack', 3), ('Spark Drone', 'range', 2), ('Spark Drone', 'move', 2);
INSERT INTO card_string_values (title, key, value) VALUES ('Spark Drone', 'movement', 'omni');
INSERT INTO card_string_values (title, key, value) VALUES ('Spark Drone', 'WalkAnim', 'animations/clockwork-rook-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Gearwright', 'Unit', 'cards/tinkeringTom.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Gearwright', 'cost', 2), ('Gearwright', 'health', 5),
  ('Gearwright', 'attack', 2), ('Gearwright', 'range', 1), ('Gearwright', 'move', 2);
INSERT INTO card_string_values (title, key, value) VALUES ('Gearwright', 'movement', 'omni');
INSERT INTO card_string_values (title, key, value) VALUES ('Gearwright', 'WalkAnim', 'animations/steam-baron-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Brass Medic', 'Unit', 'cards/steam-baron.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Brass Medic', 'cost', 3), ('Brass Medic', 'health', 7),
  ('Brass Medic', 'attack', 2), ('Brass Medic', 'range', 1), ('Brass Medic', 'move', 2);
INSERT INTO card_string_values (title, key, value) VALUES ('Brass Medic', 'movement', 'diag');
INSERT INTO card_string_values (title, key, value) VALUES ('Brass Medic', 'WalkAnim', 'animations/steam-baron-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Boiler Imp', 'Unit', 'cards/smoke-bomb-v2.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Boiler Imp', 'cost', 1), ('Boiler Imp', 'health', 3),
  ('Boiler Imp', 'attack', 3), ('Boiler Imp', 'range', 1), ('Boiler Imp', 'move', 2);
INSERT INTO card_string_values (title, key, value) VALUES ('Boiler Imp', 'movement', 'omni');
INSERT INTO card_string_values (title, key, value) VALUES ('Boiler Imp', 'WalkAnim', 'animations/clockwork-rook-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Railgunner', 'Unit', 'cards/clockwork-rook.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Railgunner', 'cost', 5), ('Railgunner', 'health', 7),
  ('Railgunner', 'attack', 7), ('Railgunner', 'range', 4), ('Railgunner', 'move', 1);
INSERT INTO card_string_values (title, key, value) VALUES ('Railgunner', 'movement', 'ortho');
INSERT INTO card_string_values (title, key, value) VALUES ('Railgunner', 'WalkAnim', 'animations/clockwork-rook-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Swamp Skiff', 'Unit', 'cards/marsh-witch.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Swamp Skiff', 'cost', 2), ('Swamp Skiff', 'health', 6),
  ('Swamp Skiff', 'attack', 2), ('Swamp Skiff', 'range', 1), ('Swamp Skiff', 'move', 3);
INSERT INTO card_string_values (title, key, value) VALUES ('Swamp Skiff', 'movement', 'ortho');
INSERT INTO card_string_values (title, key, value) VALUES ('Swamp Skiff', 'WalkAnim', 'animations/marsh-witch-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Arc Lantern', 'Unit', 'cards/smoke-bomb-v2.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Arc Lantern', 'cost', 3), ('Arc Lantern', 'health', 5),
  ('Arc Lantern', 'attack', 4), ('Arc Lantern', 'range', 2), ('Arc Lantern', 'move', 1);
INSERT INTO card_string_values (title, key, value) VALUES ('Arc Lantern', 'movement', 'diag');
INSERT INTO card_string_values (title, key, value) VALUES ('Arc Lantern', 'WalkAnim', 'animations/clockwork-rook-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Sprocket Swarm', 'Unit', 'cards/clockwork-rook.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Sprocket Swarm', 'cost', 2), ('Sprocket Swarm', 'health', 5),
  ('Sprocket Swarm', 'attack', 3), ('Sprocket Swarm', 'range', 1), ('Sprocket Swarm', 'move', 2);
INSERT INTO card_string_values (title, key, value) VALUES ('Sprocket Swarm', 'movement', 'jump');
INSERT INTO card_string_values (title, key, value) VALUES ('Sprocket Swarm', 'WalkAnim', 'animations/clockwork-rook-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Chain Harpoon', 'Unit', 'cards/clockwork-rook.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Chain Harpoon', 'cost', 4), ('Chain Harpoon', 'health', 8),
  ('Chain Harpoon', 'attack', 5), ('Chain Harpoon', 'range', 2), ('Chain Harpoon', 'move', 1);
INSERT INTO card_string_values (title, key, value) VALUES ('Chain Harpoon', 'movement', 'ortho');
INSERT INTO card_string_values (title, key, value) VALUES ('Chain Harpoon', 'WalkAnim', 'animations/clockwork-rook-walk.png');

INSERT INTO cards (title, type, image_path) VALUES ('Mudslide', 'Unit', 'cards/marsh-witch.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Mudslide', 'cost', 4), ('Mudslide', 'health', 11),
  ('Mudslide', 'attack', 4), ('Mudslide', 'range', 1), ('Mudslide', 'move', 2);
INSERT INTO card_string_values (title, key, value) VALUES ('Mudslide', 'movement', 'diag');
INSERT INTO card_string_values (title, key, value) VALUES ('Mudslide', 'WalkAnim', 'animations/marsh-witch-walk.png');

-- ---- Spells -------------------------------------------------------------
INSERT INTO cards (title, type, image_path) VALUES ('Smoke Bomb', 'Spell', 'cards/smoke-bomb-v2.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Smoke Bomb', 'cost', 2), ('Smoke Bomb', 'power', 4);
INSERT INTO card_string_values (title, key, value) VALUES
  ('Smoke Bomb', 'effect', 'damage'), ('Smoke Bomb', 'target', 'enemy');

INSERT INTO cards (title, type, image_path) VALUES ('Cannon Blast', 'Spell', 'cards/smoke-bomb-v2.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Cannon Blast', 'cost', 4), ('Cannon Blast', 'power', 8);
INSERT INTO card_string_values (title, key, value) VALUES
  ('Cannon Blast', 'effect', 'damage'), ('Cannon Blast', 'target', 'enemy');

INSERT INTO cards (title, type, image_path) VALUES ('Repair Kit', 'Spell', 'cards/steam-baron.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Repair Kit', 'cost', 2), ('Repair Kit', 'power', 6);
INSERT INTO card_string_values (title, key, value) VALUES
  ('Repair Kit', 'effect', 'heal'), ('Repair Kit', 'target', 'ally');

INSERT INTO cards (title, type, image_path) VALUES ('Overpressure', 'Spell', 'cards/smoke-bomb-v2.png');
INSERT INTO card_integer_values (title, key, value) VALUES
  ('Overpressure', 'cost', 1), ('Overpressure', 'power', 3);
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

COMMIT;
