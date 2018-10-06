#include <iostream>
#include <ctime>
#include <random>
#include "halfmod.h"
#include "str_tok.h"
using namespace std;

#define VERSION		"v0.2.5"

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

static void (*initRtd)(hmHandle&,int (*)(hmHandle&,Effect));
int createNewEffect(hmHandle&,Effect);
static void (*mergePlugin)(hmHandle&);

static uint32_t (*mtrand)(uint32_t, uint32_t);

void setupEffects(hmHandle &handle);

extern "C" int onPluginStart(hmHandle &handle, hmGlobal *global)
{
    recallGlobal(global);
    handle.pluginInfo("Roll The Dice Base",
                      "nigel",
                      "Roll the dice for a random effect!",
                      VERSION,
                      "http://you.justca.me/to/roll/the/dice");
    
    bool found = false;
    for (auto it = global->extensions.begin(), ite = global->extensions.end();it != ite;++it)
    {
        if (it->getExtension() == "rollthedice")
        {
            *(void **) (&mtrand) = it->getFunc("mtrand");
            
            *(void **) (&initRtd) = it->getFunc("initRtdEffectFunc");
            (*initRtd)(handle,&createNewEffect);
            // only the above function is needed to add new effects to rtd
            // the following function should not be called from any other plugin
            *(void **) (&mergePlugin) = it->getFunc("mergePlugin");
            // seriously... just don't
            (*mergePlugin)(handle);
            setupEffects(handle);
            found = true;
            break;
        }
    }
    if (!found)
    {
        hmOutDebug("Missing `rollthedice` extension . . .");
        return 1;
    }
    
    return 0;
}

int delayedKill(hmHandle &handle, string client)
{
    hmSendRaw("kill " + client);
    return 1;
}

int delayedKillTeam(hmHandle &handle, string args)
{
    string client = gettok(args,1," "), team = gettok(args,2," ");
    hmSendRaw("kill " + client + "\nteam remove " + team);
    return 1;
}

int removeTeamAEC(hmHandle &handle, string args)
{
    string client = gettok(args,1," "), team = gettok(args,2," ");
    hmSendRaw("kill @e[type=minecraft:area_effect_cloud,tag=" + client + "]\nteam remove " + team);
    return 1;
}

int delayedExplode(hmHandle &handle, string client)
{
    hmSendRaw("execute at " + client + " run summon minecraft:creeper ~ ~ ~ {Fuse:0,ignited:1}");
    handle.createTimer("kill" + client,25,&delayedKill,client,MILLISECONDS);
    return 1;
}

int delayedExplodeKill(hmHandle &handle, string client)
{
    hmSendRaw("kill " + client + "\nexecute at " + client + " run summon minecraft:creeper ~ ~ ~ {Fuse:0,ignited:1}");
    return 1;
}

int delayedSmite(hmHandle &handle, string client)
{
    hmSendRaw("execute at " + client + " run summon minecraft:lightning_bolt");
    handle.createTimer("kill" + client,25,&delayedKill,client,MILLISECONDS);
    return 1;
}

int aecKeepup(hmHandle &handle, string client)
{
    hmSendRaw("teleport @e[type=minecraft:area_effect_cloud,tag=rocket,tag=" + client + "] " + client);
    return 1;
}

int delayedExplodeWith(hmHandle &handle, string args)
{
    string client = gettok(args,1," "), killer = gettok(args,2," ");
    string owner;
    hmPlayer *dat = hmGetPlayerPtr(killer);
    for (int i = 0, j = numtok(dat->custom,"\n");i < j;i++)
    {
        string tag = gettok(dat->custom,i+1,"\n");
        if (gettok(tag,1,"=") == "UUIDML")
            owner = gettok(tag,2,"=");
    }
    owner = strreplace(strreplace(owner,"L:","OwnerUUIDLeast:"),"M:","OwnerUUIDMost:");
    if (owner.size() > 2)
        owner = owner.substr(1,owner.size()-2);
    hmSendRaw("execute at " + client + " run summon minecraft:area_effect_cloud ~ ~5.5 ~ {" + owner + ",WaitTime:0,Duration:6,ReapplicationDelay:0,Radius:4.0f,Potion:\"minecraft:harming\",Tags:[\"" + client + "\",\"rocket\"]}\nexecute at " + client + " run summon minecraft:area_effect_cloud ~ ~ ~ {" + owner + ",WaitTime:0,Duration:6,ReapplicationDelay:0,Radius:4.0f,Potion:\"minecraft:harming\",Tags:[\"" + client + "\",\"rocket\"]}");
    //handle.createTimer("keepAEC",260,&aecKeepup,client,MILLISECONDS);
    handle.createTimer("kill" + client,325,&delayedExplodeKill,client,MILLISECONDS);
    return 1;
}

