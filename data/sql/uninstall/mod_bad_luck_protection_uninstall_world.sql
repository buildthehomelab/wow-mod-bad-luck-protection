-- mod-bad-luck-protection: undo the module's world database changes. Run it by hand on the world
-- database after removing the module. It isn't applied automatically: AzerothCore only runs the
-- module's db-world and db-characters folders.
--
-- Drops the rule table (your own rows included) and forgets that the module's update was
-- applied, so installing the module again applies it again. Idempotent: safe to run again.

DROP TABLE IF EXISTS `mod_bad_luck_rule`;

DELETE FROM `updates` WHERE `name` LIKE 'mod\_bad\_luck\_protection\_%';
