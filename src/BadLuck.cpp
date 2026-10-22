/*
 * mod-bad-luck-protection: a personal bonus roll for legendaries and rare mounts.
 *
 * The stock loot roll of a boss happens once per kill, for the group as a whole, and a 1% or 3%
 * roll can fail for a very long time. This module adds a roll of each player's own on top of it:
 *
 *   what            world table mod_bad_luck_rule: an item and the boss creature it protects
 *                   against, with how much the chance grows per kill
 *   when            the creature dies to players (the core's own test for loot and credit)
 *   who             every real player in the instance, dead or alive; for a boss out in the
 *                   world, the loot recipient's group within BadLuck.WorldBossRange yards. Bots
 *                   are sessions without a client and get nothing. Nor does a player who already
 *                   has the item, the mount it teaches, or what the item is made into.
 *   how much        step * kills counted so far without the item. The first kill counts (stock
 *                   roll only), the second gives `step` percent, and 100 / step kills later the
 *                   roll can't fail. The item goes into the player's bags, or by mail when they
 *                   are full. A win ends the streak; so does getting the item any other way.
 *   how often       a kill counts once per character per boss and period: a raid's lockout, a
 *                   dungeon's or world boss's day (the next daily quest reset). A raid boss in
 *                   another instance within one lockout counts again, which only happens when a
 *                   game master takes the character off the first one. BadLuck.CountEveryKill
 *                   makes every kill count.
 *   whose streak    the character's (legendaries), or the account's (mounts), as the rule says
 *
 * The streaks are kept in memory, under a lock because bosses die on map threads, and in
 * characters table mod_bad_luck_streak, read at startup, so a restart forgets nothing.
 *
 * Released under the MIT License.
 */

#include "BadLuck.h"
#include "Chat.h"
#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Group.h"
#include "InstanceSaveMgr.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldSession.h"

#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

using namespace BadLuck;

namespace BadLuck
{
    Config config;
}

namespace
{
    // The rules, and ways into them. Written only by LoadRules on the world thread while the
    // maps are not updating (startup, a command).
    std::vector<Rule> rules;
    std::unordered_map<uint32, std::vector<size_t>> rulesByCreature;    // creature entry -> rules
    std::unordered_map<uint32, std::vector<size_t>> rulesByItem;        // item -> rules
    std::unordered_map<uint32, uint32> itemByMountSpell;                // mount spell -> item

    // A streak: counted kills without the item, and the period of the last kill that counted.
    struct Streak
    {
        uint32 fails = 0;
        uint64 periodEnd = 0;
        uint32 instanceId = 0;
    };

    // scope -> owner -> item -> streak. Bosses die on map threads, several maps at once, hence
    // the lock.
    std::mutex streaksLock;
    std::unordered_map<uint32, std::unordered_map<uint32, Streak>> streaks[2];

    uint32 OwnerOf(Player* player, Scope scope)
    {
        return scope == SCOPE_ACCOUNT ? player->GetSession()->GetAccountId() : player->GetGUID().GetCounter();
    }

    // The mount an item teaches: the spell its use-effect learns, or the spell a learn-trigger
    // row names. 0 for an item that teaches nothing.
    uint32 MountSpellOf(ItemTemplate const* proto)
    {
        for (_Spell const& spell : proto->Spells)
        {
            if (spell.SpellId <= 0)
                continue;

            if (spell.SpellTrigger == ITEM_SPELLTRIGGER_LEARN_SPELL_ID)
                return uint32(spell.SpellId);

            if (spell.SpellTrigger != ITEM_SPELLTRIGGER_ON_USE)
                continue;

            SpellInfo const* info = sSpellMgr->GetSpellInfo(uint32(spell.SpellId));
            if (!info)
                continue;

            for (SpellEffectInfo const& effect : info->GetEffects())
                if (effect.Effect == SPELL_EFFECT_LEARN_SPELL && effect.TriggerSpell)
                    return effect.TriggerSpell;
        }

        return 0;
    }

    // When the period a kill on this map falls in ends. A raid: its next reset, so each lockout
    // counts once whatever hour the raid resets at. Anything else: the next daily quest reset.
    uint64 PeriodEnd(Map* map)
    {
        if (map->IsRaid())
            if (time_t reset = sInstanceSaveMgr->GetResetTimeFor(map->GetId(), map->GetDifficulty()))
                return uint64(reset);

        return uint64(sWorld->GetNextDailyQuestsResetTime().count());
    }

