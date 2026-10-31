-- mod-bad-luck-protection: which drops are protected, and from which boss.
--
-- One row per item and boss creature. When that creature dies, every real player who was there
-- gets a personal bonus roll for the item: `step` percent for every kill they have already
-- counted without it. The stock loot roll is untouched; this is on top of it.
--
--   item           the drop
--   creature       creature_template entry of the boss (one row per entry: the 10 and 25 player
--                  versions of a boss are different entries)
--   kind           0 legendary, 1 mount: which default step from the config applies
--   scope          0 the streak is the character's, 1 it is the account's (alts share it)
--   step           percent added per counted kill; 0 takes the kind's default
--                  (BadLuck.Step.Legendary / BadLuck.Step.Mount)
--   complete_item  owning this item also ends the streak and stops the rolls: what the drop is
--                  made into (Thunderfury for a Binding), 0 for nothing
--
-- Add your own rows for other drops. Outside raids keep one boss per item: a streak remembers
-- only the last boss that counted, so two dungeon bosses for one item could count the first
-- again after a reset on the same day. Idempotent: the module's rows are replaced, yours stay.

CREATE TABLE IF NOT EXISTS `mod_bad_luck_rule` (
  `item` INT UNSIGNED NOT NULL COMMENT 'the drop',
  `creature` INT UNSIGNED NOT NULL COMMENT 'creature_template entry of the boss',
  `kind` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 legendary, 1 mount: picks the default step',
  `scope` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0 per character, 1 per account',
  `step` FLOAT NOT NULL DEFAULT 0 COMMENT 'percent per counted kill, 0 = the kind''s default from the config',
  `complete_item` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'owning this item also ends the streak, 0 for none',
  `comment` VARCHAR(160) NOT NULL DEFAULT '',
  PRIMARY KEY (`item`, `creature`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='mod-bad-luck-protection: protected drops per boss';

DELETE FROM `mod_bad_luck_rule` WHERE (`item`, `creature`) IN (
  (18563, 12056), (18564, 12057), (17204, 11502), (17782, 12056), (17783, 12057), (17142, 12397),
  (23051, 351018), (32837, 22917), (32838, 22917), (34334, 25315),
  (13335, 10440), (19902, 14509), (19872, 11382), (30480, 16152), (32458, 19622), (32768, 23035),
  (35513, 24857), (44151, 30807), (43952, 28859), (49636, 10184), (49636, 36538), (20221, 11583),
  (43959, 31125), (43959, 31722), (43959, 33993), (43959, 33994), (43959, 35360), (43959, 38433), (43959, 38462),
  (44083, 31125), (44083, 31722), (44083, 33993), (44083, 33994), (44083, 35360), (44083, 38433), (44083, 38462),
  (400533, 9506001), (401038, 9506005), (400418, 9506006), (401180, 9506007), (400545, 9506008),
  (400535, 9506002), (400534, 9506003), (400536, 9506004), (400546, 9506009), (401179, 9506010),
  (400533, 9506011), (401038, 9506012), (400545, 9506013));

-- Legendaries: the character's own streak.
INSERT INTO `mod_bad_luck_rule` (`item`, `creature`, `kind`, `scope`, `step`, `complete_item`, `comment`) VALUES
(18563, 12056, 0, 0, 0, 19019, 'Bindings of the Windseeker (left) - Baron Geddon; done with Thunderfury'),
(18564, 12057, 0, 0, 0, 19019, 'Bindings of the Windseeker (right) - Garr; done with Thunderfury'),
(17204, 11502, 0, 0, 0, 17182, 'Eye of Sulfuras - Ragnaros; done with Sulfuras'),
(17782, 12056, 0, 0, 0, 0,     'Talisman of Binding Shard - Baron Geddon (mod-lost-legendaries)'),
(17783, 12057, 0, 0, 0, 0,     'Talisman of Binding Fragment - Garr (mod-lost-legendaries)'),
(17142, 12397, 0, 0, 0, 0,     'Shard of the Defiler - Lord Kazzak, Blasted Lands (mod-lost-legendaries)'),
(23051, 351018, 0, 0, 0, 0,    'Glaive of the Defender - Sapphiron, 40 player Naxxramas (mod-lost-legendaries + mod-individual-progression)'),
(32837, 22917, 0, 0, 0, 0,     'Warglaive of Azzinoth (main hand) - Illidan Stormrage'),
(32838, 22917, 0, 0, 0, 0,     'Warglaive of Azzinoth (off hand) - Illidan Stormrage'),
(34334, 25315, 0, 0, 0, 0,     'Thori''dal, the Stars'' Fury - Kil''jaeden');

-- Mounts: the account's streak, every character on it counts towards the same one.
INSERT INTO `mod_bad_luck_rule` (`item`, `creature`, `kind`, `scope`, `step`, `complete_item`, `comment`) VALUES
(13335, 10440, 1, 1, 0, 0, 'Deathcharger''s Reins - Baron Rivendare, Stratholme'),
(19902, 14509, 1, 1, 0, 0, 'Swift Zulian Tiger - High Priest Thekal, Zul''Gurub'),
(19872, 11382, 1, 1, 0, 0, 'Swift Razzashi Raptor - Bloodlord Mandokir, Zul''Gurub'),
(30480, 16152, 1, 1, 0, 0, 'Fiery Warhorse''s Reins - Attumen the Huntsman, Karazhan'),
(32458, 19622, 1, 1, 0, 0, 'Ashes of Al''ar - Kael''thas Sunstrider, Tempest Keep'),
(32768, 23035, 1, 1, 0, 0, 'Reins of the Raven Lord - Anzu, Sethekk Halls'),
(35513, 24857, 1, 1, 0, 0, 'Swift White Hawkstrider - Kael''thas Sunstrider, heroic Magisters'' Terrace'),
(44151, 30807, 1, 1, 0, 0, 'Reins of the Blue Proto-Drake - Skadi the Ruthless, heroic Utgarde Pinnacle'),
(43952, 28859, 1, 1, 0, 0, 'Reins of the Azure Drake - Malygos'),
(49636, 10184, 1, 1, 0, 0, 'Reins of the Onyxian Drake - Onyxia (10 player)'),
(49636, 36538, 1, 1, 0, 0, 'Reins of the Onyxian Drake - Onyxia (25 player)'),
(20221, 11583, 1, 1, 0, 0, 'Foror''s Fabled Steed - Nefarian (mod-lost-legendaries)'),
(43959, 31125, 1, 1, 0, 0, 'Reins of the Grand Black War Mammoth (Alliance) - Archavon (10 player)'),
(43959, 31722, 1, 1, 0, 0, 'Reins of the Grand Black War Mammoth (Alliance) - Archavon (25 player)'),
(43959, 33993, 1, 1, 0, 0, 'Reins of the Grand Black War Mammoth (Alliance) - Emalon (10 player)'),
(43959, 33994, 1, 1, 0, 0, 'Reins of the Grand Black War Mammoth (Alliance) - Emalon (25 player)'),
(43959, 35360, 1, 1, 0, 0, 'Reins of the Grand Black War Mammoth (Alliance) - Koralon (25 player)'),
(43959, 38433, 1, 1, 0, 0, 'Reins of the Grand Black War Mammoth (Alliance) - Toravon (10 player)'),
(43959, 38462, 1, 1, 0, 0, 'Reins of the Grand Black War Mammoth (Alliance) - Toravon (25 player)'),
(44083, 31125, 1, 1, 0, 0, 'Reins of the Grand Black War Mammoth (Horde) - Archavon (10 player)'),
(44083, 31722, 1, 1, 0, 0, 'Reins of the Grand Black War Mammoth (Horde) - Archavon (25 player)'),
(44083, 33993, 1, 1, 0, 0, 'Reins of the Grand Black War Mammoth (Horde) - Emalon (10 player)'),
(44083, 33994, 1, 1, 0, 0, 'Reins of the Grand Black War Mammoth (Horde) - Emalon (25 player)'),
(44083, 35360, 1, 1, 0, 0, 'Reins of the Grand Black War Mammoth (Horde) - Koralon (25 player)'),
(44083, 38433, 1, 1, 0, 0, 'Reins of the Grand Black War Mammoth (Horde) - Toravon (10 player)'),
(44083, 38462, 1, 1, 0, 0, 'Reins of the Grand Black War Mammoth (Horde) - Toravon (25 player)');

-- Un'Goro rare elites from mod-primal-bones, each dropping its own mount at 10%: the five
-- Tarborn Tyrant skins and the eight Zandalari Warbringer direhorn colours. Open-world rares, so
-- a kill counts once a day. Cobalt, Zandalari and Golden drop from a Tyrant and a Warbringer: both
-- kills feed the same streak (streaks are per item).
INSERT INTO `mod_bad_luck_rule` (`item`, `creature`, `kind`, `scope`, `step`, `complete_item`, `comment`) VALUES
(400533, 9506001, 1, 1, 0, 0, 'Cobalt Primordial Direhorn - Tarborn Tyrant (dark), Un''Goro (mod-primal-bones)'),
(401038, 9506005, 1, 1, 0, 0, 'Zandalari Direhorn - Tarborn Tyrant (blue), Un''Goro (mod-primal-bones)'),
(400418, 9506006, 1, 1, 0, 0, 'Savage Raptor - Tarborn Tyrant (green), Un''Goro (mod-primal-bones)'),
(401180, 9506007, 1, 1, 0, 0, 'Swift Albino Raptor - Tarborn Tyrant (pale), Un''Goro (mod-primal-bones)'),
(400545, 9506008, 1, 1, 0, 0, 'Golden Primal Direhorn - Tarborn Tyrant (brown), Un''Goro (mod-primal-bones)'),
(400535, 9506002, 1, 1, 0, 0, 'Slate Primordial Direhorn - Zandalari Warbringer, Un''Goro (mod-primal-bones)'),
(400534, 9506003, 1, 1, 0, 0, 'Amber Primordial Direhorn - Zandalari Warbringer, Un''Goro (mod-primal-bones)'),
(400536, 9506004, 1, 1, 0, 0, 'Jade Primordial Direhorn - Zandalari Warbringer, Un''Goro (mod-primal-bones)'),
(400546, 9506009, 1, 1, 0, 0, 'Crimson Primal Direhorn - Zandalari Warbringer, Un''Goro (mod-primal-bones)'),
(401179, 9506010, 1, 1, 0, 0, 'Palehide Direhorn - Zandalari Warbringer, Un''Goro (mod-primal-bones)'),
(400533, 9506011, 1, 1, 0, 0, 'Cobalt Primordial Direhorn - Zandalari Warbringer, Un''Goro (mod-primal-bones)'),
(401038, 9506012, 1, 1, 0, 0, 'Zandalari Direhorn - Zandalari Warbringer, Un''Goro (mod-primal-bones)'),
(400545, 9506013, 1, 1, 0, 0, 'Golden Primal Direhorn - Zandalari Warbringer, Un''Goro (mod-primal-bones)');
