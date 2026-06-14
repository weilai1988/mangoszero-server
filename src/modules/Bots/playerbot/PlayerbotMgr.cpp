#include "../botpch.h"
#include "playerbot.h"
#include "AccountMgr.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotDbStore.h"
#include "PlayerbotFactory.h"
#include "RandomItemMgr.h"
#include "RandomPlayerbotMgr.h"
#include "ServerFacade.h"
#include "World.h"

#include <algorithm>
#include <cctype>


class LoginQueryHolder;
class CharacterHandler;

namespace
{
    map<uint64, string> pendingPlayerbotRoleCommands;
    map<uint64, uint32> pendingPlayerbotInitQualities;
    map<uint64, uint32> pendingPlayerbotInitLevels;
    set<uint64> pendingPlayerbotSkipGroup;
    map<uint64, ObjectGuid> pendingPlayerbotShootPullTargets;

    uint8 GetPlayerbotClassId(const string& name);
    uint8 GetPlayerbotRaceId(const string& name);
    uint32 GetPlayerbotItemQuality(const string& name);
    uint8 ChoosePlayerbotRace(Player* master, uint8 cls);
    bool IsPlayerbotRaceClassAllowed(uint8 race, uint8 cls);

    string GetPlayerbotClassName(uint8 cls)
    {
        switch (cls)
        {
        case CLASS_DRUID: return "Druid";
        case CLASS_HUNTER: return "Hunter";
        case CLASS_MAGE: return "Mage";
        case CLASS_PALADIN: return "Paladin";
        case CLASS_PRIEST: return "Priest";
        case CLASS_ROGUE: return "Rogue";
        case CLASS_SHAMAN: return "Shaman";
        case CLASS_WARLOCK: return "Warlock";
        case CLASS_WARRIOR: return "Warrior";
        default: return "Unknown";
        }
    }

    string BuildPlayerbotUIntList(vector<uint32> const& values)
    {
        ostringstream out;
        bool first = true;
        for (vector<uint32>::const_iterator i = values.begin(); i != values.end(); ++i)
        {
            if (first) first = false; else out << ",";
            out << *i;
        }

        return out.str();
    }

    string BuildRandomBotAccountList()
    {
        ostringstream out;
        bool first = true;
        for (list<uint32>::const_iterator i = sPlayerbotAIConfig.randomBotAccounts.begin();
                i != sPlayerbotAIConfig.randomBotAccounts.end(); ++i)
        {
            if (first) first = false; else out << ",";
            out << *i;
        }

        return out.str();
    }

    uint32 SelectRandomPlayerbotAccount()
    {
        string accounts = BuildRandomBotAccountList();
        if (accounts.empty())
            return 0;

        QueryResult* results = CharacterDatabase.PQuery(
            "SELECT account, COUNT(*) AS chars FROM characters WHERE account IN (%s) GROUP BY account ORDER BY chars ASC LIMIT 1",
            accounts.c_str());
        if (results)
        {
            uint32 account = results->Fetch()[0].GetUInt32();
            delete results;
            return account;
        }

        return sPlayerbotAIConfig.randomBotAccounts.empty() ? 0 : sPlayerbotAIConfig.randomBotAccounts.front();
    }

    string TrimPlayerbotCommandParam(string text)
    {
        string::size_type start = text.find_first_not_of(" \t\r\n");
        if (start == string::npos)
            return "";

        string::size_type end = text.find_last_not_of(" \t\r\n");
        return text.substr(start, end - start + 1);
    }

    void SplitPlayerbotCommand(string input, string& cmd, string& name, string& param)
    {
        cmd.clear();
        name.clear();
        param.clear();

        input = TrimPlayerbotCommandParam(input);
        if (input.empty())
            return;

        string::size_type firstSpace = input.find_first_of(" \t\r\n");
        cmd = firstSpace == string::npos ? input : input.substr(0, firstSpace);
        string rest = firstSpace == string::npos ? "" : TrimPlayerbotCommandParam(input.substr(firstSpace + 1));

        if (cmd == "bot")
        {
            SplitPlayerbotCommand(rest, cmd, name, param);
            return;
        }

        if (rest.empty())
            return;

        string::size_type secondSpace = rest.find_first_of(" \t\r\n");
        name = secondSpace == string::npos ? rest : rest.substr(0, secondSpace);
        param = secondSpace == string::npos ? "" : TrimPlayerbotCommandParam(rest.substr(secondSpace + 1));
    }

    bool IsPlayerbotManagementCommand(const string& cmd)
    {
        return cmd == "add" || cmd == "login" || cmd == "control" || cmd == "party" ||
            cmd == "remove" || cmd == "logout" || cmd == "rm" ||
            cmd == "create" || cmd == "account" ||
            cmd == "init" || cmd == "update" || cmd == "random" ||
            cmd == "list" || cmd == "pool" ||
            cmd == "level" || cmd == "levelup" || cmd == "refresh" ||
            cmd == "init=white" || cmd == "init=common" ||
            cmd == "init=green" || cmd == "init=uncommon" ||
            cmd == "init=blue" || cmd == "init=rare" ||
            cmd == "init=epic" || cmd == "init=purple";
    }

    string BuildPlayerbotChatCommand(const string& cmd, const string& param)
    {
        if (cmd == "cmd" || cmd == "w")
            return param;

        if (cmd == "do")
            return param.empty() ? "" : "do " + param;

        return param.empty() ? cmd : cmd + " " + param;
    }

    string LowerPlayerbotCommandParam(string text)
    {
        for (string::iterator i = text.begin(); i != text.end(); ++i)
            *i = static_cast<char>(std::tolower(static_cast<unsigned char>(*i)));

        return text;
    }

    string BuildPlayerbotRoleCommand(const string& param)
    {
        string role = LowerPlayerbotCommandParam(TrimPlayerbotCommandParam(param));

        if (role == "1" || role == "heal" || role == "healer")
            return "co +heal,+cure,-conserve mana,-dps,-tank,-bear";

        if (role == "2" || role == "dps" || role == "damage")
            return "co +dps,+dps assist,+threat,-heal,-tank,-bear";

        if (role == "3" || role == "tank")
            return "co +tank,+tank assist,+tank aoe,+bthreat,+threat,-heal,-dps";

        return "";
    }

    string GetPlayerbotGearRoleFromRoleCommand(const string& command)
    {
        if (command.find("+tank") != string::npos || command.find("+bear") != string::npos)
            return "tank";

        if (command.find("+heal") != string::npos)
            return "heal";

        if (command.find("+dps") != string::npos)
            return "dps";

        return "";
    }


    bool ParseTwoPlayerbotCommandParams(const string& input, string& first, string& second)
    {
        first.clear();
        second.clear();

        string text = TrimPlayerbotCommandParam(input);
        if (text.empty())
            return false;

        string::size_type firstSpace = text.find_first_of(" \t\r\n");
        if (firstSpace == string::npos)
            return false;

        first = text.substr(0, firstSpace);
        second = TrimPlayerbotCommandParam(text.substr(firstSpace + 1));
        return !first.empty() && !second.empty();
    }

    bool IsSafePlayerbotAccountToken(const string& text)
    {
        if (text.empty() || text.size() > MAX_ACCOUNT_STR)
            return false;

        for (string::const_iterator i = text.begin(); i != text.end(); ++i)
        {
            unsigned char c = static_cast<unsigned char>(*i);
            if (!std::isalnum(c) && c != '_')
                return false;
        }

        return true;
    }

    void MarkPlayerbotLoginAccount(uint32 accountId)
    {
        if (!accountId)
            return;

        LoginDatabase.PExecute("UPDATE account SET playerBot = 1 WHERE id = '%u'", accountId);
    }

    string BuildAccountCreateResult(AccountOpResult result, const string& accountName)
    {
        switch (result)
        {
        case AOR_OK:
            return "account create: " + accountName + " ok";
        case AOR_NAME_TOO_LONG:
            return "account create: account name is too long";
        case AOR_PASS_TOO_LONG:
            return "account create: password is too long";
        case AOR_NAME_ALREADY_EXIST:
            return "account create: account already exists";
        case AOR_DB_INTERNAL_ERROR:
            return "account create: database error";
        default:
            return "account create: failed";
        }
    }

    bool ParseAccountCharacterParams(const string& input, string& accountName, string& characterName, string& params)
    {
        accountName.clear();
        characterName.clear();
        params.clear();

        string text = TrimPlayerbotCommandParam(input);
        if (text.empty())
            return false;

        string::size_type firstSpace = text.find_first_of(" \t\r\n");
        if (firstSpace == string::npos)
            return false;

        accountName = text.substr(0, firstSpace);
        string rest = TrimPlayerbotCommandParam(text.substr(firstSpace + 1));
        string::size_type secondSpace = rest.find_first_of(" \t\r\n");
        if (secondSpace == string::npos)
            return false;

        characterName = rest.substr(0, secondSpace);
        params = TrimPlayerbotCommandParam(rest.substr(secondSpace + 1));
        return !accountName.empty() && !characterName.empty() && !params.empty();
    }

    int32 GetPlayerbotGenderId(const string& name)
    {
        string gender = LowerPlayerbotCommandParam(TrimPlayerbotCommandParam(name));
        if (gender == "male" || gender == "m")
            return GENDER_MALE;

        if (gender == "female" || gender == "f")
            return GENDER_FEMALE;

        if (gender == "random" || gender == "rand")
            return -2;

        return -1;
    }

    string CreatePlayerbotLoginAccount(Player* master, const string& param)
    {
        if (!master || !master->GetSession() ||
                master->GetSession()->GetSecurity() < SEC_ADMINISTRATOR)
            return "account create: not allowed";

        string accountName, password;
        if (!ParseTwoPlayerbotCommandParams(param, accountName, password))
            return "usage: account create ACCOUNT PASSWORD";

        if (!IsSafePlayerbotAccountToken(accountName))
            return "account create: use 1-16 letters, numbers, or underscore for account";

        if (password.empty() || password.size() > MAX_PASSWORD_STR)
            return "account create: password must be 1-16 characters";

        AccountOpResult result = sAccountMgr.CreateAccount(accountName, password);
        if (result == AOR_OK)
        {
            string normalizedAccountName = accountName;
            Utf8ToUpperOnlyLatin(normalizedAccountName);
            MarkPlayerbotLoginAccount(sAccountMgr.GetId(normalizedAccountName));
        }

        return BuildAccountCreateResult(result, accountName);
    }

    void AppendPlayerbotLoginAccountMessages(list<string>& messages, Player* master, const string& param)
    {
        if (!master || !master->GetSession() ||
                master->GetSession()->GetSecurity() < SEC_ADMINISTRATOR)
        {
            messages.push_back("account list: not allowed");
            return;
        }

        string filter = TrimPlayerbotCommandParam(param);
        if (!filter.empty() && !IsSafePlayerbotAccountToken(filter))
        {
            messages.push_back("account list: use letters, numbers, or underscore for filter");
            return;
        }

        if (!filter.empty())
            Utf8ToUpperOnlyLatin(filter);

        ostringstream query;
        query << "SELECT id,username FROM account";
        if (!filter.empty())
            query << " WHERE username LIKE '" << filter << "%'";
        query << " ORDER BY username LIMIT 200";

        QueryResult* results = LoginDatabase.Query(query.str().c_str());
        if (!results)
        {
            messages.push_back("account list: none");
            return;
        }

        messages.push_back("account list: begin");
        bool hasAccount = false;
        do
        {
            Field* fields = results->Fetch();
            uint32 accountId = fields[0].GetUInt32();
            uint32 characterCount = 0;
            QueryResult* characterCountResult = CharacterDatabase.PQuery(
                    "SELECT COUNT(*) FROM characters WHERE account = '%u'",
                    accountId);
            if (characterCountResult)
            {
                characterCount = (*characterCountResult)[0].GetUInt32();
                delete characterCountResult;
            }

            ostringstream line;
            line << "account name: " << fields[1].GetString() << " " << characterCount;
            messages.push_back(line.str());
            hasAccount = true;
        } while (results->NextRow());

        delete results;
        if (!hasAccount)
            messages.push_back("account list: none");
        messages.push_back("account list: end");
    }

