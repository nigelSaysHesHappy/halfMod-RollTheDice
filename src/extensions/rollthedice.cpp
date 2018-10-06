#include <iostream>
#include <ctime>
#include <random>
#include "halfmod.h"
#include "str_tok.h"
#include "nbtmap.h"
using namespace std;

#define VERSION		"v0.0.7"

struct Effect
{
    string name;
    string message;
    int (*setCallback)(hmHandle&,std::string,float);
    int (*repeatCallback)(hmHandle&,std::string);
    int (*unsetCallback)(hmHandle&,std::string);
    float repeat = 0.0;
    short disabled = 0;
    time_t afflicted;
};

struct Status
{
    string client;
    string team;
    string color = "green";
    vector<Effect> effects;
    vector<Effect> stats;
    vector<string> timers;
    time_t last = 0;
    void reset()
    {
        client.clear();
        team.clear();
        color = "green";
        effects.clear();
        stats.clear();
        timers.clear();
        last = 0;
    }
    void resetTimers(hmHandle &handle)
    {
        for (auto ti = timers.begin();ti != timers.end();)
        {
            bool found = false;
            for (auto it = handle.timers.begin();it != handle.timers.end();)
            {
                if (*ti == it->name)
                {
                    it = handle.timers.erase(it);
                    handle.invalidTimers = true;
                    found = true;
                }
                else
                    ++it;
            }
            if (found)
                ti = timers.erase(ti);
            else
                ++ti;
        }
        timers.clear();
    }
    void resetHandle(hmHandle &handle)
    {
        resetTimers(handle);
        reset();
    }
};

vector<Effect> rtdEffects;
vector<Status> playerStatus;

bool diceEnabled = true;
float diceDuration = 20.0;
float diceWait = 45.0;
int diceMaxEffects = 0;
int diceMaxEffectsTeam = 1;

static void (*addConfigButtonCallback)(string,string,int,std::string (*)(std::string,int,std::string,std::string));

string toggleButton(string name, int socket, string ip, string client)
{
    if (diceEnabled)
    {
        hmFindConVar("rtd_enable")->setBool(false);
        return "RTD plugin is now disabled . . .";
    }
    hmFindConVar("rtd_enable")->setBool(true);
    return "RTD plugin is now enabled . . .";
}

int cEnabled(hmConVar &cvar, string oldVar, string newVar)
{
    diceEnabled = cvar.getAsBool();
    return 0;
}
int cDuration(hmConVar &cvar, string oldVar, string newVar)
{
    diceDuration = cvar.getAsFloat();
    return 0;
}
int cWaitTime(hmConVar &cvar, string oldVar, string newVar)
{
    diceWait = cvar.getAsFloat();
    return 0;
}
int cMaxEffects(hmConVar &cvar, string oldVar, string newVar)
{
    diceMaxEffects = cvar.getAsInt();
    return 0;
}
int cMaxPerTeam(hmConVar &cvar, string oldVar, string newVar)
{
    diceMaxEffectsTeam = cvar.getAsInt();
    return 0;
}

int doStartup(hmExtension &handle, string nothing);
int hasConnected(hmExtension &handle, hmExtHook hook, smatch args);

extern "C" int onExtensionLoad(hmExtension &handle, hmGlobal *global)
{
    recallGlobal(global);
    handle.extensionInfo("Roll The Dice Engine",
                         "nigel",
                         "Roll the dice for a random effect!",
                         VERSION,
                         "http://you.justca.me/to/roll/the/dice");
    
    handle.hookConVarChange(handle.createConVar("rtd_enable","true","Enable or disable the RTD plugin.",FCVAR_NOTIFY,true,0.0,true,1.0),&cEnabled);
    handle.hookConVarChange(handle.createConVar("rtd_duration","20.0","How long the effects will last.",FCVAR_NOTIFY,true,0.0),&cDuration);
    handle.hookConVarChange(handle.createConVar("rtd_wait","45.0","How long a player must wait between rolls.",FCVAR_NOTIFY,true,0.0),&cWaitTime);
    handle.hookConVarChange(handle.createConVar("rtd_max","0.0","How many players can roll at the same time? 0 for no limit.",FCVAR_NOTIFY,true,0.0),&cMaxEffects);
    handle.hookConVarChange(handle.createConVar("rtd_max_per_team","1.0","How many players per team can roll at the same time? 0 for no limit.",FCVAR_NOTIFY,true,0.0),&cMaxPerTeam);
    
    if (global->maxPlayers)
        doStartup(handle,"");
    else
        handle.hookPattern("load","^\\[[0-9]{2}:[0-9]{2}:[0-9]{2}\\] \\[Server thread/INFO\\]: There are [0-9]{1,} of a max [0-9]{1,} players online:.*$",&hasConnected);
    
    return 0;
}