    // The instance a kill counts for: a raid's, so that the same lockout in another instance
    // (a game master released the character) counts again. 0 for anything else, or resetting a
    // dungeon would count again.
    uint32 KillInstance(Map* map)
    {
        return map->IsRaid() ? map->GetInstanceId() : 0;
    }

    // Has the item already, the mount it teaches, or what it is made into: nothing to roll for.
    bool Owns(Player* player, Rule const& rule)
    {
        if (player->HasItemCount(rule.item, 1, true))
            return true;
        if (rule.completeItem && player->HasItemCount(rule.completeItem, 1, true))
            return true;
        if (rule.mountSpell && player->HasSpell(rule.mountSpell))
            return true;
        return false;
    }

    void SaveStreak(uint32 owner, Scope scope, uint32 item, Streak const& streak)
    {
        CharacterDatabase.Execute("REPLACE INTO mod_bad_luck_streak (owner, scope, item, fails, period_end, instance) VALUES ({}, {}, {}, {}, {}, {})",
            owner, uint32(scope), item, streak.fails, streak.periodEnd, streak.instanceId);
    }

    void DeleteStreak(uint32 owner, Scope scope, uint32 item)
    {
        CharacterDatabase.Execute("DELETE FROM mod_bad_luck_streak WHERE owner = {} AND scope = {} AND item = {}",
            owner, uint32(scope), item);
    }

    // Forgets the owner's streak for the item, if there is one.
    void EndStreak(uint32 owner, Scope scope, uint32 item)
    {
        bool had = false;
        {
            std::lock_guard<std::mutex> guard(streaksLock);
            auto owners = streaks[scope].find(owner);
            if (owners != streaks[scope].end())
            {
                had = owners->second.erase(item) > 0;
                if (owners->second.empty())
                    streaks[scope].erase(owners);
            }
        }

        if (had)
            DeleteStreak(owner, scope, item);
    }

    // The player got the item, by whatever road: every streak for it is over.
    void EndStreaksFor(Player* player, uint32 item)
    {
        auto itr = rulesByItem.find(item);
        if (itr == rulesByItem.end())
            return;

        // Rules for one item share a scope as a rule; both are tried in case a hand-written row
        // doesn't.
        bool scopes[2] = { false, false };
        for (size_t index : itr->second)
            scopes[rules[index].scope] = true;

        for (uint8 scope = 0; scope < 2; ++scope)
            if (scopes[scope])
                EndStreak(OwnerOf(player, Scope(scope)), Scope(scope), item);
    }