    uint32 GetPlayerbotRoundedMasterLevel(Player* master)
    {
        uint32 maxLevel = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);
        uint32 level = master ? master->getLevel() : maxLevel;
        uint32 rounded = ((level + 9) / 10) * 10;
        if (rounded < 10)
            rounded = 10;
        if (rounded > maxLevel)
            rounded = maxLevel;
        return rounded;
    }

    void AppendPlayerbotLoginCharacterMessages(list<string>& messages, Player* master, const string& param)
    {
        if (!master || !master->GetSession() ||
                master->GetSession()->GetSecurity() < SEC_ADMINISTRATOR)
        {
            messages.push_back("character list: not allowed");
            return;
        }

        string accountName = TrimPlayerbotCommandParam(param);
        if (!IsSafePlayerbotAccountToken(accountName))
        {
            messages.push_back("character list: choose an account");
            return;
        }

        Utf8ToUpperOnlyLatin(accountName);
        uint32 accountId = sAccountMgr.GetId(accountName);
        if (!accountId)
        {
            messages.push_back("character list: account does not exist");
            return;
        }

        QueryResult* results = CharacterDatabase.PQuery(
                "SELECT name,class,level FROM characters WHERE account = '%u' ORDER BY name LIMIT 30",
                accountId);
        if (!results)
        {
            messages.push_back(string("character list ") + accountName + ": none");
            return;
        }

        messages.push_back(string("character list ") + accountName + ": begin");
        do
        {
            Field* fields = results->Fetch();
            ostringstream line;
            line << "character name " << accountName << ": "
                    << fields[0].GetString() << "/"
                    << GetPlayerbotClassName(fields[1].GetUInt8()) << "/"
                    << uint32(fields[2].GetUInt8());
            messages.push_back(line.str());
        } while (results->NextRow());

        delete results;
        messages.push_back(string("character list ") + accountName + ": end");
    }

    string CreatePlayerbotCharacterForAccount(Player* master, uint32 accountId, const string& accountName,
            const string& rawName, const string& rawParams, bool loginAfterCreate,
            ObjectGuid& createdGuid, string& roleCommand, uint32& initQuality)
    {
        createdGuid = ObjectGuid();
        roleCommand.clear();
        initQuality = ITEM_QUALITY_RARE;

        if (!master)
            return "character create: no active master";

        MarkPlayerbotLoginAccount(accountId);

        string name = TrimPlayerbotCommandParam(rawName);
        if (name.empty())
            return "usage: account character ACCOUNT NAME class [race] [gender] [role] [quality]";

        if (!normalizePlayerName(name) || ObjectMgr::CheckPlayerName(name, true) != CHAR_NAME_SUCCESS)
            return "character create: invalid character name";

        QueryResult* existing = CharacterDatabase.PQuery("SELECT guid FROM characters WHERE name = '%s'", name.c_str());
        if (existing)
        {
            delete existing;
            return "character create: name already exists";
        }

        QueryResult* characterCount = CharacterDatabase.PQuery("SELECT COUNT(*) FROM characters WHERE account = '%u'", accountId);
        if (characterCount)
        {
            uint32 count = characterCount->Fetch()[0].GetUInt32();
            delete characterCount;
            if (count >= 10)
                return "character create: account already has 10 characters";
        }

        vector<string> tokens;
        split(tokens, rawParams, " \t\r\n");
        if (tokens.empty())
            return "usage: account character ACCOUNT NAME class [race] [gender] [role] [quality]";

        uint8 cls = GetPlayerbotClassId(tokens[0]);
        if (!cls)
            return "character create: unknown class";

        uint8 race = 0;
        int32 genderOption = -1;
        for (uint32 i = 1; i < tokens.size(); ++i)
        {
            uint8 parsedRace = GetPlayerbotRaceId(tokens[i]);
            if (parsedRace)
            {
                race = parsedRace;
                continue;
            }

            int32 parsedGender = GetPlayerbotGenderId(tokens[i]);
            if (parsedGender != -1)
            {
                genderOption = parsedGender;
                continue;
            }

            string parsedRole = BuildPlayerbotRoleCommand(tokens[i]);
            if (!parsedRole.empty())
            {
                roleCommand = parsedRole;
                continue;
            }

            uint32 parsedQuality = GetPlayerbotItemQuality(tokens[i]);
            if (parsedQuality)
            {
                initQuality = parsedQuality;
                continue;
            }

            return string("character create: unknown option ") + tokens[i];
        }

        if (!race)
            race = ChoosePlayerbotRace(master, cls);

        if (!race)
            return "character create: class is not available for your faction";

        if (!IsPlayerbotRaceClassAllowed(race, cls))
            return "character create: race cannot be that class";

        if (Player::TeamForRace(race) != master->GetTeam())
            return "character create: opposing faction";

        uint8 gender = genderOption == GENDER_MALE || genderOption == GENDER_FEMALE
            ? uint8(genderOption) : uint8(urand(0, 1) ? GENDER_MALE : GENDER_FEMALE);
        uint8 skinColor = urand(0, 7);
        uint8 face = urand(0, 7);
        uint8 hairStyle = urand(0, 7);
        uint8 hairColor = urand(0, 7);
        uint8 facialHair = urand(0, 7);

        WorldSession* session = new WorldSession(accountId, NULL, SEC_PLAYER,
#ifdef MANGOSBOT_ONE
            1,
#endif
#ifdef MANGOSBOT_TWO
            2,
#endif
            0, LOCALE_enUS);

        Player* player = new Player(session);
        if (!player->Create(sObjectMgr.GeneratePlayerLowGuid(), name, race, cls, gender,
                skinColor, face, hairStyle, hairColor, facialHair, 0))
        {
            delete player;
            delete session;
            return "character create: character creation failed";
        }

        player->setCinematic(2);
        player->SetAtLoginFlag(AT_LOGIN_NONE);
        player->SaveToDB();
        createdGuid = player->GetObjectGuid();

        delete player;
        delete session;

        ostringstream out;
        out << "character create: " << accountName << "/" << name << " - ok (" << GetPlayerbotClassName(cls) << ")";
        if (loginAfterCreate)
            out << ", logging in";
        return out.str();
    }

    string CreatePlayerbotLoginCharacter(Player* master, const string& param,
            ObjectGuid& createdGuid, string& roleCommand, uint32& initQuality)
    {
        createdGuid = ObjectGuid();
        roleCommand.clear();
        initQuality = ITEM_QUALITY_RARE;

        if (!master || !master->GetSession() ||
                master->GetSession()->GetSecurity() < SEC_ADMINISTRATOR)
            return "character create: not allowed";

        string accountName, characterName, params;
        if (!ParseAccountCharacterParams(param, accountName, characterName, params))
            return "usage: account character ACCOUNT NAME class [race] [gender] [role] [quality]";

        if (!IsSafePlayerbotAccountToken(accountName))
            return "character create: use 1-16 letters, numbers, or underscore for account";

        Utf8ToUpperOnlyLatin(accountName);
        uint32 accountId = sAccountMgr.GetId(accountName);
        if (!accountId)
            return "character create: account does not exist";

        return CreatePlayerbotCharacterForAccount(master, accountId, accountName, characterName, params, false,
                createdGuid, roleCommand, initQuality);
    }

    string QuickCreatePlayerbotLoginCharacter(Player* master, const string& param,
            ObjectGuid& createdGuid, string& roleCommand, uint32& initQuality, uint32& initLevel)
    {
        createdGuid = ObjectGuid();
        roleCommand.clear();
        initQuality = ITEM_QUALITY_EPIC;
        initLevel = GetPlayerbotRoundedMasterLevel(master);

        if (!master || !master->GetSession() ||
                master->GetSession()->GetSecurity() < SEC_ADMINISTRATOR)
            return "quick create: not allowed";

        string accountName, characterName, params;
        if (!ParseAccountCharacterParams(param, accountName, characterName, params))
            return "usage: account quick ACCOUNT NAME class [gender] [role]";

        if (!IsSafePlayerbotAccountToken(accountName))
            return "quick create: use 1-16 letters, numbers, or underscore for account";

        Utf8ToUpperOnlyLatin(accountName);
        uint32 accountId = sAccountMgr.GetId(accountName);
        if (!accountId)
            return "quick create: account does not exist";

        string quickParams = params + " epic";
        string result = CreatePlayerbotCharacterForAccount(master, accountId, accountName, characterName,
                quickParams, true, createdGuid, roleCommand, initQuality);
        if (!createdGuid.IsEmpty())
        {
            CharacterDatabase.PExecute("UPDATE characters SET level = '%u', xp = 0 WHERE guid = '%u'",
                    initLevel, createdGuid.GetCounter());
            ostringstream out;
            out << result << ", level " << initLevel << ", epic gear, online";
            return out.str();
        }

        return result;
    }

    bool GetPlayerbotClassFilter(const string& filter, vector<uint32>& classes)
    {
        string name = LowerPlayerbotCommandParam(TrimPlayerbotCommandParam(filter));
        if (name.empty())
            return false;

        if (name == "tank")
        {
            classes.push_back(CLASS_WARRIOR);
            classes.push_back(CLASS_PALADIN);
            classes.push_back(CLASS_DRUID);
            return true;
        }

        if (name == "heal" || name == "healer")
        {
            classes.push_back(CLASS_DRUID);
            classes.push_back(CLASS_PALADIN);
            classes.push_back(CLASS_PRIEST);
            classes.push_back(CLASS_SHAMAN);
            return true;
        }

        if (name == "dps" || name == "damage")
        {
            classes.push_back(CLASS_WARRIOR);
            classes.push_back(CLASS_PALADIN);
            classes.push_back(CLASS_HUNTER);
            classes.push_back(CLASS_ROGUE);
            classes.push_back(CLASS_PRIEST);
            classes.push_back(CLASS_SHAMAN);
            classes.push_back(CLASS_MAGE);
            classes.push_back(CLASS_WARLOCK);
            classes.push_back(CLASS_DRUID);
            return true;
        }

        if (name == "warrior") classes.push_back(CLASS_WARRIOR);
        else if (name == "paladin") classes.push_back(CLASS_PALADIN);
        else if (name == "hunter") classes.push_back(CLASS_HUNTER);
        else if (name == "rogue") classes.push_back(CLASS_ROGUE);
        else if (name == "priest") classes.push_back(CLASS_PRIEST);
        else if (name == "shaman") classes.push_back(CLASS_SHAMAN);
        else if (name == "mage") classes.push_back(CLASS_MAGE);
        else if (name == "warlock") classes.push_back(CLASS_WARLOCK);
        else if (name == "druid") classes.push_back(CLASS_DRUID);

        return !classes.empty();
    }

    uint8 GetPlayerbotClassId(const string& name)
    {
        string cls = LowerPlayerbotCommandParam(TrimPlayerbotCommandParam(name));
        if (cls == "warrior" || cls == "war") return CLASS_WARRIOR;
        if (cls == "paladin" || cls == "pala" || cls == "pally") return CLASS_PALADIN;
        if (cls == "hunter" || cls == "hunt") return CLASS_HUNTER;
        if (cls == "rogue") return CLASS_ROGUE;
        if (cls == "priest") return CLASS_PRIEST;
        if (cls == "shaman" || cls == "sham") return CLASS_SHAMAN;
        if (cls == "mage") return CLASS_MAGE;
        if (cls == "warlock" || cls == "lock") return CLASS_WARLOCK;
        if (cls == "druid") return CLASS_DRUID;
        return 0;
    }

    uint8 GetPlayerbotRaceId(const string& name)
    {
        string race = LowerPlayerbotCommandParam(TrimPlayerbotCommandParam(name));
        if (race == "human") return RACE_HUMAN;
        if (race == "orc") return RACE_ORC;
        if (race == "dwarf") return RACE_DWARF;
        if (race == "nightelf" || race == "night-elf" || race == "night_elf" || race == "ne") return RACE_NIGHTELF;
        if (race == "undead" || race == "forsaken") return RACE_UNDEAD;
        if (race == "tauren") return RACE_TAUREN;
        if (race == "gnome") return RACE_GNOME;
        if (race == "troll") return RACE_TROLL;
        return 0;
    }

    uint32 GetPlayerbotItemQuality(const string& name)
    {
        string quality = LowerPlayerbotCommandParam(TrimPlayerbotCommandParam(name));
        if (quality == "white" || quality == "common") return ITEM_QUALITY_NORMAL;
        if (quality == "green" || quality == "uncommon") return ITEM_QUALITY_UNCOMMON;
        if (quality == "blue" || quality == "rare") return ITEM_QUALITY_RARE;
        if (quality == "epic" || quality == "purple") return ITEM_QUALITY_EPIC;
        return 0;
    }

    string GetPlayerbotItemQualityName(uint32 quality)
    {
        switch (quality)
        {
        case ITEM_QUALITY_NORMAL: return "white";
        case ITEM_QUALITY_UNCOMMON: return "green";
        case ITEM_QUALITY_RARE: return "blue";
        case ITEM_QUALITY_EPIC: return "epic";
        default: return "auto";
        }
    }

    bool ParsePlayerbotUInt32(const string& text, uint32& value)
    {
        string number = TrimPlayerbotCommandParam(text);
        if (number.empty())
            return false;

        for (string::const_iterator i = number.begin(); i != number.end(); ++i)
        {
            if (!std::isdigit(static_cast<unsigned char>(*i)))
                return false;
        }

        value = uint32(atoi(number.c_str()));
        return true;
    }

    bool ParsePlayerbotLevelGearParams(const string& input, bool requiresLevel, string& characterName,
            bool& hasLevel, uint32& level, uint32& quality, string& role, string& error)
    {
        characterName.clear();
        hasLevel = false;
        level = 0;
        quality = ITEM_QUALITY_RARE;
        role = "dps";
        error.clear();

        vector<string> tokens;
        split(tokens, input, " \t\r\n");
        if (tokens.empty() || (requiresLevel && tokens.size() < 2))
        {
            error = requiresLevel ? "usage: account level CHARACTER LEVEL [quality]"
                : "usage: account gear CHARACTER [quality] [level]";
            return false;
        }

        characterName = tokens[0];
        if (!normalizePlayerName(characterName))
        {
            error = "account gear: invalid character name";
            return false;
        }

        uint32 optionStart = 1;
        if (requiresLevel)
        {
            if (!ParsePlayerbotUInt32(tokens[1], level))
            {
                error = "account level: level must be a number";
                return false;
            }

            hasLevel = true;
            optionStart = 2;
        }

        for (uint32 i = optionStart; i < tokens.size(); ++i)
        {
            string parsedRole = LowerPlayerbotCommandParam(tokens[i]);
            if (!requiresLevel && (parsedRole == "heal" || parsedRole == "healer" ||
                        parsedRole == "dps" || parsedRole == "damage" || parsedRole == "tank"))
            {
                role = (parsedRole == "healer") ? "heal" : (parsedRole == "damage" ? "dps" : parsedRole);
                continue;
            }

            uint32 parsedQuality = GetPlayerbotItemQuality(tokens[i]);
            if (parsedQuality)
            {
                quality = parsedQuality;
                continue;
            }

            uint32 parsedLevel = 0;
            if (!requiresLevel && ParsePlayerbotUInt32(tokens[i], parsedLevel))
            {
                level = parsedLevel;
                hasLevel = true;
                continue;
            }

            error = string(requiresLevel ? "account level: unknown option " : "account gear: unknown option ") + tokens[i];
            return false;
        }

        uint32 maxLevel = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);
        if (hasLevel && (level < 1 || level > maxLevel))
        {
            ostringstream out;
            out << (requiresLevel ? "account level" : "account gear")
                << ": level must be between 1 and " << maxLevel;
            error = out.str();
            return false;
        }

        return true;
    }

    string SchedulePlayerbotLevelGear(Player* master, const string& action, const string& param,
            bool requiresLevel, ObjectGuid& pendingGuid, bool& pendingHasLevel,
            uint32& pendingLevel, uint32& pendingQuality)
    {
        pendingGuid = ObjectGuid();
        pendingHasLevel = false;
        pendingLevel = 0;
        pendingQuality = ITEM_QUALITY_RARE;

        if (!master || !master->GetSession() ||
                master->GetSession()->GetSecurity() < SEC_ADMINISTRATOR)
            return string("account ") + action + ": not allowed";

        string characterName;
        string pendingRole;
        string error;
        if (!ParsePlayerbotLevelGearParams(param, requiresLevel, characterName,
                pendingHasLevel, pendingLevel, pendingQuality, pendingRole, error))
            return error;

        ObjectGuid guid = sObjectMgr.GetPlayerGuidByName(characterName);
        if (guid.IsEmpty())
            return string("account ") + action + ": character not found";

        if (guid.GetRawValue() == master->GetObjectGuid().GetRawValue())
            return string("account ") + action + ": use normal GM commands for yourself";

        Player* online = sObjectMgr.GetPlayer(guid);
        if (online)
        {
            if (!online->GetPlayerbotAI())
                return string("account ") + action + ": character is online";

            uint32 targetLevel = pendingHasLevel ? pendingLevel : online->getLevel();
            PlayerbotFactory factory(online, targetLevel, pendingQuality);
            factory.GearOnly(pendingRole);

            ostringstream out;
            out << "account " << action << ": " << characterName << " - ok";
            out << " (level " << targetLevel << ", " << GetPlayerbotItemQualityName(pendingQuality) << " " << pendingRole << " gear)";
            return out.str();
        }

        return string("account ") + action + ": bring the bot online first, then gear it";
    }

    string ApplyPlayerbotCharacterLevel(Player* master, const string& param)
    {
        if (!master || !master->GetSession() ||
                master->GetSession()->GetSecurity() < SEC_ADMINISTRATOR)
            return "account level: not allowed";

        string characterName;
        bool hasLevel = false;
        uint32 targetLevel = 0;
        uint32 ignoredQuality = ITEM_QUALITY_RARE;
        string ignoredRole;
        string error;
        if (!ParsePlayerbotLevelGearParams(param, true, characterName, hasLevel, targetLevel, ignoredQuality, ignoredRole, error))
            return error;

        ObjectGuid guid = sObjectMgr.GetPlayerGuidByName(characterName);
        if (guid.IsEmpty())
            return "account level: character not found";

        uint32 oldLevel = 0;
        QueryResult* results = CharacterDatabase.PQuery("SELECT level FROM characters WHERE guid = '%u'", guid.GetCounter());
        if (results)
        {
            oldLevel = results->Fetch()[0].GetUInt32();
            delete results;
        }

        Player* online = sObjectMgr.GetPlayer(guid);
        if (online)
        {
            oldLevel = online->getLevel();
            online->GiveLevel(targetLevel);
            online->InitTalentForLevel();
            online->SetUInt32Value(PLAYER_XP, 0);
            PlayerbotFactory factory(online, targetLevel);
            factory.TrainForLevel();
            online->SaveToDB();
        }
        else
        {
            CharacterDatabase.PExecute("UPDATE characters SET level = '%u', xp = 0 WHERE guid = '%u'",
                    targetLevel, guid.GetCounter());
        }

        ostringstream out;
        out << "account level: " << characterName << " - ok";
        if (oldLevel)
            out << " (" << oldLevel << " -> " << targetLevel << ")";
        else
            out << " (level " << targetLevel << ")";
        out << ", gear unchanged";
        if (online)
            out << ", spells trained";
        else
            out << ", bring online and use gear/tune to train spells";
        return out.str();
    }

    void GetAvailablePlayerbotRaces(uint8 cls, vector<uint8>& races)
    {
        switch (cls)
        {
        case CLASS_WARRIOR:
            races.push_back(RACE_HUMAN); races.push_back(RACE_NIGHTELF); races.push_back(RACE_GNOME); races.push_back(RACE_DWARF);
            races.push_back(RACE_ORC); races.push_back(RACE_UNDEAD); races.push_back(RACE_TAUREN); races.push_back(RACE_TROLL);
            break;
        case CLASS_PALADIN:
            races.push_back(RACE_HUMAN); races.push_back(RACE_DWARF);
            break;
        case CLASS_HUNTER:
            races.push_back(RACE_DWARF); races.push_back(RACE_NIGHTELF);
            races.push_back(RACE_ORC); races.push_back(RACE_TAUREN); races.push_back(RACE_TROLL);
            break;
        case CLASS_ROGUE:
            races.push_back(RACE_HUMAN); races.push_back(RACE_DWARF); races.push_back(RACE_NIGHTELF); races.push_back(RACE_GNOME);
            races.push_back(RACE_ORC); races.push_back(RACE_TROLL);
            break;
        case CLASS_PRIEST:
            races.push_back(RACE_HUMAN); races.push_back(RACE_DWARF); races.push_back(RACE_NIGHTELF);
            races.push_back(RACE_TROLL); races.push_back(RACE_UNDEAD);
            break;
        case CLASS_SHAMAN:
            races.push_back(RACE_ORC); races.push_back(RACE_TAUREN); races.push_back(RACE_TROLL);
            break;
        case CLASS_MAGE:
            races.push_back(RACE_HUMAN); races.push_back(RACE_GNOME);
            races.push_back(RACE_UNDEAD); races.push_back(RACE_TROLL);
            break;
        case CLASS_WARLOCK:
            races.push_back(RACE_HUMAN); races.push_back(RACE_GNOME);
            races.push_back(RACE_UNDEAD); races.push_back(RACE_ORC);
            break;
        case CLASS_DRUID:
            races.push_back(RACE_NIGHTELF); races.push_back(RACE_TAUREN);
            break;
        }
    }

    bool IsPlayerbotRaceClassAllowed(uint8 race, uint8 cls)
    {
        vector<uint8> races;
        GetAvailablePlayerbotRaces(cls, races);
        return find(races.begin(), races.end(), race) != races.end();
    }

    uint8 ChoosePlayerbotRace(Player* master, uint8 cls)
    {
        vector<uint8> races;
        GetAvailablePlayerbotRaces(cls, races);

        vector<uint8> filtered;
        for (vector<uint8>::iterator i = races.begin(); i != races.end(); ++i)
        {
            if (!master || Player::TeamForRace(*i) == master->GetTeam())
                filtered.push_back(*i);
        }

        if (filtered.empty())
            return 0;

        return filtered[urand(0, filtered.size() - 1)];
    }

    string CreatePlayerbotCharacter(Player* master, const string& rawName, const string& rawParams,
            ObjectGuid& createdGuid, string& roleCommand, uint32& initQuality)
    {
        createdGuid = ObjectGuid();
        roleCommand.clear();
        initQuality = ITEM_QUALITY_RARE;

        if (!master)
            return "create: no active master";

        string name = TrimPlayerbotCommandParam(rawName);
        if (name.empty())
            return "usage: create NAME class [race] [heal|dps|tank] [green|blue|epic]";

        if (!normalizePlayerName(name) || ObjectMgr::CheckPlayerName(name, true) != CHAR_NAME_SUCCESS)
            return "create: invalid character name";

        QueryResult* existing = CharacterDatabase.PQuery("SELECT guid FROM characters WHERE name = '%s'", name.c_str());
        if (existing)
        {
            delete existing;
            return "create: name already exists";
        }

        vector<string> tokens;
        split(tokens, rawParams, " \t\r\n");
        if (tokens.empty())
            return "usage: create NAME class [race] [heal|dps|tank] [green|blue|epic]";

        uint8 cls = GetPlayerbotClassId(tokens[0]);
        if (!cls)
            return "create: unknown class";

        uint8 race = 0;
        for (uint32 i = 1; i < tokens.size(); ++i)
        {
            uint8 parsedRace = GetPlayerbotRaceId(tokens[i]);
            if (parsedRace)
            {
                race = parsedRace;
                continue;
            }

            string parsedRole = BuildPlayerbotRoleCommand(tokens[i]);
            if (!parsedRole.empty())
            {
                roleCommand = parsedRole;
                continue;
            }

            uint32 parsedQuality = GetPlayerbotItemQuality(tokens[i]);
            if (parsedQuality)
            {
                initQuality = parsedQuality;
                continue;
            }

            return string("create: unknown option ") + tokens[i];
        }

        if (!race)
            race = ChoosePlayerbotRace(master, cls);

        if (!race)
            return "create: class is not available for your faction";

        if (!IsPlayerbotRaceClassAllowed(race, cls))
            return "create: race cannot be that class";

        if (Player::TeamForRace(race) != master->GetTeam())
            return "create: opposing faction";

        uint32 accountId = SelectRandomPlayerbotAccount();
        if (!accountId)
            return "create: no bot account pool is configured";

        uint8 gender = urand(0, 1) ? GENDER_MALE : GENDER_FEMALE;
        uint8 skinColor = urand(0, 7);
        uint8 face = urand(0, 7);
        uint8 hairStyle = urand(0, 7);
        uint8 hairColor = urand(0, 7);
        uint8 facialHair = urand(0, 7);

        WorldSession* session = new WorldSession(accountId, NULL, SEC_PLAYER,
#ifdef MANGOSBOT_ONE
            1,
#endif
#ifdef MANGOSBOT_TWO
            2,
#endif
            0, LOCALE_enUS);

        Player* player = new Player(session);
        if (!player->Create(sObjectMgr.GeneratePlayerLowGuid(), name, race, cls, gender,
                skinColor, face, hairStyle, hairColor, facialHair, 0))
        {
            delete player;
            delete session;
            return "create: character creation failed";
        }

        player->setCinematic(2);
        player->SetAtLoginFlag(AT_LOGIN_NONE);
        player->SaveToDB();
        createdGuid = player->GetObjectGuid();
        MarkPlayerbotLoginAccount(accountId);

        delete player;
        delete session;

        ostringstream out;
        out << "create: " << name << " - ok (" << GetPlayerbotClassName(cls) << "), logging in";
        return out.str();
    }

    void AppendRandomBotPoolFilters(ostringstream& query, Player* master, const string& filter)
    {
        if (master && master->GetSession()->GetSecurity() < SEC_GAMEMASTER)
        {
            if (master->GetTeam() == ALLIANCE)
                query << " AND race IN (1,3,4,7)";
            else if (master->GetTeam() == HORDE)
                query << " AND race IN (2,5,6,8)";
        }

        vector<uint32> classes;
        if (GetPlayerbotClassFilter(filter, classes))
        {
            query << " AND class IN (" << BuildPlayerbotUIntList(classes) << ")";
            return;
        }

        string prefix = TrimPlayerbotCommandParam(filter);
        if (!prefix.empty() && normalizePlayerName(prefix))
            query << " AND name LIKE '" << prefix << "%'";
    }

    string FindRandomPlayerbotName(Player* master, const string& filter, string* reason = NULL)
    {
        string accounts = BuildRandomBotAccountList();
        if (accounts.empty())
        {
            if (reason) *reason = "random bot pool is empty";
            return "";
        }

        ostringstream query;
        query << "SELECT name FROM characters WHERE account IN (" << accounts << ") AND online = 0";
        AppendRandomBotPoolFilters(query, master, filter);
        query << " ORDER BY RAND() LIMIT 1";

        QueryResult* results = CharacterDatabase.Query(query.str().c_str());
        if (!results)
        {
            if (reason) *reason = filter.empty() ? "no available random bots" : "no available random bots matching filter";
            return "";
        }

        string name = results->Fetch()[0].GetString();
        delete results;
        return name;
    }

    string ListRandomPlayerbotPool(Player* master, const string& filter)
    {
        string accounts = BuildRandomBotAccountList();
        if (accounts.empty())
            return "random bot pool is empty";

        ostringstream query;
        query << "SELECT name,class,level FROM characters WHERE account IN (" << accounts << ") AND online = 0";
        AppendRandomBotPoolFilters(query, master, filter);
        query << " ORDER BY name LIMIT 30";

        QueryResult* results = CharacterDatabase.Query(query.str().c_str());
        if (!results)
            return "Random bot pool: no matching offline bots";

        ostringstream out;
        bool first = true;
        out << "Random bot pool: ";
        do
        {
            Field* fields = results->Fetch();
            if (first) first = false; else out << ", ";
            out << fields[0].GetString() << " " << GetPlayerbotClassName(fields[1].GetUInt8())
                    << " " << fields[2].GetUInt8();
        } while (results->NextRow());

        delete results;
        return out.str();
    }

    string NormalizePlayerbotBossName(const string& param)
    {
        string name = LowerPlayerbotCommandParam(TrimPlayerbotCommandParam(param));
        for (string::iterator i = name.begin(); i != name.end(); ++i)
        {
            if (*i == '-' || *i == '_')
                *i = ' ';
        }

        return name;
    }

    string GetPlayerbotBossPresetList()
    {
        return "boss presets: lucifron, magmadar, gehennas, garr, geddon, shazzrah, ragnaros";
    }

    bool PlayerbotNameSetContains(set<string> const& names, string const& expected)
    {
        string lowerExpected = LowerPlayerbotCommandParam(expected);
        for (set<string>::const_iterator i = names.begin(); i != names.end(); ++i)
        {
            if (LowerPlayerbotCommandParam(*i) == lowerExpected)
                return true;
        }

        return false;
    }

    bool PlayerbotCanCure(Player* bot)
    {
        if (!bot)
            return false;

        switch (bot->getClass())
        {
        case CLASS_DRUID:
        case CLASS_MAGE:
        case CLASS_PALADIN:
        case CLASS_PRIEST:
        case CLASS_SHAMAN:
            return true;
        default:
            return false;
        }
    }

    uint32 CountPlayerbotKnownSpells(Player* bot)
    {
        if (!bot)
            return 0;

        QueryResult* results = CharacterDatabase.PQuery(
            "SELECT COUNT(*) FROM character_spell WHERE guid = '%u'",
            bot->GetObjectGuid().GetCounter());
        if (!results)
            return 0;

        uint32 count = results->Fetch()[0].GetUInt32();
        delete results;
        return count;
    }

    void AddPlayerbotBaseRaidCommands(vector<string>& commands, Player* bot)
    {
        commands.push_back("nc +follow,-grind,-rpg,-runaway,-stay");

        if (bot->GetPlayerbotAI()->IsHeal(bot))
        {
            if (PlayerbotCanCure(bot))
                commands.push_back("co +heal,+cure,-conserve mana,-dps,-tank,-bear");
            else
                commands.push_back("co +heal,-conserve mana,-dps,-tank,-bear");
        }
        else if (bot->GetPlayerbotAI()->IsTank(bot))
        {
            commands.push_back("co +tank,+tank assist,+tank aoe,+bthreat,+threat,-heal,-dps");
        }
        else
        {
            commands.push_back("co +dps,+dps assist,+threat,-heal,-tank,-bear");
        }
    }

    vector<string> BuildPlayerbotBossCommands(const string& param, Player* bot)
    {
        vector<string> commands;
        string boss = NormalizePlayerbotBossName(param);

        if (!bot || !bot->GetPlayerbotAI() || boss.empty() || boss == "list" || boss == "?")
            return commands;

        AddPlayerbotBaseRaidCommands(commands, bot);

        if (boss == "lucifron")
        {
            if (PlayerbotCanCure(bot))
                commands.push_back("co +cure");
        }
        else if (boss == "magmadar")
        {
            commands.push_back("co +flee,+ranged");
            if (bot->getClass() == CLASS_HUNTER)
                commands.push_back("co +dps debuff");
        }
        else if (boss == "gehennas")
        {
            commands.push_back("co +flee,+ranged");
            if (PlayerbotCanCure(bot))
                commands.push_back("co +cure");
        }
        else if (boss == "garr")
        {
            commands.push_back("co +mark rti,+dps assist,-dps aoe,-tank aoe");
        }
        else if (boss == "geddon" || boss == "baron geddon" || boss == "baron")
        {
            commands.push_back("co +flee,+runaway,+ranged,-dps aoe,-tank aoe");
            commands.push_back("nc +follow,-stay");
        }
        else if (boss == "shazzrah")
        {
            commands.push_back("co +ranged,+dps assist,-dps aoe");
            if (PlayerbotCanCure(bot))
                commands.push_back("co +cure");
        }
        else if (boss == "ragnaros" || boss == "rag")
        {
            commands.push_back("co +ranged,+dps assist,+threat,-dps aoe");
            commands.push_back("nc +follow,-stay");
        }
        else
        {
            commands.clear();
        }

        return commands;
    }

    string NormalizePlayerbotPullOpener(const string& opener)
    {
        string normalized = LowerPlayerbotCommandParam(TrimPlayerbotCommandParam(opener));
        if (normalized == "charge")
            return "charge";

        return "shoot";
    }

    bool RefreshPlayerbotRangedAmmo(Player* bot, string* reason)
    {
        if (!bot)
        {
            if (reason) *reason = "no bot";
            return false;
        }

        Item* ranged = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED);
        if (!ranged || !ranged->GetProto())
        {
            if (reason) *reason = "no ranged weapon equipped";
            return false;
        }

        uint32 weaponSubClass = ranged->GetProto()->SubClass;
        if (weaponSubClass != ITEM_SUBCLASS_WEAPON_GUN &&
                weaponSubClass != ITEM_SUBCLASS_WEAPON_BOW &&
                weaponSubClass != ITEM_SUBCLASS_WEAPON_CROSSBOW &&
                weaponSubClass != ITEM_SUBCLASS_WEAPON_THROWN)
        {
            if (reason) *reason = "equipped ranged item cannot shoot";
            return false;
        }

        if (weaponSubClass == ITEM_SUBCLASS_WEAPON_THROWN)
        {
            if (!bot->HasItemCount(ranged->GetEntry(), 1))
            {
                if (reason) *reason = "no thrown weapon stack available";
                return false;
            }

            return true;
        }

        uint32 ammoSubClass = weaponSubClass == ITEM_SUBCLASS_WEAPON_GUN ? ITEM_SUBCLASS_BULLET : ITEM_SUBCLASS_ARROW;
        uint32 ammo = bot->GetUInt32Value(PLAYER_AMMO_ID);
        ItemPrototype const* ammoProto = ammo ? sObjectMgr.GetItemPrototype(ammo) : NULL;
        if (!ammoProto || ammoProto->Class != ITEM_CLASS_PROJECTILE || ammoProto->SubClass != ammoSubClass ||
                !bot->HasItemCount(ammo, 1))
        {
            ammo = sRandomItemMgr.GetAmmo(bot->getLevel(), ammoSubClass);
            ammoProto = ammo ? sObjectMgr.GetItemPrototype(ammo) : NULL;
            if (!ammoProto || ammoProto->Class != ITEM_CLASS_PROJECTILE || ammoProto->SubClass != ammoSubClass)
            {
                if (reason) *reason = "no matching ammo available";
                return false;
            }

            Item* newItem = bot->StoreNewItemInInventorySlot(ammo, 200);
            if (newItem)
                newItem->AddToUpdateQueueOf(bot);
            bot->SetAmmo(ammo);
        }

        if (!bot->HasItemCount(ammo, 1))
        {
            if (reason) *reason = "selected ammo is not in bags";
            return false;
        }

        return true;
    }

    uint32 GetPlayerbotShootPullSpell(Player* bot, string* reason)
    {
        Item* ranged = bot ? bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED) : NULL;
        if (!ranged || !ranged->GetProto())
        {
            if (reason) *reason = "no ranged weapon equipped";
            return 0;
        }

        switch (ranged->GetProto()->SubClass)
        {
        case ITEM_SUBCLASS_WEAPON_BOW:
            return 2480; // Shoot Bow
        case ITEM_SUBCLASS_WEAPON_GUN:
            return 7918; // Shoot Gun
        case ITEM_SUBCLASS_WEAPON_CROSSBOW:
            return 7919; // Shoot Crossbow
        case ITEM_SUBCLASS_WEAPON_THROWN:
            return 2764; // Throw
        default:
            if (reason) *reason = "equipped ranged item cannot shoot";
            return 0;
        }
    }

    enum PlayerbotShootPullResult
    {
        PLAYERBOT_SHOOT_PULL_FAILED,
        PLAYERBOT_SHOOT_PULL_STARTED,
        PLAYERBOT_SHOOT_PULL_PENDING
    };

    float GetPlayerbotShootPullRange(Player* bot)
    {
        if (!bot || !bot->GetPlayerbotAI())
            return sPlayerbotAIConfig.shootDistance;

        float range = bot->GetPlayerbotAI()->GetRange("shoot");
        return range > 0.0f ? range : sPlayerbotAIConfig.shootDistance;
    }

    float GetPlayerbotShootPullMaxRange(Player* bot, SpellEntry const* spellInfo)
    {
        float range = GetPlayerbotShootPullRange(bot);
        if (!spellInfo)
            return range;

        float spellRange = GetSpellMaxRange(sSpellRangeStore.LookupEntry(spellInfo->rangeIndex));
        if (spellRange > 0.0f && spellRange < range)
            return spellRange;

        return range;
    }

    float GetPlayerbotShootPullMinRange(SpellEntry const* spellInfo)
    {
        if (!spellInfo)
            return sPlayerbotAIConfig.tooCloseDistance;

        float range = GetSpellMinRange(sSpellRangeStore.LookupEntry(spellInfo->rangeIndex));
        return range > 0.0f ? range : sPlayerbotAIConfig.tooCloseDistance;
    }

    void PreparePlayerbotShootPullCast(Player* bot, Unit* target)
    {
        if (!bot || !target)
            return;

        if (!bot->IsStandState())
            bot->SetStandState(UNIT_STAND_STATE_STAND);

        if (sServerFacade.isMoving(bot))
            bot->InterruptMoving();

        if (!sServerFacade.IsInFront(bot, target, sPlayerbotAIConfig.sightDistance, CAST_ANGLE_IN_FRONT))
            sServerFacade.SetFacingTo(bot, target, true);
    }

    bool MovePlayerbotShootPullIntoRange(Player* bot, Unit* target, float shootRange, string* reason)
    {
        if (!bot || !target || !bot->GetPlayerbotAI())
            return false;

        float desiredDistance = shootRange - 1.0f;
        if (desiredDistance <= sPlayerbotAIConfig.tooCloseDistance)
            desiredDistance = sPlayerbotAIConfig.tooCloseDistance + sPlayerbotAIConfig.contactDistance;

        bot->clearUnitState(UNIT_STAT_CHASE);
        bot->clearUnitState(UNIT_STAT_FOLLOW);

        if (!bot->IsStandState())
            bot->SetStandState(UNIT_STAND_STATE_STAND);

        if (bot->IsNonMeleeSpellCasted(true))
        {
            bot->CastStop();
            bot->GetPlayerbotAI()->InterruptSpell();
        }

        float preferredAngle = target->GetAngle(bot);
        for (float angle = preferredAngle; angle <= preferredAngle + 2.0f * M_PI_F; angle += M_PI_F / 4.0f)
        {
            float x = target->GetPositionX();
            float y = target->GetPositionY();
            float z = target->GetPositionZ();
            target->GetNearPoint(bot, x, y, z, bot->GetObjectBoundingRadius(), desiredDistance, angle);
            bot->UpdateAllowedPositionZ(x, y, z);

            if (!target->IsWithinLOS(x, y, z))
                continue;

            bot->GetMotionMaster()->MovePoint(target->GetMapId(), x, y, z, true);
            sServerFacade.SetFacingTo(bot, target, true);
            bot->GetPlayerbotAI()->SetNextCheckDelay(sPlayerbotAIConfig.reactDelay);
            if (reason) *reason = "moving into shoot range";
            return true;
        }

        if (reason) *reason = "no line of sight to shoot range";
        return false;
    }

    PlayerbotShootPullResult CastPlayerbotShootPull(Player* bot, Unit* target, string* reason)
    {
        if (!bot || !bot->GetPlayerbotAI())
        {
            if (reason) *reason = "bot is not online";
            return PLAYERBOT_SHOOT_PULL_FAILED;
        }

        if (!target || !bot->GetMap() || target->GetMapId() != bot->GetMapId())
        {
            if (reason) *reason = "pull target is not near bot";
            return PLAYERBOT_SHOOT_PULL_FAILED;
        }

        if (!RefreshPlayerbotRangedAmmo(bot, reason))
            return PLAYERBOT_SHOOT_PULL_FAILED;

        uint32 shootSpellId = GetPlayerbotShootPullSpell(bot, reason);
        if (!shootSpellId)
            return PLAYERBOT_SHOOT_PULL_FAILED;

        SpellEntry const* shootSpellInfo = sServerFacade.LookupSpellInfo(shootSpellId);
        if (!shootSpellInfo)
        {
            if (reason) *reason = "shoot spell is unavailable";
            return PLAYERBOT_SHOOT_PULL_FAILED;
        }

        float distance = sServerFacade.GetDistance2d(bot, target);
        float shootRange = GetPlayerbotShootPullMaxRange(bot, shootSpellInfo);
        if (sServerFacade.IsDistanceGreaterThan(distance, shootRange))
        {
            if (!MovePlayerbotShootPullIntoRange(bot, target, shootRange, reason))
                return PLAYERBOT_SHOOT_PULL_FAILED;

            return PLAYERBOT_SHOOT_PULL_PENDING;
        }

        if (sServerFacade.IsDistanceLessOrEqualThan(distance, GetPlayerbotShootPullMinRange(shootSpellInfo)))
        {
            bot->GetPlayerbotAI()->DoSpecificAction("attack my target");
            if (reason) *reason = "target too close for shoot; attacking";
            return PLAYERBOT_SHOOT_PULL_STARTED;
        }

        if (!bot->IsWithinLOSInMap(target))
        {
            if (!MovePlayerbotShootPullIntoRange(bot, target, shootRange, reason))
                return PLAYERBOT_SHOOT_PULL_FAILED;

            if (reason) *reason = "moving to line up shoot pull";
            return PLAYERBOT_SHOOT_PULL_PENDING;
        }

        PreparePlayerbotShootPullCast(bot, target);

        if (!bot->GetPlayerbotAI()->CastSpell(shootSpellId, target))
        {
            if (reason) *reason = "shoot cast failed";
            return PLAYERBOT_SHOOT_PULL_FAILED;
        }

        return PLAYERBOT_SHOOT_PULL_STARTED;
    }

    void StartPlayerbotTankMeleePull(Player* bot, Unit* target)
    {
        if (!bot || !target || !bot->GetPlayerbotAI())
            return;

        ObjectGuid targetGuid = target->GetObjectGuid();
        bot->SetSelectionGuid(targetGuid);
        bot->GetPlayerbotAI()->GetAiObjectContext()->GetValue<Unit*>("current target")->Set(target);
        bot->GetPlayerbotAI()->GetAiObjectContext()->GetValue<ObjectGuid>("pull target")->Set(targetGuid);

        if (!sServerFacade.IsInFront(bot, target, sPlayerbotAIConfig.sightDistance, CAST_ANGLE_IN_FRONT))
            sServerFacade.SetFacingTo(bot, target);

        bot->Attack(target, true);
        bot->GetPlayerbotAI()->ChangeEngine(BOT_STATE_COMBAT);
        bot->GetPlayerbotAI()->DoSpecificAction("reach melee");
        bot->GetPlayerbotAI()->DoSpecificAction("tank assist");
        bot->GetPlayerbotAI()->SetNextCheckDelay(sPlayerbotAIConfig.reactDelay);
    }

    void UpdatePendingPlayerbotShootPulls(PlayerbotMgr* mgr)
    {
        if (!mgr)
            return;

        for (map<uint64, ObjectGuid>::iterator i = pendingPlayerbotShootPullTargets.begin(); i != pendingPlayerbotShootPullTargets.end();)
        {
            Player* bot = mgr->GetPlayerBot(i->first);
            if (!bot)
                bot = sObjectMgr.GetPlayer(ObjectGuid(i->first));

            if (!bot || !bot->GetPlayerbotAI() || !bot->GetMap())
            {
                map<uint64, ObjectGuid>::iterator erase = i++;
                pendingPlayerbotShootPullTargets.erase(erase);
                continue;
            }

            if (bot->GetPlayerbotAI()->GetMaster() != mgr->GetMaster())
            {
                ++i;
                continue;
            }

            Unit* target = bot->GetMap()->GetUnit(i->second);
            if (!target || sServerFacade.UnitIsDead(target) || sServerFacade.IsFriendlyTo(bot, target))
            {
                map<uint64, ObjectGuid>::iterator erase = i++;
                pendingPlayerbotShootPullTargets.erase(erase);
                continue;
            }

            bot->SetSelectionGuid(i->second);
            bot->GetPlayerbotAI()->GetAiObjectContext()->GetValue<Unit*>("current target")->Set(target);
            bot->GetPlayerbotAI()->GetAiObjectContext()->GetValue<ObjectGuid>("pull target")->Set(i->second);

            string pullReason;
            PlayerbotShootPullResult result = CastPlayerbotShootPull(bot, target, &pullReason);
            if (result == PLAYERBOT_SHOOT_PULL_PENDING)
            {
                ++i;
                continue;
            }

            if (result == PLAYERBOT_SHOOT_PULL_STARTED)
                StartPlayerbotTankMeleePull(bot, target);

            map<uint64, ObjectGuid>::iterator erase = i++;
            pendingPlayerbotShootPullTargets.erase(erase);
        }
    }

    vector<string> BuildPlayerbotPullCommands(const string& tankName, Player* bot, const string& opener)
    {
        vector<string> commands;
        if (!bot || !bot->GetPlayerbotAI())
            return commands;

        commands.push_back("formation shield");
        commands.push_back("nc +follow,-stay,-passive,-grind,-rpg,-runaway,+food,-buff");

        if (LowerPlayerbotCommandParam(bot->GetName()) == LowerPlayerbotCommandParam(tankName))
        {
            commands.push_back("directpullattack " + NormalizePlayerbotPullOpener(opener));
            commands.push_back("nc +tank assist,+tank aoe");
            commands.push_back("co +tank,+tank assist,+tank aoe,+bthreat,+threat,-heal,-dps");
            if (NormalizePlayerbotPullOpener(opener) == "charge")
            {
                commands.push_back("tank attack");
                commands.push_back("attack");
            }
        }
        else if (bot->GetPlayerbotAI()->IsHeal(bot))
        {
            if (PlayerbotCanCure(bot))
                commands.push_back("co +heal,+cure,-conserve mana,-dps,-tank,-bear");
            else
                commands.push_back("co +heal,-conserve mana,-dps,-tank,-bear");
        }
        else
        {
            commands.push_back("nc +dps assist,+follow,-stay,-passive");
            if (bot->getClass() == CLASS_HUNTER)
                commands.push_back("co +ranged,+dps,+dps assist,+attack weak,+threat,+dps debuff,-heal,-tank,-bear");
            else
                commands.push_back("co +dps,+dps assist,+attack weak,+threat,-heal,-tank,-bear");
        }

        return commands;
    }

    vector<string> BuildPlayerbotPrepCommands(Player* bot)
    {
        vector<string> commands;
        if (!bot || !bot->GetPlayerbotAI())
            return commands;

        commands.push_back("formation stack");
        commands.push_back("nc +follow,-stay,-passive,-grind,-rpg,-runaway,+food");

        switch (bot->getClass())
        {
        case CLASS_DRUID:
        case CLASS_MAGE:
        case CLASS_PRIEST:
            commands.push_back("nc +buff");
            commands.push_back("grouprefresh");
            break;
        case CLASS_PALADIN:
            commands.push_back("nc +bstats,+barmor");
            commands.push_back("grouprefresh");
            break;
        case CLASS_HUNTER:
            commands.push_back("nc +bdps");
            break;
        case CLASS_SHAMAN:
            commands.push_back("nc +bmana");
            break;
        default:
            break;
        }

        return commands;
    }

    int GetPlayerbotRaidMarkerIndex(string marker)
    {
        marker = LowerPlayerbotCommandParam(TrimPlayerbotCommandParam(marker));
        if (marker == "star" || marker == "1")
            return 0;
        if (marker == "circle" || marker == "coin" || marker == "2")
            return 1;
        if (marker == "diamond" || marker == "3")
            return 2;
        if (marker == "triangle" || marker == "4")
            return 3;
        if (marker == "moon" || marker == "5")
            return 4;
        if (marker == "square" || marker == "6")
            return 5;
        if (marker == "cross" || marker == "x" || marker == "7")
            return 6;
        if (marker == "skull" || marker == "8")
            return 7;

        return -1;
    }

    bool MarkPlayerbotBrainTarget(Player* master, const string& marker, string* reason = NULL)
    {
        if (!master)
        {
            if (reason) *reason = "no active master";
            return false;
        }

        int markerIndex = GetPlayerbotRaidMarkerIndex(marker);
        if (markerIndex < 0)
        {
            if (reason) *reason = "unknown marker";
            return false;
        }

        Group* group = master->GetGroup();
        if (!group)
        {
            if (reason) *reason = "not in a group";
            return false;
        }

        if (!master->GetSelectionGuid())
        {
            if (reason) *reason = "no enemy selected";
            return false;
        }

        Unit* target = master->GetMap() ? master->GetMap()->GetUnit(master->GetSelectionGuid()) : NULL;
        if (!target)
        {
            if (reason) *reason = "no enemy selected";
            return false;
        }

        if (sServerFacade.UnitIsDead(target))
        {
            if (reason) *reason = "target is dead";
            return false;
        }

        if (sServerFacade.IsFriendlyTo(master, target))
        {
            if (reason) *reason = "target is friendly";
            return false;
        }

        group->SetTargetIcon(markerIndex,
#ifdef MANGOSBOT_TWO
            master->GetObjectGuid(),
#endif
            target->GetObjectGuid());
        return true;
    }

    vector<string> BuildPlayerbotBrainAssistCommands(Player* bot)
    {
        vector<string> commands;
        if (!bot || !bot->GetPlayerbotAI())
            return commands;

        commands.push_back("nc +follow,-stay,-passive,-grind,-rpg,-runaway,+food,+loot,+threat");

        if (bot->GetPlayerbotAI()->IsTank(bot))
        {
            commands.push_back("co +tank,+tank assist,+tank aoe,+bthreat,+threat,-heal,-dps,-passive");
        }
        else if (bot->GetPlayerbotAI()->IsHeal(bot))
        {
            if (PlayerbotCanCure(bot))
                commands.push_back("co +heal,+cure,+follow,-stay,-flee,-ranged,-runaway,-conserve mana,-dps,-tank,-bear,-passive");
            else
                commands.push_back("co +heal,+follow,-stay,-flee,-ranged,-runaway,-conserve mana,-dps,-tank,-bear,-passive");
        }
        else
        {
            commands.push_back("co +dps,+dps assist,+attack weak,+threat,+follow,-stay,-flee,-runaway,-heal,-tank,-bear,-passive");
        }

        return commands;
    }

    vector<string> BuildPlayerbotBrainStopCommands(Player* bot)
    {
        vector<string> commands;
        if (!bot || !bot->GetPlayerbotAI())
            return commands;

        commands.push_back("nc +stay,+passive,-follow,-grind,-rpg,-runaway");
        commands.push_back("co +passive,-dps,-dps assist,-tank,-tank assist,-tank aoe,-heal,-flee,-runaway");
        commands.push_back("stay");
        return commands;
    }

    bool CanControlOnlinePlayerBot(Player* master, Player* bot, string* reason = NULL)
    {
        if (!master)
        {
            if (reason) *reason = "no active master";
            return false;
        }

        if (!bot)
        {
            if (reason) *reason = "bot is offline";
            return false;
        }

        if (bot == master)
        {
            if (reason) *reason = "cannot control yourself";
            return false;
        }

        if (!bot->GetPlayerbotAI())
        {
            if (reason) *reason = "not a playerbot";
            return false;
        }

        Player* currentMaster = bot->GetPlayerbotAI()->GetMaster();
        if (currentMaster && currentMaster != master &&
                !currentMaster->GetPlayerbotAI() &&
                master->GetSession()->GetSecurity() < SEC_GAMEMASTER)
        {
            if (reason)
            {
                *reason = "already controlled by ";
                *reason += currentMaster->GetName();
            }
            return false;
        }

        if (bot->GetPlayerbotAI()->IsOpposing(master) && master->GetSession()->GetSecurity() < SEC_GAMEMASTER)
        {
            if (reason) *reason = "opposing faction";
            return false;
        }

        return true;
    }

    bool AttachOnlinePlayerBotToMaster(Player* master, Player* bot, string* reason = NULL)
    {
        if (!CanControlOnlinePlayerBot(master, bot, reason))
            return false;

        if (bot->GetPlayerbotAI()->GetMaster() == master)
            return true;

        bot->GetPlayerbotAI()->SetMaster(master);
        bot->GetPlayerbotAI()->ResetStrategies();
        bot->GetPlayerbotAI()->TellMaster("Hello!");
        return true;
    }

    bool AddOnlinePlayerBotToMasterGroup(Player* master, Player* bot, string* reason = NULL)
    {
        if (!CanControlOnlinePlayerBot(master, bot, reason))
            return false;

        Group* group = master->GetGroup();
        if (group && group->isBGGroup())
            group = master->GetOriginalGroup();

        Group* botGroup = bot->GetGroup();
        if (botGroup && botGroup->isBGGroup())
            botGroup = bot->GetOriginalGroup();

        if (botGroup)
        {
            if (group && botGroup == group)
                return true;

            if (reason) *reason = "already in another group";
            return false;
        }

        if (group)
        {
            if (!group->IsLeader(master->GetObjectGuid()) && !group->IsAssistant(master->GetObjectGuid()))
            {
                if (reason) *reason = "not party leader or assistant";
                return false;
            }

            if (group->IsFull())
            {
                if (reason) *reason = "group full";
                return false;
            }
        }

        if (bot->GetGroupInvite())
            bot->UninviteFromGroup();

        if (!group)
        {
            group = new Group;
            if (!group->AddLeaderInvite(master))
            {
                delete group;
                if (reason) *reason = "could not start invite";
                return false;
            }
        }

        if (!group->AddInvite(bot))
        {
            if (reason) *reason = "could not invite";
            return false;
        }

        WorldPacket data(CMSG_GROUP_ACCEPT, 0);
        bot->GetSession()->HandleGroupAcceptOpcode(data);

        Group* joinedGroup = bot->GetGroup();
        if (joinedGroup && joinedGroup->isBGGroup())
            joinedGroup = bot->GetOriginalGroup();

        group = master->GetGroup();
        if (group && group->isBGGroup())
            group = master->GetOriginalGroup();

        if (joinedGroup && joinedGroup == group)
            return true;

        if (reason) *reason = "could not accept invite";
        return false;
    }

    bool DetachOnlinePlayerBotFromMaster(Player* master, Player* bot, string* reason = NULL)
    {
        if (!master)
        {
            if (reason) *reason = "no active master";
            return false;
        }

        if (!bot)
        {
            if (reason) *reason = "bot is offline";
            return false;
        }

        if (!bot->GetPlayerbotAI())
        {
            if (reason) *reason = "not a playerbot";
            return false;
        }

        if (bot->GetPlayerbotAI()->GetMaster() != master)
        {
            if (reason) *reason = "not controlled by you";
            return false;
        }

        bot->GetPlayerbotAI()->TellMaster("Goodbye!");
        bot->GetPlayerbotAI()->SetMaster(NULL);
        bot->GetPlayerbotAI()->ResetStrategies(false);
        return true;
    }

    bool TryCastPlayerbotPartyBuff(Player* master, Player* bot, const string& spell, const string& equivalentAura = "")
    {
        if (!bot || !bot->GetPlayerbotAI() || bot->IsInCombat())
            return false;

        PlayerbotAI* ai = bot->GetPlayerbotAI();
        Group* group = bot->GetGroup();
        if (!group && master)
            group = master->GetGroup();

        vector<Player*> targets;
        set<uint64> added;
        if (group)
        {
            if (group->isBGGroup() && bot->GetOriginalGroup())
                group = bot->GetOriginalGroup();

            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* member = ref->getSource();
                if (!member || !member->IsInWorld())
                    continue;

                if (added.insert(member->GetObjectGuid().GetRawValue()).second)
                    targets.push_back(member);
            }
        }

        if (master && master->IsInWorld() && added.insert(master->GetObjectGuid().GetRawValue()).second)
            targets.push_back(master);

        if (added.insert(bot->GetObjectGuid().GetRawValue()).second)
            targets.push_back(bot);

        for (vector<Player*>::iterator i = targets.begin(); i != targets.end(); ++i)
        {
            Player* target = *i;
            if (!target || !target->IsAlive() || ai->HasAura(spell, target, true) ||
                    (!equivalentAura.empty() && ai->HasAura(equivalentAura, target)))
                continue;

            if (ai->CanCastSpell(spell, target) && ai->CastSpell(spell, target))
                return true;
        }

        return false;
    }

    bool RequestImmediatePlayerbotPartyBuffs(Player* master, Player* bot)
    {
        if (!bot || !bot->GetPlayerbotAI())
            return false;

        switch (bot->getClass())
        {
        case CLASS_PRIEST:
            return TryCastPlayerbotPartyBuff(master, bot, "power word: fortitude", "prayer of fortitude") ||
                TryCastPlayerbotPartyBuff(master, bot, "divine spirit", "prayer of spirit");
        case CLASS_DRUID:
            return TryCastPlayerbotPartyBuff(master, bot, "mark of the wild");
        case CLASS_MAGE:
            return TryCastPlayerbotPartyBuff(master, bot, "arcane intellect");
        case CLASS_PALADIN:
            return TryCastPlayerbotPartyBuff(master, bot, "blessing of kings") ||
                TryCastPlayerbotPartyBuff(master, bot, "blessing of wisdom") ||
                TryCastPlayerbotPartyBuff(master, bot, "blessing of might");
        default:
            return false;
        }
    }

    bool RouteOnlinePlayerBotCommands(Player* master, Player* bot, vector<string> const& commands, string* reason);

    bool RouteOnlinePlayerBotCommand(Player* master, Player* bot, const string& command, string* reason = NULL)
    {
        string normalizedCommand = LowerPlayerbotCommandParam(TrimPlayerbotCommandParam(command));
        if (normalizedCommand.empty())
        {
            if (reason) *reason = "empty command";
            return false;
        }

        if (!AttachOnlinePlayerBotToMaster(master, bot, reason))
            return false;

        if (normalizedCommand == "think" || normalizedCommand == "brain")
        {
            bot->GetPlayerbotAI()->TellMaster(bot->GetPlayerbotAI()->FormatBrainState());
            return true;
        }

        if (normalizedCommand == "buff")
        {
            if (bot->IsInCombat())
            {
                bot->GetPlayerbotAI()->ChangeStrategy("+buff", BOT_STATE_NON_COMBAT);
                bot->GetPlayerbotAI()->TellMaster("Buff refresh queued until combat ends.");
                return true;
            }

            return RouteOnlinePlayerBotCommands(master, bot, BuildPlayerbotPrepCommands(bot), reason);
        }

        if (normalizedCommand == "grouprefresh" && bot->IsInCombat())
        {
            bot->GetPlayerbotAI()->ChangeStrategy("+buff", BOT_STATE_NON_COMBAT);
            bot->GetPlayerbotAI()->TellMaster("Group buffs queued until combat ends.");
            return true;
        }

        if (normalizedCommand == "grouprefresh")
        {
            bot->GetPlayerbotAI()->ChangeStrategy("+follow,+buff", BOT_STATE_NON_COMBAT);
            bool castStarted = RequestImmediatePlayerbotPartyBuffs(master, bot);
            bot->GetPlayerbotAI()->TellMaster(castStarted ? "Refreshing party buffs" : "No party buffs needed or available");
            return true;
        }

        if (normalizedCommand == "directpullattack" || normalizedCommand.find("directpullattack ") == 0)
        {
            string opener = NormalizePlayerbotPullOpener(normalizedCommand.size() > 16 ? normalizedCommand.substr(17) : "");
            ObjectGuid selectedGuid = master->GetSelectionGuid();
            Unit* target = selectedGuid && master->GetMap() ? master->GetMap()->GetUnit(selectedGuid) : NULL;
            if (!target)
            {
                if (reason) *reason = "no selected pull target";
                return false;
            }

            bot->SetSelectionGuid(selectedGuid);
            bot->GetPlayerbotAI()->GetAiObjectContext()->GetValue<Unit*>("current target")->Set(target);
            bot->GetPlayerbotAI()->GetAiObjectContext()->GetValue<ObjectGuid>("pull target")->Set(selectedGuid);

            if (opener == "charge" && bot->getClass() == CLASS_WARRIOR)
            {
                bot->GetPlayerbotAI()->DoSpecificAction("charge");
                bot->GetPlayerbotAI()->DoSpecificAction("attack my target");
                bot->GetPlayerbotAI()->DoSpecificAction("reach melee");
                return true;
            }
            else
            {
                string pullReason;
                PlayerbotShootPullResult pullResult = CastPlayerbotShootPull(bot, target, &pullReason);
                if (pullResult == PLAYERBOT_SHOOT_PULL_FAILED)
                {
                    bot->GetPlayerbotAI()->TellMaster(string("Shoot pull failed: ") + pullReason);
                    if (reason) *reason = pullReason;
                    return false;
                }

                if (pullResult == PLAYERBOT_SHOOT_PULL_PENDING)
                    pendingPlayerbotShootPullTargets[bot->GetObjectGuid().GetRawValue()] = selectedGuid;
                else
                {
                    pendingPlayerbotShootPullTargets.erase(bot->GetObjectGuid().GetRawValue());
                    StartPlayerbotTankMeleePull(bot, target);
                }
            }

            return true;
        }

        if (normalizedCommand.find("nc ") == 0)
        {
            bot->GetPlayerbotAI()->ChangeStrategy(normalizedCommand.substr(3), BOT_STATE_NON_COMBAT);
            return true;
        }

        if (normalizedCommand.find("co ") == 0)
        {
            bot->GetPlayerbotAI()->ChangeStrategy(normalizedCommand.substr(3), BOT_STATE_COMBAT);
            return true;
        }

        if (normalizedCommand.find("ds ") == 0)
        {
            bot->GetPlayerbotAI()->ChangeStrategy(normalizedCommand.substr(3), BOT_STATE_DEAD);
            return true;
        }

        bot->GetPlayerbotAI()->HandleCommand(CHAT_MSG_WHISPER, command, *master);
        return true;
    }

    bool RouteOnlinePlayerBotCommands(Player* master, Player* bot, vector<string> const& commands, string* reason = NULL)
    {
        if (commands.empty())
        {
            if (reason) *reason = "unknown preset";
            return false;
        }

        for (vector<string>::const_iterator i = commands.begin(); i != commands.end(); ++i)
        {
            if (!RouteOnlinePlayerBotCommand(master, bot, *i, reason))
                return false;
        }

        return true;
    }
}