extern "C" uint32_t mtrand(uint32_t lo = 0, uint32_t hi = -1)
{
    static std::mt19937 mt;// (std::chrono::system_clock::now().time_since_epoch().count());
    static bool seeded = false;
    if (!seeded)
    {
        std::random_device rd;
        std::vector<uint32_t> seed;
        for (int i = 64;i;--i)
            seed.push_back(rd());
        std::seed_seq seeq (seed.begin(),seed.end());
        mt.seed(seeq);
        seeded = true;
    }
    uint32_t r;
    if (lo >= hi)
        r = lo;
    else
        r = (std::uniform_int_distribution<uint32_t>(lo,hi))(mt);
    return r;
}

void resetStatus(hmHandle &handle)
{
    for (auto it = playerStatus.begin(), ite = playerStatus.end();it != it;++it)
        it->resetHandle(handle);
}

void resetStatus()
{
    playerStatus.clear();
    Status empty;
    for (auto it = recallGlobal(NULL)->players.begin(), ite = recallGlobal(NULL)->players.end();it != ite;++it)
    {
        empty.client = it->second.name;
        playerStatus.push_back(empty);
    }
}

void resetStatusSingleSoft(hmHandle &handle, vector<Status>::iterator it)
{
    it->effects.clear();
    it->stats.clear();
    it->resetTimers(handle);
}

void resetStatusSingleSoft(hmHandle &handle, string client)
{
    for (auto it = playerStatus.begin(), ite = playerStatus.end();it != ite;++it)
    {
        if (it->client == client)
        {
            resetStatusSingleSoft(handle,it);
            break;
        }
    }
}

void resetStatusSingle(hmHandle &handle, string client)
{
    for (auto it = playerStatus.begin(), ite = playerStatus.end();it != ite;++it)
    {
        if (it->client == client)
        {
            it->resetHandle(handle);
            break;
        }
    }
}

void newStatus(string client)
{
    for (auto it = playerStatus.begin(), ite = playerStatus.end();it != ite;++it)
    {
        if (it->client.size() < 1)
        {
            it->client = client;
            return;
        }
    }
    Status empty;
    empty.client = client;
    playerStatus.push_back(empty);
}

int doStartup(hmExtension &handle, string nothing)
{
    hmGlobal *global = recallGlobal(NULL);
    for (auto it = global->extensions.begin(), ite = global->extensions.end();it != ite;++it)
    {
        if (it->getExtension() == "webgui")
        {
            *(void **) (&addConfigButtonCallback) = it->getFunc("addConfigButtonCallback");
            (*addConfigButtonCallback)("toggleRTD","Toggle RTD Plugin",FLAG_CVAR,&toggleButton);
            break;
        }
    }
    resetStatus();
    return 1;
}

int hasConnected(hmExtension &handle, hmExtHook hook, smatch args)
{
    handle.createTimer("load",0,&doStartup);
    handle.unhookPattern(hook.name);
    return 0;
}

int lookupUUID(hmHandle &handle, hmHook hook, smatch args)
{
    handle.unhookPattern(hook.name);
    string client = args[1].str();
    NBTCompound nbt = args[2].str();
    string owner = "{L:" + nbt.get("UUIDLeast") + ", M:" + nbt.get("UUIDMost") + "}";
    hmPlayer *dat = hmGetPlayerPtr(client);
    dat->custom = addtok(dat->custom,"UUIDML=" + owner,"\n");
    hmWritePlayerDat(stripFormat(lower(client)),"UUIDML=" + owner,"UUIDML",true);
    return 1;
}