int effectSmite(hmHandle &handle, string client, float duration)
{
    delayedSmite(handle,client);
    return 0;
}

int effectHeal(hmHandle &handle, string client, float duration)
{
    string dur = to_string(int(duration+0.9999f));
    hmSendRaw("effect give " + client + " minecraft:regeneration " + dur + " 255 true\neffect give " + client + " minecraft:saturation " + dur + " 255 true");
    return 0;
}

int effectPearls(hmHandle &handle, string client, float duration)
{
    hmSendRaw("weather rain " + to_string(int(duration+0.9999f)));
    return 0;
}

int repeatPearls(hmHandle &handle, string client)
{
    string owner;
    hmPlayer *dat = hmGetPlayerPtr(client);
    for (int i = 0, j = numtok(dat->custom,"\n");i < j;i++)
    {
        string tag = gettok(dat->custom,i+1,"\n");
        if (gettok(tag,1,"=") == "UUIDML")
            owner = gettok(tag,2,"=");
    }
    int mot[3] = { int(mtrand(0,300000)), int(mtrand(5000,150000)), int(mtrand(0,300000)) };
    mot[0] -= 150000;
    mot[2] -= 150000;
    string motion = "Motion:[" + to_string(float(mot[0])/100000.0f) + "d," + to_string(float(mot[1])/100000.0f) + "d," + to_string(float(mot[2])/100000.0f) + "d]";
    hmSendRaw("execute at " + client + " run summon minecraft:ender_pearl ~ ~ ~ {owner:" + owner + "," + motion + "}");
    return 0;
}

int endPearls(hmHandle &handle, string client)
{
    hmSendRaw("weather clear");
    return 1;
}

int effectLowgrav(hmHandle &handle, string client, float duration)
{
    string dur = to_string(int(duration+0.9999f));
    hmSendRaw("effect give " + client + " minecraft:jump_boost " + dur + " 5 true\neffect give " + client + " minecraft:slow_falling " + dur + " 5 true");
    return 0;
}

int effectStarve(hmHandle &handle, string client, float duration)
{
    string dur = to_string(int(duration+0.9999f));
    hmSendRaw("effect give " + client + " minecraft:hunger " + dur + " 255 true");
    return 0;
}

int endStarve(hmHandle &handle, string client)
{
    hmSendRaw("effect give " + client + " minecraft:saturation 1 100 true");
    return 1;
}

int effectPack(hmHandle &handle, string client, float duration)
{
    string uuid = hmGetPlayerUUID(client);
    //string dur = to_string(int(duration));
    for (int i = 10;i;--i)
        hmSendRaw("execute at " + client + " run summon minecraft:wolf ~ ~ ~ {OwnerUUID:\"" + uuid + "\",CollarColor:" + to_string(mtrand(0,16)) + "b,Tags:[\"" + client + "\"]}");
    //hmSendRaw("effect give @e[type=minecraft:wolf,tag=" + client + "] minecraft:speed " + dur + " 5 true\neffect give @e[type=minecraft:wolf,tag=" + client + "] minecraft:jump_boost " + dur + " 10 true\neffect give @e[type=minecraft:wolf,tag=" + client + "] minecraft:slow_falling " + dur + " 10 true");
    return 0;
}