PlayerbotHolder::PlayerbotHolder() : PlayerbotAIBase()
{
    for (uint32 spellId = 0; spellId < sServerFacade.GetSpellInfoRows(); spellId++)
        sServerFacade.LookupSpellInfo(spellId);
}

PlayerbotHolder::~PlayerbotHolder()
{
}


void PlayerbotHolder::UpdateAIInternal(uint32 elapsed)
{
}

void PlayerbotHolder::UpdateSessions(uint32 elapsed)
{
    for (PlayerBotMap::const_iterator itr = GetPlayerBotsBegin(); itr != GetPlayerBotsEnd(); ++itr)
    {
        Player* const bot = itr->second;
        if (bot->IsBeingTeleported())
        {
            bot->GetPlayerbotAI()->HandleTeleportAck();
        }
        else if (bot->IsInWorld())
        {
            bot->GetSession()->HandleBotPackets();
        }
    }
}

void PlayerbotHolder::LogoutAllBots()
{
    while (true)
    {
        PlayerBotMap::const_iterator itr = GetPlayerBotsBegin();
        if (itr == GetPlayerBotsEnd()) break;
        Player* bot= itr->second;
        LogoutPlayerBot(bot->GetObjectGuid().GetRawValue());
    }
}

void PlayerbotHolder::LogoutPlayerBot(uint64 guid)
{
    Player* bot = GetPlayerBot(guid);
    if (bot)
    {
        if (bot->GetPlayerbotAI())
        {
            bot->GetPlayerbotAI()->TellMaster("Goodbye!");
            sPlayerbotDbStore.Save(bot->GetPlayerbotAI());
            sLog.outString("Bot %s logged out", bot->GetName());
            //bot->SaveToDB();
        }

        WorldSession * botWorldSessionPtr = bot->GetSession();
        playerBots.erase(guid);    // deletes bot player ptr inside this WorldSession PlayerBotMap
        botWorldSessionPtr->LogoutPlayer(true); // this will delete the bot Player object and PlayerbotAI object
        delete botWorldSessionPtr;  // finally delete the bot's WorldSession
    }
}

