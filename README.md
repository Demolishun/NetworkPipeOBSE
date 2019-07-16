# NetworkPipeOBSE
Adds networking capabilities to the OBSE plugin for Oblivion

**Network Pipe OBSE**

This is a network plugin for the [OBSE](http://obse.silverlock.org/) add on app for the game Oblivion.  The 
plugin provides basic networking support exposed to the Oblivion scripting language.  The networking uses UDP and 
transmits data using [JSON](http://www.json.org/).  A mod using this plugin has complete control over the data 
passed in and out of the game and can use any external apps they choose.  This basically opens the door for external 
modding tools and programs designed to extend the capabilities of the game.  


**Some potential uses for the plugin:**

* Web enabling the game for blogging/journaling, website connectivity, IRC, etc.
* External apps adding access to databases and other data repositories.  This removes the limit of needing to use save files for some persistent data.
* Control the game for things like: machinimas, dungeon mastering, messing with friends, etc.
* Potential of controlling the game externally via a phone or other remote devices.
* The sky is the limit.


**Notes:**

* Compiled with Visual C++ Express 2010
* Boost library version 1.49
* OBSE version 0020 (will not compile against 0021)


**Todo:**

* ~~Host a binary version of the plugin for people to download~~.  Done: [Binary Plugin](http://oblivion.nexusmods.com/mods/43508)
* ~~Create a number of examples to show how the plugin is to be used and/or abuse. ;)~~ Done: see Binary Plugin link.
* ~~Post links to other websites so people know about the plugin.~~