int endPack(hmHandle &handle, string client)
{
    hmSendRaw("execute as @e[type=minecraft:wolf,tag=" + client + "] run data merge entity @s {OwnerUUID:\"00000000-0000-0000-0000-00000000\"}\nkill @e[type=minecraft:wolf,tag=" + client + "]");
    return 1;
}

int effectTimebomb(hmHandle &handle, string client, float duration)
{
    hmReplyToClient(client,"You will explode in " + to_string(duration) + " seconds!");
    return 0;
}

int repeatTimebomb(hmHandle &handle, string client)
{
    //hmReplyToClient(client,"You will explode in 1 second!");
    hmSendRaw("execute at " + client + " run summon minecraft:area_effect_cloud ~ ~ ~ {WaitTime:0,Duration:10,ReapplicationDelay:0,Radius:0.1f,RadiusPerTick:0.5f,Particle:\"minecraft:lava\"}\nexecute at " + client + " run playsound entity.blaze.hurt master @a ~ ~ ~ 1.0");
    return 0;
}

int endTimebomb(hmHandle &handle, string client)
{
    string owner;
    hmPlayer *dat = hmGetPlayerPtr(client);
    for (int i = 0, j = numtok(dat->custom,"\n");i < j;i++)
    {
        string tag = gettok(dat->custom,i+1,"\n");
        if (gettok(tag,1,"=") == "UUIDML")
            owner = gettok(tag,2,"=");
    }
    string team;
    if (owner.size() > 0)
    {
        team = to_string(mtrand(1000,-1));
        owner = strreplace(strreplace(owner,"L:","OwnerUUIDLeast:"),"M:","OwnerUUIDMost:");
        owner = owner.substr(1,owner.size()-2);
        hmSendRaw("team add " + team + "\nteam option " + team + " friendlyfire false\nteam join " + team + " " + client + "\nexecute at " + client + " run summon minecraft:area_effect_cloud ~ ~ ~ {" + owner + ",WaitTime:0,Duration:6,ReapplicationDelay:0,Radius:4.0f,Potion:\"minecraft:harming\"}");
    }
    hmSendRaw("execute at " + client + " run summon minecraft:tnt ~ ~ ~ {Fuse:7s}");
    handle.createTimer("kill" + client,375,&delayedKillTeam,client + " " + team,MILLISECONDS);
    return 1;
}

int effectRocket(hmHandle &handle, string client, float duration)
{
    hmSendRaw("effect give " + client + " minecraft:levitation 5 25");
    handle.createTimer("explode" + client,3,&delayedExplode,client);
    return 0;
}

int effectRocketSmite(hmHandle &handle, string client, float duration)
{
    hmSendRaw("effect give " + client + " minecraft:levitation 5 25");
    handle.createTimer("smite" + client,3,&delayedSmite,client);
    return 0;
}

int effectExplode(hmHandle &handle, string client, float duration)
{
    delayedExplode(handle,client);
    return 0;
}

int effectInvis(hmHandle &handle, string client, float duration)
{
    hmSendRaw("effect give " + client + " minecraft:invisibility " + to_string(int(duration+0.9999f)));
    hmReplyToClient(client,"Tip: You might want to remove any armor you're wearing!");
    return 0;
}

int effectSpeed(hmHandle &handle, string client, float duration)
{
    hmSendRaw("effect give " + client + " minecraft:speed " + to_string(int(duration+0.9999f)) + " 10");
    return 0;
}

int effectJump(hmHandle &handle, string client, float duration)
{
    hmSendRaw("effect give " + client + " minecraft:jump_boost " + to_string(int(duration+0.9999f)) + " 10");
    return 0;
}

int effectNightVision(hmHandle &handle, string client, float duration)
{
    hmSendRaw("effect give " + client + " minecraft:night_vision " + to_string(int(duration+0.9999f)) + "\ntime set night");
    return 0;
}

int endNightVision(hmHandle &handle, string client)
{
    hmSendRaw("time set day");
    return 0;
}