    void Give(Player* player, Rule const& rule, Creature* boss)
    {
        ChatHandler chat(player->GetSession());
        std::string link = ItemLink(rule.item);

        ItemPosCountVec dest;
        bool stored = false;
        if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, rule.item, 1) == EQUIP_ERR_OK)
            if (Item* item = player->StoreNewItem(dest, rule.item, true))
            {
                player->SendNewItem(item, 1, true, false);
                stored = true;
            }

        // Full bags: by mail, as the core does for quest rewards.
        if (!stored)
            player->SendItemRetrievalMail(rule.item, 1);

        if (!config.announce)
            return;

        chat.PSendSysMessage("|cff40c040Bad luck protection:|r your persistence pays off. {} drops {} for you{}.",
            boss->GetName(), link, stored ? "" : ", sent to your mailbox because your bags are full");

        if (config.announce < 2)
            return;

        Group* group = player->GetGroup();
        if (!group)
            return;

        // Only group members on this map: this runs in the map's thread.
        for (GroupReference const* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || member == player || !member->IsInMap(player) || !IsRealPlayer(member))
                continue;
            ChatHandler(member->GetSession()).PSendSysMessage("|cff40c040Bad luck protection:|r {}'s persistence pays off. {} drops {} for them.",
                player->GetName(), boss->GetName(), link);
        }
    }

    // One player's roll for one rule on this kill.
    void Roll(Player* player, Rule const& rule, Creature* boss, uint64 periodEnd, uint32 instanceId)
    {
        uint32 owner = OwnerOf(player, rule.scope);

        if (Owns(player, rule))
        {
            EndStreak(owner, rule.scope, rule.item);
            return;
        }

        float step = StepOf(rule);
        bool won = false;
        Streak after;
        {
            std::lock_guard<std::mutex> guard(streaksLock);
            Streak& streak = streaks[rule.scope][owner][rule.item];

            // Counted this period already (the same kill again: a reset dungeon, or a raid
            // boss that the core credits twice).
            if (!config.countEveryKill && streak.periodEnd == periodEnd && streak.instanceId == instanceId && streak.periodEnd)
                return;

            float chance = step * float(streak.fails);
            won = chance > 0.0f && roll_chance_f(chance);
            if (won)
            {
                streaks[rule.scope][owner].erase(rule.item);
                if (streaks[rule.scope][owner].empty())
                    streaks[rule.scope].erase(owner);
            }
            else
            {
                ++streak.fails;
                streak.periodEnd = periodEnd;
                streak.instanceId = instanceId;
                after = streak;
            }
        }

        if (won)
        {
            DeleteStreak(owner, rule.scope, rule.item);
            Give(player, rule, boss);
            return;
        }

        SaveStreak(owner, rule.scope, rule.item, after);

        if (config.announce)
        {
            float next = std::min(100.0f, step * float(after.fails));
            ChatHandler(player->GetSession()).PSendSysMessage("|cff40c040Bad luck protection:|r {} kill{} without {}. Your bonus chance at the next one is {}%.",
                after.fails, after.fails == 1 ? "" : "s", ItemLink(rule.item), next);
        }
    }

    // The players a kill counts for.
    std::vector<Player*> PlayersOf(Creature* boss, Map* map)
    {
        std::vector<Player*> players;

        auto eligible = [](Player* player)
        {
            return player && IsRealPlayer(player) && !player->IsGameMaster();
        };

        if (map->IsDungeon())
        {
            for (MapReference const& ref : map->GetPlayers())
                if (Player* player = ref.GetSource(); eligible(player))
                    players.push_back(player);
            return players;
        }

        // Out in the world: the group that tapped the boss, as far as it is standing near.
        Player* recipient = boss->GetLootRecipient();
        if (Group* group = boss->GetLootRecipientGroup())
        {
            for (GroupReference const* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* member = ref->GetSource();
                if (eligible(member) && member->IsInMap(boss) && member->IsWithinDistInMap(boss, config.worldBossRange))
                    players.push_back(member);
            }
            return players;
        }

        if (eligible(recipient) && recipient->IsInMap(boss) && recipient->IsWithinDistInMap(boss, config.worldBossRange))
            players.push_back(recipient);
        return players;
    }

    void OnBossDeath(Creature* boss)
    {
        auto itr = rulesByCreature.find(boss->GetEntry());
        if (itr == rulesByCreature.end())
            return;

        // Killed by players, as the core asks before it gives loot or credit: not by a game
        // master's command or by something else in the room.
        if (!boss->IsDamageEnoughForLootingAndReward())
            return;

        Map* map = boss->FindMap();
        if (!map)
            return;

        std::vector<Player*> players = PlayersOf(boss, map);
        if (players.empty())
            return;

        uint64 periodEnd = PeriodEnd(map);
        uint32 instanceId = KillInstance(map);
        for (size_t index : itr->second)
            for (Player* player : players)
                Roll(player, rules[index], boss, periodEnd, instanceId);
    }

    void LoadStreaks()
    {
        // A streak of a character that is gone.
        CharacterDatabase.DirectExecute("DELETE s FROM mod_bad_luck_streak AS s LEFT JOIN characters AS c ON c.guid = s.owner "
            "WHERE s.scope = 0 AND c.guid IS NULL");

        std::lock_guard<std::mutex> guard(streaksLock);
        streaks[0].clear();
        streaks[1].clear();

        uint32 count = 0;
        if (QueryResult result = CharacterDatabase.Query("SELECT owner, scope, item, fails, period_end, instance FROM mod_bad_luck_streak"))
        {
            do
            {
                Field* fields = result->Fetch();
                uint8 scope = fields[1].Get<uint8>();
                if (scope > 1)
                    continue;
                streaks[scope][fields[0].Get<uint32>()][fields[2].Get<uint32>()] =
                    { fields[3].Get<uint32>(), fields[4].Get<uint64>(), fields[5].Get<uint32>() };
                ++count;
            } while (result->NextRow());
        }

        LOG_INFO("server.loading", "mod-bad-luck-protection: {} streak(s) loaded", count);
    }

    // An instance is gone for good. The server gives its id to a new instance after a restart,
    // and a kill there must not look like the one already counted.
    void ForgetInstance(uint32 instanceId)
    {
        if (!instanceId)
            return;

        bool forgot = false;
        {
            std::lock_guard<std::mutex> guard(streaksLock);
            for (auto& byOwner : streaks)
                for (auto& [owner, items] : byOwner)
                    for (auto& [item, streak] : items)
                        if (streak.instanceId == instanceId)
                        {
                            streak.instanceId = 0;
                            streak.periodEnd = 0;
                            forgot = true;
                        }
        }

        if (forgot)
            CharacterDatabase.Execute("UPDATE mod_bad_luck_streak SET instance = 0, period_end = 0 WHERE instance = {}", instanceId);
    }
}