Player* PlayerbotHolder::GetPlayerBot(uint64 playerGuid) const
{
    PlayerBotMap::const_iterator it = playerBots.find(playerGuid);
    return (it == playerBots.end()) ? 0 : it->second;
}

void PlayerbotHolder::OnBotLogin(Player * const bot)
{
    PlayerbotAI* ai = new PlayerbotAI(bot);
    bot->SetPlayerbotAI(ai);

    playerBots[bot->GetObjectGuid().GetRawValue()] = bot;

    Player* master = ai->GetMaster();
    if (master)
    {
        ObjectGuid masterGuid = master->GetObjectGuid();
        if (master->GetGroup() &&
            ! master->GetGroup()->IsLeader(masterGuid))
            master->GetGroup()->ChangeLeader(masterGuid);
    }

    Group *group = bot->GetGroup();
    if (group)
    {
        bool groupValid = false;
        Group::MemberSlotList const& slots = group->GetMemberSlots();
        for (Group::MemberSlotList::const_iterator i = slots.begin(); i != slots.end(); ++i)
        {
            ObjectGuid member = i->guid;
            uint32 account = sObjectMgr.GetPlayerAccountIdByGUID(member);
            if (!sPlayerbotAIConfig.IsInRandomAccountList(account))
            {
                groupValid = true;
                break;
            }
        }

        if (!groupValid)
        {
            WorldPacket p;
            string member = bot->GetName();
            p << uint32(PARTY_OP_LEAVE) << member << uint32(0);
            bot->GetSession()->HandleGroupDisbandOpcode(p);
        }
    }

    ai->ResetStrategies();
    OnBotLoginInternal(bot);

    ai->TellMaster("Hello!");
}