int onPlayerJoin(hmHandle &handle, smatch args)
{
    // make this get player's uuid least/most and store it in player data
    string client = args[1].str();
    bool found = false;
    hmPlayer *dat = hmGetPlayerPtr(client);
    for (int i = numtok(dat->custom,"\n");i;--i)
    {
        if (istok(dat->custom,"UUIDML","="))
        {
            found = true;
            break;
        }
    }
    if (!found)
    {
        handle.hookPattern("UUID " + client,"^\\[[0-9]{2}:[0-9]{2}:[0-9]{2}\\] \\[Server thread/INFO\\]: (" + client + ") has the following entity data: (\\{.*\\})$",&lookupUUID);
        hmSendRaw("data get entity " + client);
    }
    newStatus(args[1].str());
    return 0;
}

int onPlayerLeave(hmHandle &handle, smatch args)
{
    string client = args[1].str();
    for (auto it = playerStatus.begin(), ite = playerStatus.end();it != ite;++it)
    {
        if (it->client == client)
        {
            if (it->effects.size() > 0)
            {
                hmSendRaw("tellraw @a [\"[RTD] \",{\"text\":\"" + client + "\",\"color\":\"" + it->color + "\"},{\"text\":\" left while using the dice.\"}]");
                for (auto eff = it->effects.begin(), effe = it->effects.end();eff != effe;++eff)
                    if (eff->unsetCallback != nullptr)
                        (*eff->unsetCallback)(handle,client);
            }
            it->resetHandle(handle);
            //if (it->timers.size() < 1)
            //    it->reset();
            break;
        }
    }
    return 0;
}

int onPlayerDeath(hmHandle &handle, smatch args)
{
    string client = args[1].str();
    auto it = playerStatus.begin(), ite = playerStatus.end();
    for (;it != ite;++it)
    {
        if (it->client == client)
        {
            if (it->effects.size() > 0)
            {
                for (auto eff = it->effects.begin(), effe = it->effects.end();eff != effe;++eff)
                    if (eff->unsetCallback != nullptr)
                        (*eff->unsetCallback)(handle,client);
                hmSendRaw("tellraw @a [\"[RTD] \",{\"text\":\"" + client + "\",\"color\":\"" + it->color + "\"},{\"text\":\" has died while using the dice.\"}]");
            }
            resetStatusSingleSoft(handle,it);
            break;
        }
    }
    if (it == ite)
        newStatus(client);
    return 0;
}

int stopTimer(hmHandle &handle, string timer)
{
    handle.killTimer(timer);
    string client = gettok(timer,1,"\n");
    string effect = gettok(timer,3,"\n");
    string other = client + "\nremove\n" + effect;
    for (auto it = playerStatus.begin(), ite = playerStatus.end();it != ite;++it)
    {
        if (it->client == client)
        {
            for (auto itt = it->timers.begin();itt != it->timers.end();)
            {
                if ((*itt == other) || (*itt == timer))
                    itt = it->timers.erase(itt);
                else
                    ++itt;
            }
            for (auto itt = it->effects.begin();itt != it->effects.end();)
            {
                if (itt->name == effect)
                    itt = it->effects.erase(itt);
                else
                    ++itt;
            }
            //hmSendRaw("tellraw @a [\"[RTD] \",{\"text\":\"" + client + "\",\"color\":\"" + it->color + "\"},{\"text\":\"'s effect has worn off.\"}]");
            break;
        }
    }
    return 1;
}

