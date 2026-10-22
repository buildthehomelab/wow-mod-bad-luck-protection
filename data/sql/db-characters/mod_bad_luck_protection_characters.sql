-- mod-bad-luck-protection: everyone's streaks.
--
-- A row per owner and protected item: how many counted kills they have made without the item
-- (`fails`), and the period of the last kill that counted (`period_end`, `instance`: the raid's
-- next reset and the raid instance id, or the next daily quest reset and 0 for a dungeon or a
-- world boss), so a second kill in the same lockout or day doesn't count again. `owner` is the
-- character guid for a per-character streak (scope 0) and the account id for a per-account one
-- (scope 1). A row goes when the owner gets the item. The server keeps this in memory; the
-- table is its copy on disk, read at startup. Idempotent.

CREATE TABLE IF NOT EXISTS `mod_bad_luck_streak` (
  `owner` INT UNSIGNED NOT NULL COMMENT 'character guid (scope 0) or account id (scope 1)',
  `scope` TINYINT UNSIGNED NOT NULL COMMENT '0 character, 1 account',
  `item` INT UNSIGNED NOT NULL COMMENT 'the protected drop',
  `fails` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'counted kills without the item',
  `period_end` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'unix time the lockout or day of the last counted kill ends',
  `instance` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'raid instance id of the last counted kill, 0 otherwise',
  PRIMARY KEY (`owner`, `scope`, `item`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='mod-bad-luck-protection: kills without the drop, per owner and item';