string PlayerbotHolder::ProcessBotCommand(string cmd, ObjectGuid guid, bool admin, uint32 masterAccountId, uint32 masterGuildId)
{
    if (!sPlayerbotAIConfig.enabled || guid.IsEmpty())
        return "bot system is disabled";

    uint32 botAccount = sObjectMgr.GetPlayerAccountIdByGUID(guid);
    bool isRandomBot = sRandomPlayerbotMgr.IsRandomBot(guid);
    bool isRandomAccount = sPlayerbotAIConfig.IsInRandomAccountList(botAccount);
    bool isMasterAccount = (masterAccountId == botAccount);

    if (!isRandomAccount && !isMasterAccount && !admin)
    {
        uint32 guildId = 0;
        QueryResult* results = CharacterDatabase.PQuery("SELECT guildid FROM guild_member where guid = '%u'", guid);
        if (results)
        {
            Field* fields = results->Fetch();
            guildId = fields[0].GetUInt32();
            delete results;
        }
        if (!sPlayerbotAIConfig.allowGuildBots || guildId != masterGuildId)
            return "not in your guild or account";
    }

    if (cmd == "add" || cmd == "login" || cmd == "control" || cmd == "party")
    {
        Player* onlineBot = sObjectMgr.GetPlayer(guid);
        if (onlineBot)
        {
            if (onlineBot->GetPlayerbotAI())
                return "already online";

            return "player already logged in";
        }

        AddPlayerBot(guid.GetRawValue(), masterAccountId);
        return "ok";
    }
    else if (cmd == "remove" || cmd == "logout" || cmd == "rm")
    {
        if (!sObjectMgr.GetPlayer(guid))
            return "player is offline";

        if (!GetPlayerBot(guid.GetRawValue()))
            return "not your bot";

        LogoutPlayerBot(guid.GetRawValue());
        return "ok";
    }

    if (admin)
    {
        Player* bot = GetPlayerBot(guid.GetRawValue());
        if (!bot) bot = sRandomPlayerbotMgr.GetPlayerBot(guid.GetRawValue());
        if (!bot)
            return "bot not found";

        Player* master = bot->GetPlayerbotAI()->GetMaster();
        if (master)
        {
            if (cmd == "init=white" || cmd == "init=common")
            {
                PlayerbotFactory factory(bot, master->getLevel(), ITEM_QUALITY_NORMAL);
                factory.Randomize(false);
                return "ok";
            }
            else if (cmd == "init=green" || cmd == "init=uncommon")
            {
                PlayerbotFactory factory(bot, master->getLevel(), ITEM_QUALITY_UNCOMMON);
                factory.Randomize(false);
                return "ok";
            }
            else if (cmd == "init=blue" || cmd == "init=rare")
            {
                PlayerbotFactory factory(bot, master->getLevel(), ITEM_QUALITY_RARE);
                factory.Randomize(false);
                return "ok";
            }
            else if (cmd == "init=epic" || cmd == "init=purple")
            {
                PlayerbotFactory factory(bot, master->getLevel(), ITEM_QUALITY_EPIC);
                factory.Randomize(false);
                return "ok";
            }
        }

        if (cmd == "levelup" || cmd == "level")
        {
            PlayerbotFactory factory(bot, bot->getLevel());
            factory.Randomize(true);
            return "ok";
        }
        else if (cmd == "refresh")
        {
            PlayerbotFactory factory(bot, bot->getLevel());
            factory.Refresh();
            return "ok";
        }
        else if (cmd == "random")
        {
            sRandomPlayerbotMgr.Randomize(bot);
            return "ok";
        }
    }

    return "unknown command";
}

