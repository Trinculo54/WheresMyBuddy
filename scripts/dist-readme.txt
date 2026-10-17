WheresMyBuddy
=============

Your real Xbox friends as the Drivatars in Forza Horizon 2 (Azimuth).

FH2 names its AI drivers after 25 random players, and the servers that
used to send real Drivatars are long gone. WheresMyBuddy signs in to Xbox
Live, gets the people you follow, and gives their gamertags to those
Drivatars instead (favourites first). It can also set your own in-game name
and avatar to your gamertag and gamerpic.

Project page and source: https://github.com/Trinculo54/WheresMyBuddy


Requirements
~~~~~~~~~~~~

  - Forza Horizon 2 running through Azimuth.
  - Forza Horizon 2 Extra Options, with EnableMediaOverrides=1 in
    FH2_ExtraOptions.ini (the default). It's what lets the game load the
    new Drivatar names.
    https://github.com/RokkuDayo/forzahorizon2_extraoptions


Install
~~~~~~~

1. Close Azimuth.
2. Extract this zip into your Azimuth folder (the one with azimuth.exe).
   You should end up with:
       d3dcompiler_47.dll            next to azimuth.exe
       fh2-mods\WheresMyBuddy.dll
3. Start Azimuth and FH2. Your browser opens the Microsoft sign-in page.
   Sign in with the account you use on Xbox and allow access. You only
   do this once.
4. Restart the game. The Drivatars now have your friends' names, and your
   in-game name and avatar match your Xbox profile.

The first launch only fetches your friends because the game reads its
Drivatar list before the sign-in can finish. From then on the saved list
is used straight away and refreshed in the background every launch.

d3dcompiler_47.dll is the FH2 Mod Loader. It loads every mod in the
fh2-mods folder, and every mod that uses it ships the same file, so it's
fine to overwrite. FH2 Universal Radio 1.0.x shipped its own
d3dcompiler_47.dll; if you use the radio too, update it to 1.1.0 or later.


Settings (wheresmybuddy\WheresMyBuddy.ini)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Created on first launch. Changes apply the next time you start Azimuth.

  [names]
  enabled=1          0 = mod off, original Drivatars come back
  max_names=25       how many of the 25 Drivatar names to replace
  repeat_to_fill=0   1 = reuse your names when you have fewer than max_names
  favorites_first=1  your Xbox favourites go first

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

friends.txt: extra names, one per line, 15 characters max. These always
come before your Xbox friends.


Privacy
~~~~~~~

The mod only asks Microsoft for your Xbox profile and friends list, and
only talks to Microsoft and Xbox Live. Your sign-in is kept in
wheresmybuddy\account.dat, encrypted with Windows' data protection so only
your Windows account can read it. To revoke access, remove WheresMyBuddy
at https://account.live.com/consent/Manage


Troubleshooting
~~~~~~~~~~~~~~~

Everything is logged to wheresmybuddy\wheresmybuddy.log (fresh each
launch). Useful lines:

  signed in to Xbox Live as <you>      the sign-in works
  got N people from Xbox Live          your friends list arrived
  wrote ... with N of 25 Drivatar names replaced
                                       the Drivatar list was rewritten

Still the old names: launch a second time, and check that Extra Options is
installed with EnableMediaOverrides=1 (the log warns if it isn't).
Wrong account: delete wheresmybuddy\account.dat and launch again.
Microsoft error page in the browser: set browser_sign_in=0.
No wheresmybuddy folder at all: check fh2-mods\loader.log.


Uninstall
~~~~~~~~~

Set enabled=0 under [names] and launch once (restores the original
Drivatars), then delete fh2-mods\WheresMyBuddy.dll and the wheresmybuddy
folder. Delete d3dcompiler_47.dll and fh2-mods too if no other FH2 mods use
them. Your previous avatar is saved next to FH2_ExtraOptions.ini as
Gamerpic.before-wheresmybuddy.png.


License
~~~~~~~

GPLv3. Unofficial fan-made mod; not affiliated with Playground Games,
Turn 10, Microsoft or Xbox.