int commandRoll(hmHandle &handle, const hmPlayer &client, string args[], int argc)
{
    if ((diceEnabled) && (hmIsPlayerOnline(client.name)))
    {
        time_t cTime = time(NULL);
        int active = 0;
        time_t last;
        vector<Status>::iterator player;
        for (auto it = playerStatus.begin(), ite = playerStatus.end();it != ite;++it)
        {
            if (it->effects.size() > 0)
                active++;
            if (it->client == client.name)
            {
                last = it->last;
                player = it;
            }
        }
        if ((last=last+(int)diceWait) > cTime)
        {
            hmSendRaw("tellraw " + client.name + " [\"[RTD] \",{\"text\":\"You must wait \"},{\"text\":\"" + to_string(int(last-cTime)) + "\",\"color\":\"yellow\"},{\"text\":\" more seconds.\",\"color\":\"none\"}]");
            return 1;
        }
        if ((diceMaxEffects) && (active >= diceMaxEffects))
        {
            hmSendRaw("tellraw " + client.name + " [\"[RTD] \",{\"text\":\"Too many players are currently using the dice.\"}]");
            return 1;
        }
        player->last = cTime;
        vector<vector<Effect>::iterator> enabled;
        for (auto it = rtdEffects.begin(), ite = rtdEffects.end();it != ite;++it)
            if (!it->disabled)
                enabled.push_back(it);
        if (enabled.size() > 0)
        {
            Effect effect = **(enabled.begin()+mtrand(0,enabled.size()));
            effect.afflicted = cTime;
            bool found = false;
            for (auto it = player->effects.begin(), ite = player->effects.end();it != ite;++it)
            {
                if (it->name == effect.name)
                {
                    it->afflicted = cTime;
                    found = true;
                    break;
                }
            }
            if ((effect.unsetCallback != nullptr) || (effect.repeat > 0.0))
            {
                if (!found)
                    player->effects.push_back(effect);
                handle.killTimer(client.name + "\nkill\n" + effect.name);
                handle.createTimer(client.name + "\nkill\n" + effect.name,long(diceDuration*1000.0f),&stopTimer,client.name + "\nrepeat\n" + effect.name,MILLISECONDS);
            }
            string outMsg = strreplace(effect.message,"%s","\"},{\"text\":\"" + client.name + "\",\"color\":\"" + player->color + "\"},{\"color\":\"none\",\"text\":\"");
            hmSendRaw("tellraw @a [{\"text\":\"[RTD] " + outMsg + "\"}]");
            (*effect.setCallback)(handle,client.name,diceDuration);
            if (effect.unsetCallback != nullptr)
            {
                if (handle.timers.size() == handle.killTimer(client.name + "\nremove\n" + effect.name))
                    player->timers.push_back(client.name + "\nremove\n" + effect.name);
                long interval = long(diceDuration*1000.0f)+1;
                handle.createTimer(client.name + "\nremove\n" + effect.name,interval,effect.unsetCallback,client.name,MILLISECONDS);
            }
            if ((effect.repeat > 0.0) && (effect.repeatCallback != nullptr))
            {
                if (handle.timers.size() == handle.killTimer(client.name + "\nrepeat\n" + effect.name))
                    player->timers.push_back(client.name + "\nrepeat\n" + effect.name);
                long interval = long(effect.repeat*1000.0f);
                handle.createTimer(client.name + "\nrepeat\n" + effect.name,interval,effect.repeatCallback,client.name,MILLISECONDS);
                //player->timers.push_back(client.name + "\nkill\n" + effect.name);
            }
        }
        else
            hmSendRaw("tellraw " + client.name + " [\"[RTD] \",{\"text\":\"There are no sides to these die!\"}]");
    }
    return 0;
}

