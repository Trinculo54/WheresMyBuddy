WheresMyBuddy
=============

Your real Xbox friends as the Drivatars in Forza Horizon 2 (Azimuth).

FH2 names its AI drivers after 25 random players, and the servers that
used to send real Drivatars are long gone. WheresMyBuddy signs in to Xbox
Live, gets the people you follow, and gives their gamertags to those
Drivatars instead (favourites first), in free roam, championships, road
trips and car meets. It can also set your own in-game name and avatar to
your gamertag and gamerpic.

Project page and source: https://github.com/Trinculo54/WheresMyBuddy


Requirements
~~~~~~~~~~~~

  - Forza Horizon 2 running through Azimuth.
  - FH2ML, the Forza Horizon 2 mod loader, which loads WheresMyBuddy.
    https://github.com/Refilewski/FH2ML
  - Forza Horizon 2 Extra Options, with EnableMediaOverrides=1 in
    FH2_ExtraOptions.ini (the default). It's what lets the game load the
    new names.
    https://github.com/RokkuDayo/forzahorizon2_extraoptions


Install
~~~~~~~

1. Close Azimuth.
2. Install the zip with the FH2ML Manager (Mods > Install mod, and pick
   the zip), or extract it into your Azimuth folder (the one with
   azimuth.exe). Either way you end up with
       mods\wheresmybuddy\WheresMyBuddy.dll   (and its mod.json)
3. Start Azimuth and FH2. Your browser opens the Microsoft sign-in page.
   Sign in with the account you use on Xbox and allow access. You only
   do this once.
4. Restart the game. The Drivatars now have your friends' names, and your
   in-game name and avatar match your Xbox profile.

The first launch only fetches your friends because the game reads its
Drivatar list before the sign-in can finish. From then on the saved list
is used straight away and refreshed in the background every launch.

Coming from WheresMyBuddy 1.7 or earlier: those ran on a different
loader, and FH2ML doesn't start its mods while that one is still
installed. The FH2ML Manager shows it and removes it for you. On its first
launch under FH2ML, WheresMyBuddy copies your settings, friends.txt,
saved friends list and sign-in from the old wheresmybuddy folder next to
azimuth.exe, so there's nothing to redo. After that you can delete the
old folder.


Settings (FH2ML\data\wheresmybuddy\WheresMyBuddy.ini)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Everything the mod keeps lives in FH2ML\data\wheresmybuddy, in your
Azimuth folder, and stays there when you update or reinstall the mod.
The ini is created on first launch. Changes apply the next time you start
Azimuth. The main on/off switches are also in the game, under Pause >
Settings > MODS (the game offers to restart for you when you change one).

  [names]
  enabled=1          0 = mod off, original Drivatars come back
  max_names=25       how many of the 25 Drivatar names to replace
  repeat_to_fill=0   1 = reuse your names when you have fewer than max_names
  favorites_first=1  your Xbox favourites go first (and always stay in)
  rotate=1           more friends than max_names: a different set each
                     launch so everyone gets a turn; 0 = always the same

  [xbox]
  enabled=1          0 = don't sign in, only use friends.txt
  browser_sign_in=1  0 = enter a code at microsoft.com/link instead
  client_id=...      Azure app used for sign-in; leave as is

  [profile]
  player_name=once   once / always / off
  gamerpic=once      once / always / off
                     "once" sets it after your first sign-in and then
                     leaves it alone, so you can still change it yourself.
                     Delete profile-applied.ini to do it again.

friends.txt (in the same folder): extra names, one per line, 15
characters max. These always come before your Xbox friends.


Updates
~~~~~~~

When a newer version is out, FH2ML says so on the title screen, after
"PRESS (A) TO START", and on its MODS tab. To stop it checking GitHub for
WheresMyBuddy, add this to FH2ML\config\FH2ML.json:
    "mods": {"wheresmybuddy": {"updateChecks": false}}
(or "updateChecks": false to stop it for every mod). The [updates]
section older versions put in WheresMyBuddy.ini is removed from it.


Privacy
~~~~~~~

The mod only asks Microsoft for your Xbox profile and friends list, and
only talks to Microsoft and Xbox Live. Your sign-in is kept in
FH2ML\data\wheresmybuddy\account.dat, encrypted with Windows' data
protection so only your Windows account can read it. To revoke access,
remove WheresMyBuddy at https://account.live.com/consent/Manage


Troubleshooting
~~~~~~~~~~~~~~~

Everything is logged to FH2ML\data\wheresmybuddy\wheresmybuddy.log (fresh
each launch). Useful lines:

  signed in to Xbox Live as <you>      the sign-in works
  got N people from Xbox Live          your friends list arrived
  wrote ... with N of 25 Drivatar names replaced
                                       the Drivatar list was rewritten
  AI driver names (car meets, ...)      car meet / road trip names too
  championship Drivatar X -> Y          an older championship got renamed

Still the old names: launch a second time, and check that Extra Options is
installed with EnableMediaOverrides=1 (the log warns if it isn't).
Wrong account: delete FH2ML\data\wheresmybuddy\account.dat and launch
again.
Microsoft error page in the browser: set browser_sign_in=0.
No FH2ML\data\wheresmybuddy folder at all: check FH2ML\logs\FH2ML.log,
which says whether FH2ML loaded the mod (look for "wheresmybuddy:").


Uninstall
~~~~~~~~~

Set enabled=0 under [names] and launch once (restores the original
Drivatars), then uninstall WheresMyBuddy in the FH2ML Manager (Mods >
Uninstall), or delete the mods\wheresmybuddy folder. Your settings stay in
FH2ML\data\wheresmybuddy; delete that folder too if you won't be back.
Your previous avatar is saved next to FH2_ExtraOptions.ini as
Gamerpic.before-wheresmybuddy.png.


License
~~~~~~~

GPLv3. Unofficial fan-made mod; not affiliated with Playground Games,
Turn 10, Microsoft or Xbox.
