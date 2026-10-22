-- mod-bad-luck-protection: undo the module's characters database changes. Run it by hand on the
-- characters database after removing the module, with the server stopped.
--
-- Drops everyone's streaks. Items the bonus rolls gave are ordinary items and stay. Idempotent:
-- safe to run again.

DROP TABLE IF EXISTS `mod_bad_luck_streak`;

DELETE FROM `updates` WHERE `name` LIKE 'mod\_bad\_luck\_protection\_%';