bool BadLuck::IsRealPlayer(Player* player)
{
    WorldSession* session = player ? player->GetSession() : nullptr;
    return session && !IsBotSession(session);
}

float BadLuck::StepOf(Rule const& rule)
{
    if (rule.step > 0.0f)
        return rule.step;
    return rule.kind == KIND_MOUNT ? config.stepMount : config.stepLegendary;
}

std::vector<Rule> const& BadLuck::Rules()
{
    return rules;
}

uint32 BadLuck::LoadRules()
{
    rules.clear();
    rulesByCreature.clear();
    rulesByItem.clear();
    itemByMountSpell.clear();

    uint32 skipped = 0;
    if (QueryResult result = WorldDatabase.Query("SELECT item, creature, kind, scope, step, complete_item, comment FROM mod_bad_luck_rule ORDER BY kind, item, creature"))
    {
        do
        {
            Field* fields = result->Fetch();
            Rule rule;
            rule.item = fields[0].Get<uint32>();
            rule.creature = fields[1].Get<uint32>();
            rule.kind = fields[2].Get<uint8>() == KIND_MOUNT ? KIND_MOUNT : KIND_LEGENDARY;
            rule.scope = fields[3].Get<uint8>() == SCOPE_ACCOUNT ? SCOPE_ACCOUNT : SCOPE_CHARACTER;
            rule.step = std::clamp(fields[4].Get<float>(), 0.0f, 100.0f);
            rule.completeItem = fields[5].Get<uint32>();
            rule.comment = fields[6].Get<std::string>();

            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(rule.item);
            if (!proto)
            {
                LOG_ERROR("server.loading", "mod-bad-luck-protection: mod_bad_luck_rule names item {} (creature {}), which doesn't exist; row skipped",
                    rule.item, rule.creature);
                ++skipped;
                continue;
            }

            // A boss this server doesn't have (another module's) never dies: the row is kept,
            // for `.luck all` and in case the module arrives, but it is said once.
            if (!sObjectMgr->GetCreatureTemplate(rule.creature))
                ++skipped;

            rule.mountSpell = MountSpellOf(proto);
            if (rule.mountSpell)
                itemByMountSpell[rule.mountSpell] = rule.item;

            rulesByCreature[rule.creature].push_back(rules.size());
            rulesByItem[rule.item].push_back(rules.size());
            rules.push_back(std::move(rule));
        } while (result->NextRow());
    }

    LOG_INFO("server.loading", "mod-bad-luck-protection: {} rule(s) for {} item(s) loaded, {} with a boss or item not on this server",
        rules.size(), rulesByItem.size(), skipped);
    return uint32(rules.size());
}

std::vector<Standing> BadLuck::StandingsOf(Player* player)
{
    std::vector<Standing> standings;
    std::unordered_map<uint32, size_t> byItem;

    for (Rule const& rule : rules)
    {
        auto [itr, added] = byItem.try_emplace(rule.item, standings.size());
        if (added)
        {
            Standing standing;
            standing.item = rule.item;
            standing.kind = rule.kind;
            standing.scope = rule.scope;
            standing.owned = Owns(player, rule);
            standings.push_back(std::move(standing));
        }
        standings[itr->second].creatures.push_back(rule.creature);
    }

    std::lock_guard<std::mutex> guard(streaksLock);
    for (Standing& standing : standings)
    {
        float step = StepOf(rules[rulesByItem[standing.item].front()]);
        auto owners = streaks[standing.scope].find(OwnerOf(player, standing.scope));
        if (owners == streaks[standing.scope].end())
            continue;
        auto streak = owners->second.find(standing.item);
        if (streak == owners->second.end())
            continue;
        standing.fails = streak->second.fails;
        standing.nextChance = std::min(100.0f, step * float(standing.fails));
    }

    return standings;
}

bool BadLuck::SetFails(Player* player, uint32 item, uint32 fails)
{
    auto itr = rulesByItem.find(item);
    if (itr == rulesByItem.end())
        return false;

    Scope scope = rules[itr->second.front()].scope;
    uint32 owner = OwnerOf(player, scope);
    if (!fails)
    {
        EndStreak(owner, scope, item);
        return true;
    }

    Streak streak;
    {
        std::lock_guard<std::mutex> guard(streaksLock);
        // The next kill counts, whatever was counted this period.
        Streak& current = streaks[scope][owner][item];
        current.fails = fails;
        current.periodEnd = 0;
        current.instanceId = 0;
        streak = current;
    }
    SaveStreak(owner, scope, item, streak);
    return true;
}

