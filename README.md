# RollTheDice
RollTheDice (RTD) is an extension/plugin combo for halfMod that is loosely based off of https://forums.alliedmods.net/showthread.php?t=75561  

The extension allows any plugin to add new effects to the dice, not just the included plugin.  

The included plugin acts as a base to start with. This plugin is mostly a Proof of Concept and also a major Work in Progress.  

Expect things to change a lot in future updates.  

# ConVars

Cvar | Default | Description
:--- | :---: | :---
`rtd_enable` | true | Enable or disable the RTD plugin.
`rtd_duration` | 20.0 | How long the effects will last.
`rtd_wait` | 45.0 | How long a player must wait between rolls.
`rtd_max` | 0.0 | How many players can roll at the same time? 0 for no limit.
`rtd_max_per_team` | 1.0 | How many players per team can roll at the same time? 0 for no limit. (Currently unused)

Compatable with the WebGUI Extension. Adds a button to enable/disable RTD.

# Commands

Command | Flag | Description
:--- | :---: | :---
`hm_rtd` | none | Roll the dice.
`hm_adminrtd <target> [effect] [duration]` | FLAG_CHEATS (x) | Allows admins to force a random or specific effect on a player for an optional amount of time.
`hm_effect_disable <effect>` | FLAG_CHEATS (x) | Disables an effect. Disabling an effect twice will disable it from random hm_adminrtd's too. A third time will even prevent admins from forcing it with `hm_adminrtd`. ~~'all' will disable all effects.~~
`hm_rtd_effect_enable` | FLAG_CHEATS (x) | Enables an effect. ~~'all' will enable all effects.~~
`hm_rtd_reset_teams` | FLAG_CHEATS (x) | Unused. Will be used to recheck every player's team.

# Effects

Effect | Description
:--- | :---
smite | Smites the player with lightning.
health | Health regenerates at an alarming rate.
enderain | Player learns how it feels to be an Enderman in the rain.
lowgrav | Player receives low gravity.
starve | Player gets extremely hungry and might die.
pack | Player receives a pack of tamed wolves.
timebomb | Player turns into a timebomb, set to blow when the effect wears off.
rocket | Player is launched into space and explodes.
rocketsmite | Player is launched into a storm cloud.
explode | Player explodes.
invis | Player receives invisibility.
speed | Player receives a generous speed boost.
jump | Player receives a generous jump boost.
nightvision | Player receives night vision.
toxic | Anyone within a small radius of the player will be killed by Player for the duration of the effect.
toxicsmite | Anyone within a small radius of the player will be smote.
toxicrocket | Anyone within a small radius of the player will be rocketed.
nausea | Player becomes nauseous.
godmode | Player does not take damage.
slow | Player can barely move.
weak | Player can no longer deal melee damage.
volcano | Player becomes a living, breathing volcano. Launching fire and killing anything in its path.

# Installation
Installation is very easy as it has no dependencies other than the halfmod API.  

After copying the repo into your halfMod install, just run the following commands:
```sh
cd src/extensions
./compile.sh --install rollthedice.cpp
cd ../plugins
./compile.sh --install rtd_base.cpp
```