int commandAdminRoll(hmHandle &handle, const hmPlayer &client, string args[], int argc)
{
    // hm_adminrtd <user> [effect] [duration]
    if (argc < 2)
    {
        string elist = "Valid effects: ";
        for (auto it = rtdEffects.begin(), ite = rtdEffects.end();it != ite;++it)
            if (it->disabled < 3)
                elist.append("'" + it->name + "', ");
        if (rtdEffects.size() > 0)
            elist.erase(elist.size()-2,2);
        else
            elist = "No effects loaded :(";
        hmReplyToClient(client,"Usage: " + args[0] + " <target> [effect] [duration]");
        hmReplyToClient(client,elist);
    }
    else
    {
        time_t cTime = time(NULL);
        vector<hmPlayer> targs;
	    int targn = hmProcessTargets(client,args[1],targs,FILTER_NAME|FILTER_NO_SELECTOR);
	    if (targn < 1)
		    hmReplyToClient(client,"No matching players found.");
	    else
	    {
	        auto eff = rtdEffects.begin();
	        bool validEff = false;
	        float dur = diceDuration;
	        Effect effect;
	        if (argc > 2)
	        {
	            for (auto ite = rtdEffects.end();eff != ite;++eff)
	            {
	                if ((eff->name == args[2]) && (eff->disabled < 2))
	                {
	                    validEff = true;
	                    break;
                    }
                }
                if ((argc > 3) && (isdigit(args[3].at(0))))
                    dur = stof(args[3]);
            }
            long duration = dur*1000.0f;
            if ((!validEff) && (eff == rtdEffects.end()))
            {
                string elist = "Valid effects: ";
                for (auto it = rtdEffects.begin(), ite = rtdEffects.end();it != ite;++it)
                    if (it->disabled < 3)
                        elist.append("'" + it->name + "', ");
                if (rtdEffects.size() > 0)
                    elist.erase(elist.size()-2,2);
                else
                    elist = "No effects loaded :(";
                hmReplyToClient(client,args[0] + ": Invalid effect name.");
                hmReplyToClient(client,elist);
            }
            else
            {
                vector<vector<Effect>::iterator> enabled;
                if (!validEff)
                {
                    for (auto it = rtdEffects.begin(), ite = rtdEffects.end();it != ite;++it)
                        if (it->disabled < 2)
                            enabled.push_back(it);
                }
                else
                    enabled.push_back(eff);
                if (enabled.size() < 1)
                    hmReplyToClient(client,"No effects loaded :(");
                else
                {
                    for (auto target = targs.begin(), targete = targs.end();target != targete;++target)
                    {
                        effect = **(enabled.begin()+mtrand(0,enabled.size()));
                        effect.afflicted = cTime;
                        for (auto player = playerStatus.begin(), ite = playerStatus.end();player != ite;++player)
                        {
                            if (player->client == target->name)
                            {
                                player->last = cTime;
                                bool found = false;
                                for (auto it = player->effects.begin(), ite = player->effects.end();it != ite;++it)
                                {
                                    if (it->name == effect.name)
                                    {
                                        it->afflicted = cTime;
                                        found = true;
                                        break;
                                    }
                                }
                                if ((effect.unsetCallback != nullptr) || (effect.repeat > 0.0))
                                {
                                    if (!found)
                                        player->effects.push_back(effect);
                                    handle.killTimer(player->client + "\nkill\n" + effect.name);
                                    handle.createTimer(player->client + "\nkill\n" + effect.name,duration,&stopTimer,player->client + "\nrepeat\n" + effect.name,MILLISECONDS);
                                }
                                string outMsg = strreplace(effect.message,"%s","\"},{\"text\":\"" + player->client + "\",\"color\":\"" + player->color + "\"},{\"color\":\"none\",\"text\":\"");
                                hmSendRaw("tellraw @a [{\"text\":\"[RTD] " + outMsg + "\"}]");
                                (*effect.setCallback)(handle,player->client,dur);
                                if (effect.unsetCallback != nullptr)
                                {
                                    if (handle.timers.size() == handle.killTimer(player->client + "\nremove\n" + effect.name))
                                        player->timers.push_back(player->client + "\nremove\n" + effect.name);
                                    handle.createTimer(player->client + "\nremove\n" + effect.name,duration,effect.unsetCallback,player->client,MILLISECONDS);
                                }
                                if ((effect.repeat > 0.0) && (effect.repeatCallback != nullptr))
                                {
                                    if (handle.timers.size() == handle.killTimer(player->client + "\nrepeat\n" + effect.name))
                                        player->timers.push_back(player->client + "\nrepeat\n" + effect.name);
                                    long interval = long(effect.repeat*1000.0f);
                                    handle.createTimer(player->client + "\nrepeat\n" + effect.name,interval,effect.repeatCallback,player->client,MILLISECONDS);
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}

int commandDisableEffect(hmHandle &handle, const hmPlayer &client, string args[], int argc)
{
    if (argc < 2)
    {
        hmReplyToClient(client,"Usage: " + args[0] + " <effect>");
        hmReplyToClient(client,"Disabling an effect once disables it for 'hm_rtd'.");
        hmReplyToClient(client,"Twice disables it from random 'hm_adminrtd'.");
        hmReplyToClient(client,"Disabling it a third time disables it from being given with 'hm_adminrtd <user> <effect>'.");
    }
    else for (auto it = rtdEffects.begin(), ite = rtdEffects.end();it != ite;++it)
    {
        if (it->name == args[1])
        {
            if (it->disabled > 2)
                hmReplyToClient(client,"'" + args[1] + "' is already completely disabled.");
            else switch (++it->disabled)
            {
                case 1:
                {
                    hmReplyToClient(client,"'" + args[1] + "' will no longer be chosen by 'hm_rtd'.");
                    break;
                }
                case 2:
                {
                    hmReplyToClient(client,"'" + args[1] + "' will no longer be chosen randomly by 'hm_adminrtd'.");
                    break;
                }
                case 3:
                {
                    hmReplyToClient(client,"'" + args[1] + "' is now completely disabled.");
                    break;
                }
            }
            break;
        }
    }
    return 0;
}

int commandEnableEffect(hmHandle &handle, const hmPlayer &client, string args[], int argc)
{
    if (argc < 2)
        hmReplyToClient(client,"Usage: " + args[0] + " <effect>");
    else for (auto it = rtdEffects.begin(), ite = rtdEffects.end();it != ite;++it)
    {
        if (it->name == args[1])
        {
            if (it->disabled == 0)
                hmReplyToClient(client,"'" + args[1] + "' is already enabled.");
            else 
            {
                it->disabled = 0;
                hmReplyToClient(client,"'" + args[1] + "' is now enabled.");
            }
            break;
        }
    }
    return 0;
}

int commandTeamSet(hmHandle &handle, const hmPlayer &client, string args[], int argc)
{
    hmReplyToClient(client,"Not implemented yet.");
    // also make this get all players uuid least/most and set it in their player data
    return 0;
}

int onRTDPluginEnd(hmHandle &handle)
{
    string effects;
    for (int i = numtok(handle.vars,"\n"), effect = findtok(handle.vars,"START RTD UNLOAD PROCEDURE",1,"\n")+1;effect <= i;effect++)
        effects = appendtok(effects,gettok(handle.vars,effect,"\n"),"\n");
    for (auto it = rtdEffects.begin();it != rtdEffects.end();)
    {
        if (istok(effects,it->name,"\n"))
            it = rtdEffects.erase(it);
        else
            ++it;
    }
    for (auto it = playerStatus.begin(), ite = playerStatus.end();it != ite;++it)
        it->resetTimers(handle);
    //cout<<"Unloaded all effects created by plugin: "<<handle.getPlugin()<<" ... rtfEffects.size() == "<<rtdEffects.size()<<"\n";
    return 0;
}

int createNewEffect(hmHandle &handle, Effect eff)
{
    for (auto it = rtdEffects.begin(), ite = rtdEffects.end();it != ite;++it)
    {
        if (it->name == eff.name)
        {
            hmOutDebug("RTD: Error, an effect with the name \"" + eff.name + "\" already exists . . .");
            return 0;
        }
    }
    handle.vars = appendtok(handle.vars,eff.name,"\n");
    rtdEffects.push_back(eff);
    return rtdEffects.size();
}

extern "C" void initRtdEffectFunc(hmHandle &handle, int (*func)(hmHandle&,Effect))
{
    handle.hookEvent(208,&onRTDPluginEnd);
    if (handle.vars.size() > 0)
        handle.vars.append("\n");
    handle.vars.append("START RTD UNLOAD PROCEDURE\n");
    func = &createNewEffect;
}

extern "C" void mergePlugin(hmHandle &handle)
{
    handle.hookEvent(16,&onPlayerJoin);
    handle.hookEvent(56,&onPlayerLeave);
    handle.hookEvent(112,&onPlayerDeath);
    
    handle.regConsoleCmd("hm_rtd",&commandRoll,"Roll the dice!");
    
    handle.regAdminCmd("hm_adminrtd",&commandAdminRoll,FLAG_CHEATS,"Allows admins to force a random or specific effect on a player for an optional amount of time.");
    handle.regAdminCmd("hm_rtd_effect_disable",&commandDisableEffect,FLAG_CHEATS,"Disables one or more of the effects. Disabling an effect twice will disable it from random hm_adminrtd's too. 'all' will disable all effects.");
    handle.regAdminCmd("hm_rtd_effect_enable",&commandEnableEffect,FLAG_CHEATS,"Enables one or more of the effects. 'all' will enable all effects.");
    handle.regAdminCmd("hm_rtd_reset_teams",&commandTeamSet,FLAG_CHEATS,"Recheck every player's team.");
}