bool PlayerbotMgr::HandlePlayerbotMgrCommand(ChatHandler* handler, char const* args)
{
    if (!sPlayerbotAIConfig.enabled)
    {
        handler->PSendSysMessage("|cffff0000Playerbot system is currently disabled!");
        return false;
    }

    WorldSession *m_session = handler->GetSession();

    if (!m_session)
    {
        handler->PSendSysMessage("You may only add bots from an active session");
        return false;
    }

    Player* player = m_session->GetPlayer();
    PlayerbotMgr* mgr = player->GetPlayerbotMgr();
    if (!mgr)
    {
        handler->PSendSysMessage("you cannot control bots yet");
        return false;
    }

    list<string> messages = mgr->HandlePlayerbotCommand(args, player);
    if (messages.empty())
        return true;

    for (list<string>::iterator i = messages.begin(); i != messages.end(); ++i)
    {
        handler->PSendSysMessage(i->c_str());
    }

    return true;
}

list<string> PlayerbotHolder::HandlePlayerbotCommand(char const* args, Player* master)
{
    list<string> messages;
    const string usage = "usage: list [prefix], pool [class|role|prefix], create NAME class [race] [role] [quality], account list [prefix], account chars ACCOUNT, account quick ACCOUNT NAME class [gender] [role], account create ACCOUNT PASSWORD, account character ACCOUNT NAME class [race] [gender] [role] [quality], account level CHARACTER LEVEL [quality], account gear CHARACTER [quality] [level], add random [class|role], control random [class|role], party PLAYERNAME|random [class|role], prep [PLAYERNAME|*], brain prep|assist|status|monitor|debug|stop|mark MARKER|pull TANK [MARKER] [shoot|charge], or add/init/remove/cmd/role PLAYERNAME|* [COMMAND|heal|dps|tank], pull TANKNAME [TARGETNAME], or boss PRESET";

    string cmdStr, charnameStr, paramStr;
    SplitPlayerbotCommand(args ? args : "", cmdStr, charnameStr, paramStr);

    if (cmdStr.empty())
    {
        messages.push_back(usage);
        return messages;
    }

    if (cmdStr == "list")
    {
        messages.push_back(ListBots(master, charnameStr));
        return messages;
    }

    if (cmdStr == "pool")
    {
        messages.push_back(ListRandomPlayerbotPool(master, charnameStr));
        return messages;
    }

    if (cmdStr == "create")
    {
        ObjectGuid createdGuid;
        string roleCommand;
        uint32 initQuality = ITEM_QUALITY_RARE;
        messages.push_back(CreatePlayerbotCharacter(master, charnameStr, paramStr, createdGuid, roleCommand, initQuality));
        if (!createdGuid.IsEmpty())
        {
            pendingPlayerbotInitQualities[createdGuid.GetRawValue()] = initQuality;
            if (!roleCommand.empty())
                pendingPlayerbotRoleCommands[createdGuid.GetRawValue()] = roleCommand;

            AddPlayerBot(createdGuid.GetRawValue(), master->GetSession()->GetAccountId());
        }

        return messages;
    }

    if (cmdStr == "account")
    {
        string accountAction = LowerPlayerbotCommandParam(charnameStr);
        if (accountAction == "create")
        {
            messages.push_back(CreatePlayerbotLoginAccount(master, paramStr));
            return messages;
        }

        if (accountAction == "list" || accountAction == "accounts")
        {
            AppendPlayerbotLoginAccountMessages(messages, master, paramStr);
            return messages;
        }

        if (accountAction == "chars" || accountAction == "characters")
        {
            AppendPlayerbotLoginCharacterMessages(messages, master, paramStr);
            return messages;
        }

        if (accountAction == "quick")
        {
            ObjectGuid createdGuid;
            string roleCommand;
            uint32 initQuality = ITEM_QUALITY_EPIC;
            uint32 initLevel = GetPlayerbotRoundedMasterLevel(master);
            messages.push_back(QuickCreatePlayerbotLoginCharacter(master, paramStr,
                    createdGuid, roleCommand, initQuality, initLevel));
            if (!createdGuid.IsEmpty())
            {
                pendingPlayerbotInitQualities[createdGuid.GetRawValue()] = initQuality;
                pendingPlayerbotInitLevels[createdGuid.GetRawValue()] = initLevel;
                pendingPlayerbotSkipGroup.insert(createdGuid.GetRawValue());
                if (!roleCommand.empty())
                    pendingPlayerbotRoleCommands[createdGuid.GetRawValue()] = roleCommand;

                AddPlayerBot(createdGuid.GetRawValue(), master->GetSession()->GetAccountId());
            }

            return messages;
        }

        if (accountAction == "character" || accountAction == "char")
        {
            ObjectGuid createdGuid;
            string roleCommand;
            uint32 initQuality = ITEM_QUALITY_RARE;
            messages.push_back(CreatePlayerbotLoginCharacter(master, paramStr, createdGuid, roleCommand, initQuality));
            if (!createdGuid.IsEmpty())
            {
                pendingPlayerbotInitQualities[createdGuid.GetRawValue()] = initQuality;
                if (!roleCommand.empty())
                    pendingPlayerbotRoleCommands[createdGuid.GetRawValue()] = roleCommand;
            }

            return messages;
        }

        if (accountAction == "level")
        {
            messages.push_back(ApplyPlayerbotCharacterLevel(master, paramStr));
            return messages;
        }

        if (accountAction == "gear" || accountAction == "outfit")
        {
            ObjectGuid targetGuid;
            bool hasLevel = false;
            uint32 targetLevel = 0;
            uint32 initQuality = ITEM_QUALITY_RARE;
            string actionName = "gear";

            messages.push_back(SchedulePlayerbotLevelGear(master, actionName, paramStr, false,
                    targetGuid, hasLevel, targetLevel, initQuality));
            if (!targetGuid.IsEmpty())
            {
                pendingPlayerbotInitQualities[targetGuid.GetRawValue()] = initQuality;
                if (hasLevel)
                    pendingPlayerbotInitLevels[targetGuid.GetRawValue()] = targetLevel;

                AddPlayerBot(targetGuid.GetRawValue(), master->GetSession()->GetAccountId());
            }

            return messages;
        }

        messages.push_back("usage: account list [prefix], account chars ACCOUNT, account quick ACCOUNT NAME class [gender] [role], account create ACCOUNT PASSWORD, account character ACCOUNT NAME class [race] [gender] [role] [quality], account level CHARACTER LEVEL [quality], account gear CHARACTER [quality] [level]");
        return messages;
    }

    bool routeBossCommand = cmdStr == "boss";
    if (routeBossCommand)
    {
        if (charnameStr.empty() || charnameStr == "list" || charnameStr == "?")
        {
            messages.push_back(GetPlayerbotBossPresetList());
            return messages;
        }

        paramStr = charnameStr + (paramStr.empty() ? "" : " " + paramStr);
        charnameStr = "*";
    }

    bool routePrepCommand = cmdStr == "prep";
    if (routePrepCommand && charnameStr.empty())
        charnameStr = "*";

    bool routePullCommand = cmdStr == "pull";

    bool routeBrainCommand = cmdStr == "brain";
    string brainAction;
    string brainParam;
    bool routeBrainAssistCommand = false;
    bool routeBrainStopCommand = false;
    bool routeBrainStatusCommand = false;
    bool routeBrainMonitorCommand = false;
    bool routeBrainDebugCommand = false;
    bool routeBrainPrepCommand = false;
    bool routeBrainPullCommand = false;
    string brainPullTankName;
    string brainPullMarker;
    string brainPullOpener;
    string brainPullTargetName;
    if (routeBrainCommand)
    {
        brainAction = LowerPlayerbotCommandParam(charnameStr);
        brainParam = TrimPlayerbotCommandParam(paramStr);
        if (brainAction.empty() || brainAction == "help" || brainAction == "?")
        {
            messages.push_back("usage: brain prep|assist|status|monitor|debug|stop|mark MARKER|pull TANK [MARKER] [shoot|charge]");
            return messages;
        }

        if (brainAction == "prep")
        {
            routeBrainPrepCommand = true;
            routePrepCommand = true;
            charnameStr = brainParam.empty() ? "*" : brainParam;
        }
        else if (brainAction == "assist")
        {
            routeBrainAssistCommand = true;
            charnameStr = brainParam.empty() ? "*" : brainParam;
        }
        else if (brainAction == "stop")
        {
            routeBrainStopCommand = true;
            charnameStr = brainParam.empty() ? "*" : brainParam;
        }
        else if (brainAction == "status" || brainAction == "think")
        {
            routeBrainStatusCommand = true;
            charnameStr = brainParam.empty() ? "*" : brainParam;
        }
        else if (brainAction == "monitor")
        {
            routeBrainMonitorCommand = true;
            charnameStr = brainParam.empty() ? "*" : brainParam;
        }
        else if (brainAction == "debug")
        {
            routeBrainDebugCommand = true;
            charnameStr = brainParam.empty() ? "*" : brainParam;
        }
        else if (brainAction == "mark")
        {
            string reason;
            string marker = brainParam.empty() ? "skull" : brainParam;
            if (!MarkPlayerbotBrainTarget(master, marker, &reason))
                messages.push_back(string("brain mark ") + marker + ": " + reason);
            else
                messages.push_back(string("brain mark ") + marker + ": ok");
            return messages;
        }
        else if (brainAction == "pull")
        {
            vector<string> pullTokens;
            split(pullTokens, brainParam, " \t\r\n");
            if (pullTokens.empty())
            {
                messages.push_back("usage: brain pull TANK [MARKER] [shoot|charge]");
                return messages;
            }

            brainPullTankName = TrimPlayerbotCommandParam(pullTokens[0]);
            brainPullOpener = "shoot";
            for (uint32 i = 1; i < pullTokens.size(); ++i)
            {
                string token = LowerPlayerbotCommandParam(TrimPlayerbotCommandParam(pullTokens[i]));
                if (token == "shoot" || token == "charge")
                    brainPullOpener = token;
                else if (brainPullMarker.empty())
                    brainPullMarker = token;
            }

            if (brainPullTankName.empty())
            {
                messages.push_back("usage: brain pull TANK [MARKER] [shoot|charge]");
                return messages;
            }

            routeBrainPullCommand = true;
            routePullCommand = true;
            charnameStr = brainPullTankName;
            paramStr = brainPullMarker;
        }
        else
        {
            messages.push_back("usage: brain prep|assist|status|monitor|debug|stop|mark MARKER|pull TANK [MARKER] [shoot|charge]");
            return messages;
        }
    }

    string pullTankName;
    string pullTargetName;
    if (routeBrainPullCommand)
    {
        if (!master || !master->GetMap() || !master->GetSelectionGuid())
        {
            messages.push_back("No enemy selected.");
            return messages;
        }

        Unit* pullTarget = master->GetMap()->GetUnit(master->GetSelectionGuid());
        if (!pullTarget)
        {
            messages.push_back("No enemy selected.");
            return messages;
        }

        if (sServerFacade.UnitIsDead(pullTarget))
        {
            messages.push_back(string("Pull target is dead: ") + pullTarget->GetName());
            return messages;
        }

        if (sServerFacade.IsFriendlyTo(master, pullTarget))
        {
            messages.push_back(string("Pull target is friendly: ") + pullTarget->GetName());
            return messages;
        }

        if (!master->IsWithinDistInMap(pullTarget, sPlayerbotAIConfig.sightDistance))
        {
            messages.push_back(string("Pull target is too far away: ") + pullTarget->GetName());
            return messages;
        }

        if (!brainPullMarker.empty())
        {
            string reason;
            if (!MarkPlayerbotBrainTarget(master, brainPullMarker, &reason))
            {
                messages.push_back(string("brain pull marker: ") + reason);
                return messages;
            }
        }

        pullTankName = brainPullTankName;
        pullTargetName = pullTarget->GetName();
        charnameStr = "*";
    }
    else if (routePullCommand)
    {
        if (charnameStr.empty())
        {
            messages.push_back("usage: pull TANKNAME [TARGETNAME]");
            return messages;
        }

        pullTankName = charnameStr;
        pullTargetName = TrimPlayerbotCommandParam(paramStr);

        if (!master || !master->GetMap() || !master->GetSelectionGuid())
        {
            messages.push_back("No enemy selected.");
            return messages;
        }

        Unit* pullTarget = master->GetMap()->GetUnit(master->GetSelectionGuid());
        if (!pullTarget)
        {
            messages.push_back("No enemy selected.");
            return messages;
        }

        if (sServerFacade.UnitIsDead(pullTarget))
        {
            messages.push_back(string("Pull target is dead: ") + pullTarget->GetName());
            return messages;
        }

        if (sServerFacade.IsFriendlyTo(master, pullTarget))
        {
            messages.push_back(string("Pull target is friendly: ") + pullTarget->GetName());
            return messages;
        }

        if (!master->IsWithinDistInMap(pullTarget, sPlayerbotAIConfig.sightDistance))
        {
            messages.push_back(string("Pull target is too far away: ") + pullTarget->GetName());
            return messages;
        }

        pullTargetName = pullTarget->GetName();
        charnameStr = "*";
    }

    if (charnameStr.empty())
    {
        messages.push_back(usage);
        return messages;
    }

    bool partyCommand = cmdStr == "party";
    bool safeControlCommand = cmdStr == "control" || cmdStr == "add" || cmdStr == "login";

    if ((cmdStr == "add" || cmdStr == "login" || safeControlCommand || partyCommand) &&
            LowerPlayerbotCommandParam(charnameStr) == "random")
    {
        string reason;
        string randomBotName = FindRandomPlayerbotName(master, paramStr, &reason);
        if (randomBotName.empty())
        {
            messages.push_back(string("add: random - ") + reason);
            return messages;
        }

        charnameStr = randomBotName;
    }

    string routedCommand = (cmdStr == "role") ? BuildPlayerbotRoleCommand(paramStr) : BuildPlayerbotChatCommand(cmdStr, paramStr);
    if (cmdStr == "role" && routedCommand.empty())
    {
        messages.push_back("usage: role PLAYERNAME heal|dps|tank");
        return messages;
    }

    bool routeChatCommand = !IsPlayerbotManagementCommand(cmdStr) && !routePullCommand && !routePrepCommand &&
        !routeBrainAssistCommand && !routeBrainStopCommand && !routeBrainStatusCommand &&
        !routeBrainMonitorCommand && !routeBrainDebugCommand;

    set<string> bots;
    if (charnameStr == "*" && master)
    {
        Group* group = master->GetGroup();
        if (group)
        {
            Group::MemberSlotList slots = group->GetMemberSlots();
            for (Group::member_citerator i = slots.begin(); i != slots.end(); i++)
            {
                ObjectGuid member = i->guid;

                if (member.GetRawValue() == master->GetObjectGuid().GetRawValue())
                    continue;

                Player* groupMember = sObjectMgr.GetPlayer(member);
                if (groupMember && groupMember->GetPlayerbotAI() && CanControlOnlinePlayerBot(master, groupMember))
                    bots.insert(groupMember->GetName());
            }
        }

        for (PlayerBotMap::const_iterator i = GetPlayerBotsBegin(); i != GetPlayerBotsEnd(); ++i)
        {
            Player* bot = i->second;
            if (bot && bot->IsInWorld())
                bots.insert(bot->GetName());
        }

        for (PlayerBotMap::const_iterator i = sRandomPlayerbotMgr.GetPlayerBotsBegin(); i != sRandomPlayerbotMgr.GetPlayerBotsEnd(); ++i)
        {
            Player* bot = i->second;
            if (bot && bot->IsInWorld() && bot->GetPlayerbotAI() && bot->GetPlayerbotAI()->GetMaster() == master)
                bots.insert(bot->GetName());
        }

        if (bots.empty())
        {
            messages.push_back("you have no controlled bots");
            return messages;
        }

        if (routePullCommand && !PlayerbotNameSetContains(bots, pullTankName))
        {
            messages.push_back(string("pull: ") + pullTankName + " - tank bot is not online/controlled");
            return messages;
        }
    }

    if (charnameStr == "!" && master && master->GetSession()->GetSecurity() > SEC_GAMEMASTER)
    {
        for (PlayerBotMap::const_iterator i = GetPlayerBotsBegin(); i != GetPlayerBotsEnd(); ++i)
        {
            Player* bot = i->second;
            if (bot && bot->IsInWorld())
                bots.insert(bot->GetName());
        }
    }

    if (bots.empty() && charnameStr != "*" && charnameStr != "!")
    {
        vector<string> chars = split(charnameStr, ',');
        for (vector<string>::iterator i = chars.begin(); i != chars.end(); i++)
        {
            string s = *i;

            uint32 accountId = GetAccountId(s);
            if (!accountId)
            {
                bots.insert(s);
                continue;
            }

            QueryResult* results = CharacterDatabase.PQuery(
                "SELECT name FROM characters WHERE account = '%u'",
                accountId);
            if (results)
            {
                do
                {
                    Field* fields = results->Fetch();
                    string charName = fields[0].GetString();
                    bots.insert(charName);
                } while (results->NextRow());

                delete results;
            }
        }
    }

    if (routePullCommand && !PlayerbotNameSetContains(bots, pullTankName))
    {
        messages.push_back(string("pull: ") + pullTankName + " - tank bot is not online/controlled");
        return messages;
    }

    if (routePullCommand)
        messages.push_back(pullTankName + " " + NormalizePlayerbotPullOpener(brainPullOpener) + " pulling " + pullTargetName);

    if (routeBrainAssistCommand)
        messages.push_back("Brain assist: tank holds threat, healer triages, DPS assists the tank.");

    if (routeBrainStopCommand)
        messages.push_back("Brain stop: bots stay passive and stop attacking.");

    if (routeBrainStatusCommand)
        messages.push_back("Brain status: shared party state from controlled bots.");

    if (routeBrainMonitorCommand)
        messages.push_back("Brain monitor: compact combat state from controlled bots.");

    if (routeBrainDebugCommand)
        messages.push_back("Brain debug: attacker, victim, and threat state from controlled bots.");

    if (routePrepCommand)
        messages.push_back("Group prep: stack together and refresh available buffs.");

    vector<string> routedBots;
    if (routePullCommand)
    {
        string normalizedTankName = LowerPlayerbotCommandParam(pullTankName);
        for (set<string>::iterator i = bots.begin(); i != bots.end(); ++i)
        {
            if (LowerPlayerbotCommandParam(*i) == normalizedTankName)
            {
                routedBots.push_back(*i);
                break;
            }
        }

        for (set<string>::iterator i = bots.begin(); i != bots.end(); ++i)
        {
            if (LowerPlayerbotCommandParam(*i) != normalizedTankName)
                routedBots.push_back(*i);
        }
    }
    else
    {
        for (set<string>::iterator i = bots.begin(); i != bots.end(); ++i)
            routedBots.push_back(*i);
    }

    for (vector<string>::iterator i = routedBots.begin(); i != routedBots.end(); ++i)
    {
        string bot = *i;
        ostringstream out;
        out << cmdStr << ": " << bot << " - ";

        ObjectGuid member = sObjectMgr.GetPlayerGuidByName(bot);
        if (!member)
        {
            out << "character not found";
        }
        else if (master && member.GetRawValue() != master->GetObjectGuid().GetRawValue())
        {
            Player* onlineBot = sObjectMgr.GetPlayer(member);
            bool admin = master->GetSession()->GetSecurity() >= SEC_GAMEMASTER;
            string reason;

            if (routePullCommand)
            {
                vector<string> pullCommands = BuildPlayerbotPullCommands(pullTankName, onlineBot, brainPullOpener);
                out << (RouteOnlinePlayerBotCommands(master, onlineBot, pullCommands, &reason) ? "ok" : reason);
            }
            else if (routePrepCommand)
            {
                vector<string> prepCommands = BuildPlayerbotPrepCommands(onlineBot);
                out << (RouteOnlinePlayerBotCommands(master, onlineBot, prepCommands, &reason) ? "ok" : reason);
            }
            else if (routeBrainAssistCommand)
            {
                vector<string> brainCommands = BuildPlayerbotBrainAssistCommands(onlineBot);
                out << (RouteOnlinePlayerBotCommands(master, onlineBot, brainCommands, &reason) ? "ok" : reason);
            }
            else if (routeBrainStopCommand)
            {
                vector<string> brainCommands = BuildPlayerbotBrainStopCommands(onlineBot);
                out << (RouteOnlinePlayerBotCommands(master, onlineBot, brainCommands, &reason) ? "ok" : reason);
            }
            else if (routeBrainStatusCommand)
            {
                if (AttachOnlinePlayerBotToMaster(master, onlineBot, &reason))
                {
                    onlineBot->GetPlayerbotAI()->TellMaster(onlineBot->GetPlayerbotAI()->FormatBrainState());
                    out << "ok";
                }
                else
                {
                    out << reason;
                }
            }
            else if (routeBrainMonitorCommand)
            {
                if (AttachOnlinePlayerBotToMaster(master, onlineBot, &reason))
                {
                    onlineBot->GetPlayerbotAI()->TellMaster(onlineBot->GetPlayerbotAI()->FormatBrainMonitor());
                    out << "ok";
                }
                else
                {
                    out << reason;
                }
            }
            else if (routeBrainDebugCommand)
            {
                if (AttachOnlinePlayerBotToMaster(master, onlineBot, &reason))
                {
                    onlineBot->GetPlayerbotAI()->TellMaster(onlineBot->GetPlayerbotAI()->FormatBrainDebug());
                    out << "ok";
                }
                else
                {
                    out << reason;
                }
            }
            else if (routeBossCommand)
            {
                vector<string> bossCommands = BuildPlayerbotBossCommands(paramStr, onlineBot);
                out << (RouteOnlinePlayerBotCommands(master, onlineBot, bossCommands, &reason) ? "ok" : reason);
            }
            else if (routeChatCommand)
            {
                out << (RouteOnlinePlayerBotCommand(master, onlineBot, routedCommand, &reason) ? "ok" : reason);
            }
            else if ((cmdStr == "add" || cmdStr == "login" || safeControlCommand || partyCommand) && onlineBot && onlineBot->GetPlayerbotAI())
            {
                if (AttachOnlinePlayerBotToMaster(master, onlineBot, &reason))
                {
                    out << "ok";

                    if (partyCommand)
                    {
                        string groupReason;
                        if (!AddOnlinePlayerBotToMasterGroup(master, onlineBot, &groupReason))
                            out << ", party failed: " << groupReason;
                        else
                            out << ", party invite sent";
                    }
                    else
                    {
                        out << ", controlled without party";
                    }

                    string roleCommand = BuildPlayerbotRoleCommand(paramStr);
                    if (!roleCommand.empty())
                    {
                        string roleReason;
                        if (!RouteOnlinePlayerBotCommand(master, onlineBot, roleCommand, &roleReason))
                            out << ", role failed: " << roleReason;
                    }
                }
                else
                {
                    out << reason;
                }
            }
            else if ((cmdStr == "remove" || cmdStr == "logout" || cmdStr == "rm") &&
                    onlineBot && onlineBot->GetPlayerbotAI() && !GetPlayerBot(member.GetRawValue()))
            {
                out << (DetachOnlinePlayerBotFromMaster(master, onlineBot, &reason) ? "ok" : reason);
            }
            else
            {
                bool loginControlCommand = (cmdStr == "add" || cmdStr == "login" || safeControlCommand || partyCommand);
                string roleCommand = BuildPlayerbotRoleCommand(paramStr);
                if (loginControlCommand)
                {
                    if (safeControlCommand)
                    {
                        pendingPlayerbotInitQualities.erase(member.GetRawValue());
                        pendingPlayerbotInitLevels.erase(member.GetRawValue());
                    }
                    if (!roleCommand.empty())
                        pendingPlayerbotRoleCommands[member.GetRawValue()] = roleCommand;
                    if (safeControlCommand)
                        pendingPlayerbotSkipGroup.insert(member.GetRawValue());
                }

                string processResult = ProcessBotCommand(cmdStr, member, admin,
                        master->GetSession()->GetAccountId(),
                        master->GetGuildId());

                out << processResult;

                if (loginControlCommand && processResult == "already online")
                    out << ", use party/summon if needed";

                if (loginControlCommand && processResult != "ok")
                {
                    if (!roleCommand.empty())
                        pendingPlayerbotRoleCommands.erase(member.GetRawValue());
                    if (safeControlCommand)
                        pendingPlayerbotSkipGroup.erase(member.GetRawValue());
                }
            }
        }
        else if (!master)
        {
            out << ProcessBotCommand(cmdStr, member, true, -1, -1);
        }

        messages.push_back(out.str());
    }

    return messages;
}

