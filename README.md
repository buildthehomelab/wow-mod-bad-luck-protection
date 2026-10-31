# Bad Luck Protection

An [AzerothCore](https://www.azerothcore.org/) (WotLK 3.3.5a) module that gives every player a
**personal bonus roll** for legendaries and rare mounts, one that grows with every boss kill
that didn't drop them. The boss's own loot is untouched: a lucky first kill still happens. This
is for the twentieth.

| Drop | Boss | Stock chance | Streak |
| --- | --- | --- | --- |
| Bindings of the Windseeker (both) | Baron Geddon, Garr | 3% | character, done with Thunderfury |
| Eye of Sulfuras | Ragnaros | 3% | character, done with Sulfuras |
| Warglaives of Azzinoth (both) | Illidan Stormrage | 5% | character |
| Thori'dal, the Stars' Fury | Kil'jaeden | 6% | character |
| Talisman of Binding Shard and Fragment, Shard of the Defiler, Glaive of the Defender | Baron Geddon, Garr, Lord Kazzak, Sapphiron (40) | 3% | character; needs [mod-lost-legendaries](https://github.com/buildthehomelab/wow-mod-lost-legendaries) |
| Deathcharger's Reins | Baron Rivendare | 1% | account |
| Swift Zulian Tiger, Swift Razzashi Raptor | High Priest Thekal, Bloodlord Mandokir | 2% | account |
| Fiery Warhorse's Reins | Attumen the Huntsman | 1% | account |
| Ashes of Al'ar | Kael'thas Sunstrider | 2% | account |
| Reins of the Raven Lord | Anzu | 0.9% | account |
| Swift White Hawkstrider | Kael'thas Sunstrider (heroic Magisters' Terrace) | 5% | account |
| Reins of the Blue Proto-Drake | Skadi the Ruthless (heroic) | 1.5% | account |
| Reins of the Azure Drake | Malygos | 1% | account |
| Reins of the Onyxian Drake | Onyxia | 1-2% | account |
| Reins of the Grand Black War Mammoth | Archavon, Emalon, Koralon, Toravon | shared group roll | account |
| Foror's Fabled Steed | Nefarian | 1% | account; needs mod-lost-legendaries |
| All eight direhorn colours: Slate, Amber, Jade and Cobalt Primordial, Crimson and Golden Primal, Palehide and Zandalari Direhorns | Zandalari Warbringer (each drops the direhorn it rides) | 10% | account; needs mod-primal-bones |

Those are the module's rows. Protecting another drop is one row in a table: see
[Adding a drop](#adding-a-drop).

## How it works

- When one of those bosses dies to players, **every real player in the instance** (dead ones
  too) rolls once for each protected item it has that their class and race can use: no
  Warglaives for a mage, no Horde mammoth for the Alliance. For a boss out in the world, the
  group that tapped it rolls, those of it near enough for the core's own kill credit. Bots roll
  nothing.
- The roll is **`step` percent for every kill you have counted without the item**. Your first
  kill counts but gives no bonus (the stock roll is all there is). Your second kill rolls at
  10% for a legendary, 5% for a mount; your third at 20% and 10%; and so on, until the roll
  can't fail: the eleventh kill for a legendary, the twenty-first for a mount. With the stock
  drop still rolling on top, half of all players have their legendary by the fourth kill and
  their mount by the sixth.
- A win puts the item **straight into your bags**, or in your mailbox when they are full, and
  ends the streak. So does getting the item any other way: from the corpse, a roll, the master
  looter, or a game master. A mount's streak also ends when you learn the mount. On a kill where
  the boss's own loot holds the item, the kill counts but nobody bonus-rolls: one copy per kill.
- Nobody rolls for something they have: the item (bags, bank or mailbox), the mount it teaches,
  or what the item is made into (a Binding with Thunderfury in the bank).
- **A kill counts once per lockout** for a raid boss, and **once per day** for a dungeon or
  world boss (until the next daily quest reset), so resetting Stratholme twenty times tonight
  ramps nothing. Each boss counts on its own: the four Vault of Archavon bosses are four counts
  towards the mammoth. `BadLuck.CountEveryKill` turns the limit off.
- Legendaries are the **character's** streak. Mounts are the **account's**: every character on
  the account counts towards the same one, and the one that wins ends it for all. That fits
  account-wide mounts; without them, set `scope` to 0 in the rule rows. With mod-playerbots,
  your own alt bots are characters of your account too: one of them looting the mount ends the
  account's streak.
- After each counted kill you are told where you stand. `.luck` lists your running streaks,
  `.luck all` every protected drop and where you stand on each.

## Adding a drop

A rule is a row of world table `mod_bad_luck_rule`:

| Column | |
| --- | --- |
| `item` | the drop |
| `creature` | `creature_template` entry of the boss. The 10 and 25 player versions of a boss are different entries: one row each |
| `kind` | `0` legendary, `1` mount: which default step applies |
| `scope` | `0` the character's streak, `1` the account's |
| `step` | percent per counted kill; `0` takes the kind's default from the config |
| `complete_item` | owning this item also ends the streak, for a drop that is made into something else; `0` for none |
| `comment` | for people |

The boss must drop the item itself for the stock roll to exist, but the module doesn't
check: a rule for an item the boss never drops is a bonus roll and nothing else, which is one way
to put a new mount on a boss. `.badluck reload` reads the table again without a restart.

Mammoth note: the Vault of Archavon bosses roll one mount for the raid out of an
equal-chance group, so the stock chance isn't a fixed percent. The rule gives each faction's
mount its own streak.

## Commands

| Command | Who | |
| --- | --- | --- |
| `.luck` | everyone | your running streaks |
| `.luck all` | everyone | every protected drop, with your kills and next chance, or that it is yours |
| `.badluck set <item> <kills> [player]` | game master | puts the player's (or your target's, or your own) streak for the item at that many counted kills. `0` ends it |
| `.badluck reset [player]` | game master | ends every streak of the character and of its account |
| `.badluck reload` | administrator | reads `mod_bad_luck_rule` again |

## Requirements

- AzerothCore 3.3.5a. Works with and without mod-playerbots (bots never roll).
- Rules for another module's drops (mod-lost-legendaries, mod-primal-bones,
  mod-individual-progression's 40 player Naxxramas) are harmless without that module: the boss
  or item isn't there, so nothing happens.

## Install

```bash
cd azerothcore-wotlk/modules
git clone https://github.com/buildthehomelab/wow-mod-bad-luck-protection.git mod-bad-luck-protection
```

Clone into `mod-bad-luck-protection`, without the `wow-`: AzerothCore names the module's loader
after the folder. Re-run CMake, build, and copy `conf/mod_bad_luck_protection.conf.dist` to your
config folder as `mod_bad_luck_protection.conf`. The world and characters database changes apply
themselves at the next start. No client patch, no addon.

## Configuration

`mod_bad_luck_protection.conf`:

| Setting | Default | |
| --- | --- | --- |
| `BadLuck.Enable` | `1` | With `0` no kill counts and nobody rolls. Streaks stay where they are |
| `BadLuck.Step.Legendary` | `10` | Percent per counted kill for rules of kind 0 without a step of their own |
| `BadLuck.Step.Mount` | `5` | The same for kind 1 |
| `BadLuck.CountEveryKill` | `0` | `1`: every kill counts, not one per lockout or day |
| `BadLuck.Announce` | `1` | `0` silent, `1` tell the player after each counted kill and on a win, `2` tell their group of a win too |

## What it adds to the databases

- World: table `mod_bad_luck_rule` and its rows. Nothing else changes; no loot rows are touched.
- Characters: table `mod_bad_luck_streak`, a row per owner and item with a streak.

No new items, spells, creatures or quests.

## Uninstalling

Remove the module and rebuild. Then run `data/sql/uninstall/mod_bad_luck_protection_uninstall_world.sql`
on the world database and `..._characters.sql` on the characters database by hand, with the
server stopped. Items that bonus rolls gave are ordinary items and stay.

## Troubleshooting

- **A boss died and nobody was told anything.** The boss has no rule (`.luck all` lists the
  protected drops and their bosses), the kill didn't count because this lockout or day already
  did (`.luck` shows the count), everyone already has the item, or the boss wasn't killed by
  players: something else in the room, or `.die` with `DieCommand.Mode = 1` (with mode 0 the
  core treats `.die` as the game master's damage and gives loot, so the kill counts here too).
  A game master in GM mode (`.gm on`) never counts: switch it off to test.
- **"mod_bad_luck_rule names item N, which doesn't exist".** A rule row for an item from a
  module this server doesn't have. Harmless; delete the row to quiet it.
- **A bot got nothing.** On purpose.
- **Someone keeps a streak for an item they own.** They own it on another character: a
  per-character streak is per character. `.badluck reset` ends it.

## Patch Notes: Bad Luck Protection

Category: Dungeons & Raids

- The rarest drops now have **bad luck protection**. Every time you kill a boss that doesn't
  drop its legendary or its mount for you, your next kill gets a personal bonus roll: 10% more
  per kill for legendaries, 5% more for mounts, until it can't miss. The boss's own loot still
  rolls as before, so a lucky first kill is still a thing.
- It covers the **Bindings of the Windseeker**, the **Eye of Sulfuras**, the **Warglaives of
  Azzinoth**, **Thori'dal**, and the lost legendaries; and the **Deathcharger**, **Swift Zulian
  Tiger**, **Swift Razzashi Raptor**, **Fiery Warhorse**, **Ashes of Al'ar**, **Raven Lord**,
  **Swift White Hawkstrider**, **Blue Proto-Drake**, **Azure Drake**, **Onyxian Drake**,
  **Grand Black War Mammoth** and **Foror's Fabled Steed**.
- A kill counts once per raid lockout, and once a day for a dungeon or world boss. Mount
  streaks are shared by every character on your account. Type `.luck` to see where you stand.

> Nobody should run Stratholme a hundred times for a horse. Every kill now brings it closer,
> and one day it just comes.

## Credits

Author: [buildthehomelab](https://github.com/buildthehomelab)

## License

[MIT](LICENSE)
