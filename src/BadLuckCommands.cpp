/*
 * mod-bad-luck-protection: the chat commands.
 *
 *   .luck               your streaks: every protected item you have killed a boss for and not got
 *   .luck all           every protected item, with where you stand on each
 *   .badluck set <item> <kills> [player]   a game master puts a streak at that many kills (0 ends it)
 *   .badluck reset [player]                ends every streak of the character and its account
 *   .badluck reload                        reads mod_bad_luck_rule again
 *
 * Released under the MIT License.
 */

#include "BadLuck.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "WorldSession.h"

using namespace Acore::ChatCommands;
using namespace BadLuck;

namespace
{
    std::string BossNames(std::vector<uint32> const& creatures)
    {
        std::string names;
        std::string last;
        for (uint32 entry : creatures)
        {
            CreatureTemplate const* proto = sObjectMgr->GetCreatureTemplate(entry);
            std::string name = proto ? proto->Name : Acore::StringFormat("creature {}", entry);
            if (name == last)
                continue;   // the 10 and 25 player entries share a name
            if (!names.empty())
                names += ", ";
            names += name;
            last = name;
        }
        return names;
    }

    void SendStandings(ChatHandler* handler, Player* player, bool all)
    {
        std::vector<Standing> standings = StandingsOf(player);
        uint32 shown = 0;
        for (Standing const& standing : standings)
        {
            if (!all && (standing.owned || !standing.fails))
                continue;

            std::string line = Acore::StringFormat("{} ({}): ", ItemLink(standing.item), BossNames(standing.creatures));
            if (standing.owned)
                line += "yours already.";
            else if (!standing.fails)
                line += "no kill counted yet.";
            else
                line += Acore::StringFormat("{} kill{} without it, {}% bonus chance at the next.",
                    standing.fails, standing.fails == 1 ? "" : "s", standing.nextChance);
            if (standing.scope == SCOPE_ACCOUNT)
                line += " Shared by your account.";
            handler->SendSysMessage(line);
            ++shown;
        }

        if (!shown)
        {
            if (all)
                handler->SendSysMessage("No drops are protected on this server.");
            else
                handler->PSendSysMessage("No streak running: every protected drop you have killed a boss for is yours, or you haven't "
                    "killed one yet. Type .luck all for the {} protected drops.", standings.size());
        }
    }
}

class BadLuckCommandScript : public CommandScript
{
public:
    BadLuckCommandScript() : CommandScript("BadLuckCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable luckCommandTable =
        {
            { "",       HandleLuckCommand,    SEC_PLAYER, Console::No },
            { "all",    HandleLuckAllCommand, SEC_PLAYER, Console::No },
        };

        static ChatCommandTable badLuckCommandTable =
        {
            { "",       HandleLuckCommand,          SEC_PLAYER,        Console::No  },
            { "all",    HandleLuckAllCommand,       SEC_PLAYER,        Console::No  },
            { "set",    HandleBadLuckSetCommand,    SEC_GAMEMASTER,    Console::No  },
            { "reset",  HandleBadLuckResetCommand,  SEC_GAMEMASTER,    Console::No  },
            { "reload", HandleBadLuckReloadCommand, SEC_ADMINISTRATOR, Console::Yes },
        };

        static ChatCommandTable commandTable =
        {
            { "luck",    luckCommandTable },
            { "badluck", badLuckCommandTable },
        };

        return commandTable;
    }

    static bool HandleLuckCommand(ChatHandler* handler)
    {
        SendStandings(handler, handler->GetSession()->GetPlayer(), false);
        return true;
    }

    static bool HandleLuckAllCommand(ChatHandler* handler)
    {
        SendStandings(handler, handler->GetSession()->GetPlayer(), true);
        return true;
    }

    static Player* TargetOf(ChatHandler* handler, Optional<PlayerIdentifier> target)
    {
        if (!target)
            target = PlayerIdentifier::FromTargetOrSelf(handler);
        if (!target || !target->IsConnected())
        {
            handler->SendErrorMessage("That player isn't online.");
            return nullptr;
        }
        return target->GetConnectedPlayer();
    }

    static bool HandleBadLuckSetCommand(ChatHandler* handler, uint32 item, uint32 fails, Optional<PlayerIdentifier> target)
    {
        Player* player = TargetOf(handler, target);
        if (!player)
            return false;

        if (!SetFails(player, item, fails))
        {
            handler->SendErrorMessage("No rule protects item {}. See .luck all.", item);
            return false;
        }

        handler->PSendSysMessage("{}'s streak for {} is {} kill{} without it now.", player->GetName(), ItemLink(item), fails,
            fails == 1 ? "" : "s");
        return true;
    }

    static bool HandleBadLuckResetCommand(ChatHandler* handler, Optional<PlayerIdentifier> target)
    {
        Player* player = TargetOf(handler, target);
        if (!player)
            return false;

        ResetAll(player);
        handler->PSendSysMessage("Every streak of {} and their account is over.", player->GetName());
        return true;
    }

    static bool HandleBadLuckReloadCommand(ChatHandler* handler)
    {
        uint32 count = LoadRules();
        handler->PSendSysMessage("mod_bad_luck_rule read again: {} rule(s).", count);
        return true;
    }
};

void AddBadLuckCommandScripts()
{
    new BadLuckCommandScript();
}