int effectToxic(hmHandle &handle, string client, float duration)
{
    string team = to_string(mtrand(1000,-1));
    hmSendRaw("team add " + team + "\nteam option " + team + " friendlyfire false\nteam join " + team + " " + client);
    handle.createTimer(client + "#team",(int)duration*1000,&removeTeamAEC,client + " " + team,MILLISECONDS);
    return 0;
}

int repeatToxic(hmHandle &handle, string client)
{
    string owner;
    hmPlayer *dat = hmGetPlayerPtr(client);
    for (int i = 0, j = numtok(dat->custom,"\n");i < j;i++)
    {
        string tag = gettok(dat->custom,i+1,"\n");
        if (gettok(tag,1,"=") == "UUIDML")
            owner = gettok(tag,2,"=");
    }
    if (owner.size() > 0)
    {
        owner = strreplace(strreplace(owner,"L:","OwnerUUIDLeast:"),"M:","OwnerUUIDMost:");
        owner = owner.substr(1,owner.size()-2);
    }
    else
        owner = "OwnerUUIDLeast:0L,OwnerUUIDMost:0L";
    hmSendRaw("execute at " + client + " run summon minecraft:area_effect_cloud ~ ~ ~ {" + owner + ",WaitTime:0,Duration:6,ReapplicationDelay:0,Radius:4.0f,RadiusOnUse:0.0f,RadiusPerTick:0.0f,Effects:[{Amplifier:28b,Id:7b,ShowParticles:0b,Duration:999999}],Tags:[\"" + client + "\"]}");
    return 0;
}

int effectToxicSmite(hmHandle &handle, string client, float duration)
{
    hmSendRaw("tag " + client + " add toxicsmite");
    return 0;
}

int endToxicSmite(hmHandle &handle, string client)
{
    hmSendRaw("tag " + client + " remove toxicsmite");
    return 1;
}

int rocketHook(hmHandle &handle, hmHook hook, smatch args)
{
    string killer = args[1], client = args[2];
    hmSendRaw("effect give " + client + " minecraft:levitation 5 25\ntag " + client + " remove rocket_" + killer);
    handle.createTimer("explode" + client,2700,&delayedExplodeWith,client + " " + killer,MILLISECONDS);
    return 1;
}

int effectToxicRocket(hmHandle &handle, string client, float duration)
{
    handle.hookPattern("rocket_" + client,"^\\[[0-9]{2}:[0-9]{2}:[0-9]{2}\\] \\[Server thread/INFO\\]: Added tag 'rocket_([^']+)' to (.+)$",&rocketHook);
    return 0;
}

int repeatToxicRocket(hmHandle &handle, string client)
{
    hmSendRaw("execute at " + client + " as @a[name=!" + client + ",distance=..4] run tag @s add rocket_" + client);
    //hmSendRaw("execute at " + client + " as @a[distance=..4] run tag @s add rocket_" + client);
    return 0;
}

int endToxicRocket(hmHandle &handle, string client)
{
    handle.unhookPattern("rocket_" + client);
    //handle.killTimer("keepAEC");
    return 1;
}

int effectTrip(hmHandle &handle, string client, float duration)
{
    hmSendRaw("effect give " + client + " minecraft:nausea " + to_string(int(duration+0.9999f)));
    return 0;
}

int effectResist(hmHandle &handle, string client, float duration)
{
    hmSendRaw("effect give " + client + " minecraft:resistance " + to_string(int(duration+0.9999f)) + " 5");
    return 0;
}

int effectSlow(hmHandle &handle, string client, float duration)
{
    hmSendRaw("effect give " + client + " minecraft:slowness " + to_string(int(duration+0.9999f)) + " 5");
    return 0;
}

int effectWeak(hmHandle &handle, string client, float duration)
{
    hmSendRaw("effect give " + client + " minecraft:weakness " + to_string(int(duration+0.9999f)) + " 126");
    return 0;
}

int effectVolcano(hmHandle &handle, string client, float duration)
{
    string team = to_string(mtrand(1000,-1));
    hmSendRaw("team add " + team + "\nteam option " + team + " friendlyfire false\nteam join " + team + " " + client + "\neffect give " + client + " minecraft:fire_resistance " + to_string(int(duration+2.9999f)));
    handle.createTimer(client + "#team",(int)duration*1000,&removeTeamAEC,client + " " + team,MILLISECONDS);
    return 0;
}