uint32 PlayerbotHolder::GetAccountId(string name)
{
    uint32 accountId = 0;

    QueryResult* results = LoginDatabase.PQuery("SELECT id FROM account WHERE username = '%s'", name.c_str());
    if(results)
    {
        Field* fields = results->Fetch();
        accountId = fields[0].GetUInt32();
        delete results;
    }

    return accountId;
}

string PlayerbotHolder::ListBots(Player* master, string filter)
{
    set<string> bots;
    map<string, string> online;
    list<string> names;
    map<string, string> classes;
    filter = TrimPlayerbotCommandParam(filter);

    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        string name = bot->GetName();
        if (!filter.empty() && name.find(filter) != 0)
            continue;

        bots.insert(name);

        names.push_back(name);
        online[name] = "+";
        classes[name] = GetPlayerbotClassName(bot->getClass());
    }

    if (master)
    {
        QueryResult* results = CharacterDatabase.PQuery("SELECT class,name FROM characters where account = '%u'",
                master->GetSession()->GetAccountId());
        if (results != NULL)
        {
            do
            {
                Field* fields = results->Fetch();
                uint8 cls = fields[0].GetUInt8();
                string name = fields[1].GetString();
                if (!filter.empty() && name.find(filter) != 0)
                    continue;

                if (bots.find(name) == bots.end() && name != master->GetSession()->GetPlayerName())
                {
                    names.push_back(name);
                    online[name] = "-";
                    classes[name] = GetPlayerbotClassName(cls);
                    bots.insert(name);
                }
            } while (results->NextRow());
            delete results;
        }
    }

    if (master)
    {
        Group* group = master->GetGroup();
        if (group)
        {
            Group::MemberSlotList const& groupSlot = group->GetMemberSlots();
            for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); itr++)
            {
                Player *member = sObjectMgr.GetPlayer(itr->guid);
                if (member && sRandomPlayerbotMgr.IsRandomBot(member))
                {
                    string name = member->GetName();
                    if (!filter.empty() && name.find(filter) != 0)
                        continue;

                    if (bots.find(name) != bots.end())
                        continue;

                    bots.insert(name);
                    names.push_back(name);
                    online[name] = "+";
                    classes[name] = GetPlayerbotClassName(member->getClass());
                }
            }
        }

        for (PlayerBotMap::const_iterator it = sRandomPlayerbotMgr.GetPlayerBotsBegin(); it != sRandomPlayerbotMgr.GetPlayerBotsEnd(); ++it)
        {
            Player* const bot = it->second;
            if (!bot || !bot->GetPlayerbotAI() || bot->GetPlayerbotAI()->GetMaster() != master)
                continue;

            string name = bot->GetName();
            if (!filter.empty() && name.find(filter) != 0)
                continue;

            if (bots.find(name) == bots.end())
            {
                bots.insert(name);
                names.push_back(name);
                online[name] = "+";
                classes[name] = GetPlayerbotClassName(bot->getClass());
            }
        }
    }

    names.sort();

    ostringstream out;
    bool first = true;
    out << "Bot roster: ";
    for (list<string>::iterator i = names.begin(); i != names.end(); ++i)
    {
        if (first) first = false; else out << ", ";
        string name = *i;
        out << online[name] << name << " " << classes[name];
    }

    return out.str();
}


