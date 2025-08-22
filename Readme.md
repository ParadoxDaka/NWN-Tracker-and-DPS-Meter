# NWN Stats and Cooldown Tracker
By UnknownLight


**Video:**
[Watch on YouTube](https://www.youtube.com/watch?v=bMqkrmTklm4)

[Update on YouTube](https://www.youtube.com/watch?v=_Tl6ItbCvKY)


Also by UnknownLight is a map of Lost Souls Reborn:  
[View the Map](https://paradoxlight.neocities.org/)

This overlay only works if Nvidia ShadowPlay is running, it hooks into it as a way to not have another overlay running.

Features:
Server Restart timer
Total EXP
Total Damage
Total Kills
DPS
Current Level
Xp to next Level
XP/HR
Level % bar
Gold Earned this session
Gold / HR

How it works:

Reads and parses the log nwcliendLog1.txt, it looks for damages, gold, casts, exp etc. It needs your Player name for Total Damage and DPS etc, so put it in the config.

Hit Insert to bring up the config menu.

Set the logfile path in the config.txt

Example:
LogFilePath="C:\Users\CHANGE ME TO YOUR PC USERNAME\Documents\Neverwinter Nights\logs\nwclientLog1.txt"

Change CHANGE ME TO YOUR USERNAME to your username or set the path manualy.


Change CHANGE ME TO YOUR NWN USERNAME to your name in nwn.

UserName="CHANGE ME TO YOUR NWN NAME"

If it cant find the overlay, hit ALT+Z twice while in game and try to run the tracker again.


To set the Server restart time do !restart
To set your XP for the level info do  currentxp is XPVALUE  Example:  currentxp is 572543

It reads from the nwclientlog.txt chat and combatlog for stats and commands.

You might need to enable extra logs in the Settings.tml in C:\Users\CHANGE USERNAME\Documents\Neverwinter Nights

[game.log]
		[game.log.chat]
			[game.log.chat.all]
				enabled = true
			[game.log.chat.emotes]
				enabled = true
			[game.log.chat.text]
				enabled = false
		[game.log.debug]
			network = false
			nui = false
			scriptvm = false
		[game.log.resman]
			[game.log.resman.lookup-failures]
				enabled = false
				
Change the logs to enabled = true

You also might have to add 

[Client]
ClientEntireChatWindowLogging=1
[Communication]
DebugLog=1
DebugLogFile=logs/nwclientDebug.txt

To the nwn.ini in the same folder