void BadLuck::ResetAll(Player* player)
{
    for (uint8 scope = 0; scope < 2; ++scope)
    {
        uint32 owner = OwnerOf(player, Scope(scope));
        bool had = false;
        {
            std::lock_guard<std::mutex> guard(streaksLock);
            had = streaks[scope].erase(owner) > 0;
        }
        if (had)
            CharacterDatabase.Execute("DELETE FROM mod_bad_luck_streak WHERE owner = {} AND scope = {}", owner, scope);
    }
}

std::string BadLuck::ItemLink(uint32 item)
{
    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(item);
    if (!proto)
        return Acore::StringFormat("item {}", item);

    uint32 color = proto->Quality < MAX_ITEM_QUALITY ? ItemQualityColors[proto->Quality] : ItemQualityColors[ITEM_QUALITY_NORMAL];
    return Acore::StringFormat("|c{:08x}|Hitem:{}:0:0:0:0:0:0:0:0|h[{}]|h|r", color, item, proto->Name1);
}

class BadLuckUnitScript : public UnitScript
{
public:
    BadLuckUnitScript() : UnitScript("BadLuckUnitScript", true, { UNITHOOK_ON_UNIT_DEATH }) { }

    // Every death in the world comes through here, so the first question is the cheap one: is
    // this one of the handful of creatures with a rule?
    void OnUnitDeath(Unit* unit, Unit* /*killer*/) override
    {
        if (!config.enabled || rulesByCreature.empty() || !unit->IsCreature())
            return;

        OnBossDeath(unit->ToCreature());
    }
};

class BadLuckPlayerScript : public PlayerScript
{
public:
    BadLuckPlayerScript() : PlayerScript("BadLuckPlayerScript", { PLAYERHOOK_ON_STORE_NEW_ITEM, PLAYERHOOK_ON_LEARN_SPELL }) { }

    // The item arrived, from the corpse, a roll, the master looter, the mail or a game master:
    // the streak is over.
    void OnPlayerStoreNewItem(Player* player, Item* item, uint32 /*count*/) override
    {
        if (!item || rulesByItem.empty())
            return;
        EndStreaksFor(player, item->GetEntry());
    }

    // The mount is learned (on this character, or on an alt with account-wide mounts): over too.
    void OnPlayerLearnSpell(Player* player, uint32 spellId) override
    {
        auto itr = itemByMountSpell.find(spellId);
        if (itr == itemByMountSpell.end())
            return;
        EndStreaksFor(player, itr->second);
    }
};

class BadLuckGlobalScript : public GlobalScript
{
public:
    BadLuckGlobalScript() : GlobalScript("BadLuckGlobalScript", { GLOBALHOOK_ON_INSTANCEID_REMOVED }) { }

    void OnInstanceIdRemoved(uint32 instanceId) override
    {
        ForgetInstance(instanceId);
    }
};

class BadLuckWorldScript : public WorldScript
{
public:
    BadLuckWorldScript() : WorldScript("BadLuckWorldScript", { WORLDHOOK_ON_AFTER_CONFIG_LOAD, WORLDHOOK_ON_STARTUP }) { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        config.enabled = sConfigMgr->GetOption<bool>("BadLuck.Enable", true);
        config.stepLegendary = std::clamp(sConfigMgr->GetOption<float>("BadLuck.Step.Legendary", 10.0f), 0.0f, 100.0f);
        config.stepMount = std::clamp(sConfigMgr->GetOption<float>("BadLuck.Step.Mount", 5.0f), 0.0f, 100.0f);
        config.countEveryKill = sConfigMgr->GetOption<bool>("BadLuck.CountEveryKill", false);
        config.announce = uint8(std::clamp(sConfigMgr->GetOption<uint32>("BadLuck.Announce", 1), 0u, 2u));
        config.worldBossRange = std::max(0.0f, sConfigMgr->GetOption<float>("BadLuck.WorldBossRange", 150.0f));

        LOG_INFO("server.loading", "mod-bad-luck-protection: {}, +{}% per kill for legendaries, +{}% for mounts, counting {}",
            config.enabled ? "enabled" : "disabled", config.stepLegendary, config.stepMount,
            config.countEveryKill ? "every kill" : "one kill per lockout or day");
    }

    void OnStartup() override
    {
        LoadRules();
        LoadStreaks();
    }
};

void AddBadLuckScripts()
{
    new BadLuckUnitScript();
    new BadLuckPlayerScript();
    new BadLuckGlobalScript();
    new BadLuckWorldScript();
}