PlayerbotMgr::PlayerbotMgr(Player* const master) : PlayerbotHolder(),  master(master), lastErrorTell(0)
{
}

PlayerbotMgr::~PlayerbotMgr()
{
}

void PlayerbotMgr::UpdateAIInternal(uint32 elapsed)
{
    SetNextCheckDelay(sPlayerbotAIConfig.reactDelay);
    UpdatePendingPlayerbotShootPulls(this);
    CheckTellErrors(elapsed);
}

void PlayerbotMgr::HandleCommand(uint32 type, const string& text)
{
    Player *master = GetMaster();
    if (!master)
        return;

    if (text.find(sPlayerbotAIConfig.commandSeparator) != string::npos)
    {
        vector<string> commands;
        split(commands, text, sPlayerbotAIConfig.commandSeparator.c_str());
        for (vector<string>::iterator i = commands.begin(); i != commands.end(); ++i)
        {
            HandleCommand(type, *i);
        }
        return;
    }

    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        bot->GetPlayerbotAI()->HandleCommand(type, text, *master);
    }

    for (PlayerBotMap::const_iterator it = sRandomPlayerbotMgr.GetPlayerBotsBegin(); it != sRandomPlayerbotMgr.GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        if (bot->GetPlayerbotAI()->GetMaster() == master)
            bot->GetPlayerbotAI()->HandleCommand(type, text, *master);
    }
}

void PlayerbotMgr::HandleMasterIncomingPacket(const WorldPacket& packet)
{
    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        bot->GetPlayerbotAI()->HandleMasterIncomingPacket(packet);
    }

    for (PlayerBotMap::const_iterator it = sRandomPlayerbotMgr.GetPlayerBotsBegin(); it != sRandomPlayerbotMgr.GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        if (bot->GetPlayerbotAI()->GetMaster() == GetMaster())
            bot->GetPlayerbotAI()->HandleMasterIncomingPacket(packet);
    }

    switch (packet.GetOpcode())
    {
        // if master is logging out, log out all bots
        case CMSG_LOGOUT_REQUEST:
        {
            LogoutAllBots();
            return;
        }
    }
}
void PlayerbotMgr::HandleMasterOutgoingPacket(const WorldPacket& packet)
{
    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        bot->GetPlayerbotAI()->HandleMasterOutgoingPacket(packet);
    }

    for (PlayerBotMap::const_iterator it = sRandomPlayerbotMgr.GetPlayerBotsBegin(); it != sRandomPlayerbotMgr.GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        if (bot->GetPlayerbotAI()->GetMaster() == GetMaster())
            bot->GetPlayerbotAI()->HandleMasterOutgoingPacket(packet);
    }
}

void PlayerbotMgr::SaveToDB()
{
    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        bot->SaveToDB();
    }
    for (PlayerBotMap::const_iterator it = sRandomPlayerbotMgr.GetPlayerBotsBegin(); it != sRandomPlayerbotMgr.GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        if (bot->GetPlayerbotAI()->GetMaster() == GetMaster())
            bot->SaveToDB();
    }
}

void PlayerbotMgr::OnBotLoginInternal(Player * const bot)
{
    bot->GetPlayerbotAI()->SetMaster(master);
    bot->GetPlayerbotAI()->ResetStrategies();

    if (bot->getLevel() > 5 && CountPlayerbotKnownSpells(bot) < 20)
    {
        PlayerbotFactory factory(bot, bot->getLevel());
        factory.TrainForLevel();
        bot->SaveToDB();
        if (master)
            bot->GetPlayerbotAI()->TellMaster("Trained missing class spells.");
    }

        if (master)
        {
            uint64 botGuid = bot->GetObjectGuid().GetRawValue();
            uint32 initLevel = master->getLevel();
            string initGearRole;

            map<uint64, string>::iterator pendingRole = pendingPlayerbotRoleCommands.find(botGuid);
            if (pendingRole != pendingPlayerbotRoleCommands.end())
                initGearRole = GetPlayerbotGearRoleFromRoleCommand(pendingRole->second);

            map<uint64, uint32>::iterator level = pendingPlayerbotInitLevels.find(botGuid);
            if (level != pendingPlayerbotInitLevels.end())
        {
            initLevel = level->second;
            pendingPlayerbotInitLevels.erase(level);
        }

        map<uint64, uint32>::iterator init = pendingPlayerbotInitQualities.find(botGuid);
        if (init != pendingPlayerbotInitQualities.end())
        {
            PlayerbotFactory factory(bot, initLevel, init->second);
            factory.Randomize(false);
            if (!initGearRole.empty())
                factory.GearOnly(initGearRole);
            pendingPlayerbotInitQualities.erase(init);
        }

        bool skipGroup = false;
        set<uint64>::iterator skip = pendingPlayerbotSkipGroup.find(botGuid);
        if (skip != pendingPlayerbotSkipGroup.end())
        {
            skipGroup = true;
            pendingPlayerbotSkipGroup.erase(skip);
        }

        if (!skipGroup)
        {
            string reason;
            if (!AddOnlinePlayerBotToMasterGroup(master, bot, &reason))
                bot->GetPlayerbotAI()->TellMaster(string("Party invite failed: ") + reason);
        }
        else
        {
            bot->GetPlayerbotAI()->TellMaster("Controlled without party.");
        }

        map<uint64, string>::iterator i = pendingPlayerbotRoleCommands.find(botGuid);
        if (i != pendingPlayerbotRoleCommands.end())
        {
            bot->GetPlayerbotAI()->HandleCommand(CHAT_MSG_WHISPER, i->second, *master);
            pendingPlayerbotRoleCommands.erase(i);
        }

    }

    sLog.outString("Bot %s logged in", bot->GetName());
}

void PlayerbotMgr::OnPlayerLogin(Player* player)
{
    if (!sPlayerbotAIConfig.botAutologin)
        return;

    uint32 accountId = player->GetSession()->GetAccountId();
    QueryResult* results = CharacterDatabase.PQuery(
        "SELECT name FROM characters WHERE account = '%u'",
        accountId);
    if (results)
    {
        ostringstream out; out << "add ";
        bool first = true;
        do
        {
            Field* fields = results->Fetch();
            if (first) first = false; else out << ",";
            out << fields[0].GetString();
        } while (results->NextRow());

        delete results;

        HandlePlayerbotCommand(out.str().c_str(), player);
    }
}

void PlayerbotMgr::TellError(string botName, string text)
{
    set<string> names = errors[text];
    if (names.find(botName) == names.end())
    {
        names.insert(botName);
    }
    errors[text] = names;
}

void PlayerbotMgr::CheckTellErrors(uint32 elapsed)
{
    time_t now = time(0);
    if ((now - lastErrorTell) < sPlayerbotAIConfig.errorDelay / 1000)
        return;

    lastErrorTell = now;

    for (PlayerBotErrorMap::iterator i = errors.begin(); i != errors.end(); ++i)
    {
        string text = i->first;
        set<string> names = i->second;

        ostringstream out;
        bool first = true;
        for (set<string>::iterator j = names.begin(); j != names.end(); ++j)
        {
            if (!first) out << ", "; else first = false;
            out << *j;
        }
        out << "|cfff00000: " << text;

        ChatHandler chat(master->GetSession());
        chat.PSendSysMessage(out.str().c_str());
    }
    errors.clear();
}