int repeatVolcano(hmHandle &handle, string client)
{
    string owner;
    hmPlayer *dat = hmGetPlayerPtr(client);
    for (int i = 0, j = numtok(dat->custom,"\n");i < j;i++)
    {
        string tag = gettok(dat->custom,i+1,"\n");
        if (gettok(tag,1,"=") == "UUIDML")
            owner = gettok(tag,2,"=");
    }
    if (owner.size() > 0)
    {
        owner = strreplace(strreplace(owner,"L:","OwnerUUIDLeast:"),"M:","OwnerUUIDMost:");
        owner = owner.substr(1,owner.size()-2);
    }
    else
        owner = "OwnerUUIDLeast:0L,OwnerUUIDMost:0L";
    int mot[3] = { int(mtrand(0,70000))-35000, int(mtrand(15000,60000)), int(mtrand(0,70000))-35000 };
    string motion = "Motion:[" + to_string(float(mot[0])/100000.0f) + "d," + to_string(float(mot[1])/100000.0f) + "d," + to_string(float(mot[2])/100000.0f) + "d]";
    hmSendRaw("execute at " + client + " run summon minecraft:falling_block ~ ~ ~ {BlockState:{Name:\"minecraft:fire\"},Time:1," + motion + "}\nexecute at " + client + " run summon minecraft:area_effect_cloud ~ ~ ~ {" + owner + ",WaitTime:0,Duration:6,ReapplicationDelay:0,Radius:4.0f,RadiusOnUse:0.0f,RadiusPerTick:0.0f,Potion:\"minecraft:harming\",Particle:\"minecraft:flame\",Tags:[\"" + client + "\"]}");
    return 0;
}

