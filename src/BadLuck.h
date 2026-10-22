/*
 * mod-bad-luck-protection: what the module's files share.
 *
 * Released under the MIT License.
 */

#ifndef MOD_BAD_LUCK_PROTECTION_H
#define MOD_BAD_LUCK_PROTECTION_H

#include "Define.h"

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

class Player;

namespace BadLuck
{
    enum Scope : uint8
    {
        SCOPE_CHARACTER = 0,
        SCOPE_ACCOUNT   = 1,
    };

    enum Kind : uint8
    {
        KIND_LEGENDARY = 0,
        KIND_MOUNT     = 1,
    };

    // A row of world table mod_bad_luck_rule: one protected drop from one boss creature.
    struct Rule
    {
        uint32 item = 0;
        uint32 creature = 0;
        Kind kind = KIND_LEGENDARY;
        Scope scope = SCOPE_CHARACTER;
        float step = 0.0f;          // percent per counted kill, 0 = the kind's default
        uint32 completeItem = 0;    // owning this ends the streak too (what the drop is made into)
        uint32 mountSpell = 0;      // the mount the item teaches, 0 for none (found at load)
        uint32 allowableClass = 0;  // the item's own class and race limits (found at load)
        uint32 allowableRace = 0;
        std::string comment;
    };

    struct Config
    {
        bool enabled = true;
        float stepLegendary = 10.0f;
        float stepMount = 5.0f;
        bool countEveryKill = false;
        uint8 announce = 1;         // 0 silent, 1 whisper the player, 2 tell the group of a win too
    };

    extern Config config;

    // Bot sessions: IsHeadless() on current playerbots cores, IsBot() on older ones.
    template <typename Session, typename = void>
    struct HasIsHeadless : std::false_type { };

    template <typename Session>
    struct HasIsHeadless<Session, std::void_t<decltype(std::declval<Session&>().IsHeadless())>> : std::true_type { };

    template <typename Session, typename = void>
    struct HasIsBot : std::false_type { };

    template <typename Session>
    struct HasIsBot<Session, std::void_t<decltype(std::declval<Session&>().IsBot())>> : std::true_type { };

    template <typename Session>
    bool IsBotSession(Session* session)
    {
        if constexpr (HasIsHeadless<Session>::value)
            return session->IsHeadless();
        else if constexpr (HasIsBot<Session>::value)
            return session->IsBot();
        else
            return false;
    }

    // A player with a client: not a bot.
    bool IsRealPlayer(Player* player);

    // The step a rule uses: its own, or its kind's default.
    float StepOf(Rule const& rule);

    // Every rule, in table order. Read on the world thread (commands) and the map threads (boss
    // deaths); only Load changes it, at startup and on `.badluck reload`, on the world thread
    // while the maps are not updating.
    std::vector<Rule> const& Rules();

    // Reads mod_bad_luck_rule again and tells how many rules it has.
    uint32 LoadRules();

    // One protected item and where the player stands with it: `fails` counted kills without
    // it, `nextChance` the bonus roll percent at the next counted kill.
    struct Standing
    {
        uint32 item = 0;
        Kind kind = KIND_LEGENDARY;
        Scope scope = SCOPE_CHARACTER;
        std::vector<uint32> creatures;
        float step = 0.0f;
        uint32 fails = 0;
        float nextChance = 0.0f;
        bool owned = false;         // has the item, its mount or its complete_item already
    };

    // The player's standing with every protected item their class and race can use, one entry
    // per item.
    std::vector<Standing> StandingsOf(Player* player);

    // Game master's hands on a streak. SetFails returns false for an item no rule protects.
    bool SetFails(Player* player, uint32 item, uint32 fails);
    void ResetAll(Player* player);

    // "|cff...|Hitem:...|h[Name]|h|r", or "item N" for one the server doesn't know.
    std::string ItemLink(uint32 item);
}

void AddBadLuckScripts();
void AddBadLuckCommandScripts();

#endif