void setupEffects(hmHandle &handle)
{
    Effect eff;
    eff.name = "smite";
    eff.message = "%s has been smote!";
    eff.setCallback = &effectSmite;
    (*createNewEffect)(handle,eff);
    eff.name = "health";
    eff.message = "%s has been given copious amounts of health!";
    eff.setCallback = &effectHeal;
    (*createNewEffect)(handle,eff);
    eff.name = "enderain";
    eff.message = "%s is now experiencing the pain of being an Enderman in the rain!";
    eff.setCallback = &effectPearls;
    eff.repeatCallback = &repeatPearls;
    eff.unsetCallback = &endPearls;
    eff.repeat = 0.2;
    (*createNewEffect)(handle,eff);
    eff.name = "lowgrav";
    eff.message = "%s is on the moon!";
    eff.setCallback = &effectLowgrav;
    eff.repeatCallback = nullptr;
    eff.unsetCallback = nullptr;
    eff.repeat = 0.0;
    (*createNewEffect)(handle,eff);
    eff.name = "starve";
    eff.message = "%s is starving to death!";
    eff.setCallback = &effectStarve;
    eff.unsetCallback = &endStarve;
    (*createNewEffect)(handle,eff);
    eff.name = "pack";
    eff.message = "%s is the leader of the pack!";
    eff.setCallback = &effectPack;
    eff.unsetCallback = &endPack;
    (*createNewEffect)(handle,eff);
    eff.name = "timebomb";
    eff.message = "%s has become a time bomb!";
    eff.setCallback = &effectTimebomb;
    eff.repeatCallback = &repeatTimebomb;
    eff.unsetCallback = &endTimebomb;
    eff.repeat = 2.0f;
    (*createNewEffect)(handle,eff);
    eff.name = "rocket";
    eff.message = "%s has been launched into space!";
    eff.setCallback = &effectRocket;
    eff.repeatCallback = nullptr;
    eff.unsetCallback = nullptr;
    eff.repeat = 0.0f;
    (*createNewEffect)(handle,eff);
    eff.name = "rocketsmite";
    eff.message = "%s has become a flying lightning rod!";
    eff.setCallback = &effectRocketSmite;
    (*createNewEffect)(handle,eff);
    eff.name = "explode";
    eff.message = "%s has exploded!";
    eff.setCallback = &effectExplode;
    (*createNewEffect)(handle,eff);
    eff.name = "invis";
    eff.message = "%s has become invisible!";
    eff.setCallback = &effectInvis;
    (*createNewEffect)(handle,eff);
    eff.name = "speed";
    eff.message = "%s scored a fat sack of speed!";
    eff.setCallback = &effectSpeed;
    (*createNewEffect)(handle,eff);
    eff.name = "jump";
    eff.message = "%s is a cute little bunny rabbit!";
    eff.setCallback = &effectJump;
    (*createNewEffect)(handle,eff);
    eff.name = "nightvision";
    eff.message = "%s can see in the dark!";
    eff.setCallback = &effectNightVision;
    eff.unsetCallback = &endNightVision;
    (*createNewEffect)(handle,eff);
    eff.name = "toxic";
    eff.message = "%s has become toxic!";
    eff.setCallback = &effectToxic;
    eff.repeatCallback = &repeatToxic;
    eff.unsetCallback = nullptr;
    eff.repeat = 0.05;
    (*createNewEffect)(handle,eff);
    eff.name = "toxicsmite";
    eff.message = "%s has become shockingly toxic!";
    eff.setCallback = &effectToxicSmite;
    eff.unsetCallback = &endToxicSmite;
    eff.repeatCallback = nullptr;
    eff.repeat = 0.0;
    (*createNewEffect)(handle,eff);
    eff.name = "toxicrocket";
    eff.message = "%s has become highly toxic!";
    eff.setCallback = &effectToxicRocket;
    eff.repeatCallback = &repeatToxicRocket;
    eff.unsetCallback = &endToxicRocket;
    eff.repeat = 0.05;
    (*createNewEffect)(handle,eff);
    eff.name = "nausea";
    eff.message = "%s is tripping out!";
    eff.setCallback = &effectTrip;
    eff.repeatCallback = nullptr;
    eff.unsetCallback = nullptr;
    eff.repeat = 0.0f;
    (*createNewEffect)(handle,eff);
    eff.name = "godmode";
    eff.message = "%s has received godmode!";
    eff.setCallback = &effectResist;
    (*createNewEffect)(handle,eff);
    eff.name = "slow";
    eff.message = "%s has been stripped of their mobility!";
    eff.setCallback = &effectSlow;
    (*createNewEffect)(handle,eff);
    eff.name = "weak";
    eff.message = "%s can no longer deal melee damage!";
    eff.setCallback = &effectWeak;
    (*createNewEffect)(handle,eff);
    eff.name = "volcano";
    eff.message = "%s is erupting like a volcano!";
    eff.setCallback = &effectVolcano;
    eff.repeatCallback = &repeatVolcano;
    eff.repeat = 0.05;
    (*createNewEffect)(handle,eff);
}

/*/           TODO Effects:
 X  Smite
 X  Regen
 X  Rocket
 X  Rocket smite
 X  Enderman in the rain (make them throw ender pearls everywhere)
 X  Toxic
 X  Toxic smite
 X  Toxic rocket
 X  Invisible
 X  Night vision (and make it night time)
 X  Speed
 X  Jump boost
 X  Explode
 X  Low gravity (jump boost + slow fall)
 X  Starving to death
 X  Pack leader (summon a pack of wolves around the player that get angered towards nearby players)
 X  Time bomb (summon charged creeper at player when duration runs out)
 X  Tripping out (nausea)
 X  Godmode (resistance)
 X  Slowness
 X  Weakness
 *  No fall damage
 *  Enemy glow? (if this can be done where only the player sees the glow)
 *  God of thunder/water? Thunderstorm, get tridents
 *  Pussy magnet
 *  Tag bomb (if can be done without too much complexity)
 *  Squashed by anvil
 *  Volcano (fire res + randomly launch falling fire from the player)
 *  Overflowing with health (health boost + regen self and surrounding players)
/*/

