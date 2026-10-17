<h1 align="center" id="title">WheresMyBuddy</h1>

A mod for **Forza Horizon 2** on PC that puts your real **Xbox friends** in the game. FH2 fills its world with Drivatars named after random players, and the servers that used to hand out real ones are long gone. WheresMyBuddy signs in to Xbox Live, grabs the people you follow, and gives their gamertags to the Drivatars instead, so the cars you race, overtake and get punted by around Europe carry your friends' names again.

It can also set your own in-game name and avatar to your gamertag and gamerpic, which gets you most of the way to a pseudo online mode without any servers at all.

## Features

The game ships with 25 Drivatar names, each used several times across the AI drivers you meet. WheresMyBuddy swaps up to all 25 of them for your friends, with your Xbox favourites first. If you have more friends than that, they take turns: your favourites are always there, and everyone else rotates through the remaining spots, so a different set shows up each time you play and everyone gets their turn. Anyone you add on Xbox shows up the next time you play, and if Xbox Live can't be reached, the last friends list it saved is used instead. You can also add names by hand in `friends.txt`, which always come before your Xbox friends. That's handy for people who aren't on Xbox, or for testing.

Signing in happens on the normal Microsoft sign-in page in your browser, the same one any app uses for "Sign in with Microsoft". You do it once. After that the mod keeps you signed in on its own, and your sign-in is stored encrypted with Windows' own data protection, so only your Windows account can read it. The mod only asks Microsoft for access to your Xbox profile and friends list, and it talks to nobody but Microsoft and Xbox Live.

After your first sign-in it copies your gamertag into Azimuth's player name and your gamerpic into the custom avatar slot from [Forza Horizon 2 Extra Options](https://github.com/RokkuDayo/forzahorizon2_extraoptions). This happens once, and then it's left alone so you can still change either one yourself. Your old avatar picture is kept as a backup.

Your friends show up everywhere the game names its AI drivers: free roam, championship races (including ones you started before installing the mod), road trips and car meets. The game's own files are never modified. The new names go into the override folder that Extra Options provides, so turning the mod off puts the original Drivatars right back.

## Install

You'll need Forza Horizon 2 running on PC through Azimuth, and [Forza Horizon 2 Extra Options](https://github.com/RokkuDayo/forzahorizon2_extraoptions) installed with `EnableMediaOverrides=1` in its ini, which is the default. Extra Options is what lets the game load the modified Drivatar list and AI driver names.

Download the latest zip from the [Releases](../../releases) page, close Azimuth, and extract the zip into your Azimuth folder, the one with `azimuth.exe` in it. You should end up with `d3dcompiler_47.dll` next to `azimuth.exe` and `WheresMyBuddy.dll` inside a `fh2-mods` folder.

Launch the game as normal. Your browser opens on the Microsoft sign-in page, and a small WheresMyBuddy note pops up on top of the game in case the browser ends up behind it. Pick the account you use on Xbox and allow access. The tab tells you when you're done, and the note closes itself.

The game reads its Drivatar list very early, usually before the sign-in finishes, so your friends show up from the second launch onwards. The same goes for your name and avatar. From then on, every launch uses your saved friends straight away and quietly refreshes the list in the background.

### Updates

When a newer WheresMyBuddy is out, the title screen tells you: "PRESS (A) TO START" gets a note like "WheresMyBuddy 1.2.0 is available" added to it. The mod asks GitHub at most every six hours and remembers the answer between launches. If another mod has put back an older FH2 Mod Platform than the one WheresMyBuddy came with, the title screen says that too, and reinstalling WheresMyBuddy fixes it. Other mods on the platform, like FH2 Universal Radio, put their notes in the same spot. The version you have is in the first line of the log and in the DLL's file properties.

### Using it with other FH2 mods

`d3dcompiler_47.dll` is the [FH2 Mod Platform](https://github.com/Trinculo54/FH2-Mod-Platform), the shared base for FH2 mods. It loads every mod in the `fh2-mods` folder and handles the jobs they have in common, like changing the game's text and showing update notices, so mods don't trip over each other. Every mod built on it ships it, so you can install them in any order and let them overwrite it. [FH2 Universal Radio](https://github.com/Trinculo54/FH2-Universal-Radio) 1.3.0 and later is built on it as well. Extra Options loads through `winmm.dll` and isn't affected. If the platform is missing or very old, WheresMyBuddy still renames the Drivatars, but car meet names and update notices are off until you reinstall it. With platform 2.1.0 or later, the Drivatar file goes through the platform too, so removing WheresMyBuddy puts the original Drivatars back on its own, and other mods can ask WheresMyBuddy which names are on the Drivatars and who's signed in, through the `WheresMyBuddy.Friends` interface in [`include/wheresmybuddy.h`](include/wheresmybuddy.h).

## Settings

Everything lives in the `wheresmybuddy` folder next to `azimuth.exe`, which is created the first time you launch. `WheresMyBuddy.ini` has the options, and changes apply the next time you start Azimuth.

Under `[names]`, `enabled` turns the whole mod on or off, and setting it to 0 restores the original Drivatars on the next launch. `max_names` is how many of the 25 Drivatar names get replaced. If you have fewer friends than that, `repeat_to_fill` decides whether the leftover slots keep their original names or reuse your friends'. `favorites_first` puts your Xbox favourites at the front of the line. `rotate` decides what happens when you have more friends than `max_names`. Set to 1, which is the default, your favourites and the names in `friends.txt` always stay in, and the rest take turns, a different set each launch. Set to 0, it always uses the same first ones. Because championship racers follow the Drivatar list, a championship in progress picks up that launch's names too.

Under `[xbox]`, `enabled` set to 0 skips Xbox Live entirely and only uses `friends.txt`. `browser_sign_in` set to 0 swaps the browser sign-in for a code you enter at microsoft.com/link, in case the browser route doesn't work on your machine. `client_id` is the Azure app the sign-in goes through. Leave it alone unless you've registered your own, as described below.

Under `[profile]`, `player_name` and `gamerpic` can each be `once`, `always` or `off`. `once` sets it after your first sign-in and then never touches it again, `always` keeps it in sync with Xbox on every launch, and `off` leaves it alone. To run a `once` again, delete `profile-applied.ini`.

Under `[updates]`, `check` set to 0 stops the mod from asking GitHub whether there's a newer version.

`friends.txt` takes one name per line. Gamertags are limited to 15 characters, the same as on the Xbox One, and longer ones are skipped.

## Uninstall

Set `enabled=0` under `[names]` and launch the game once to put the original Drivatars back, then delete `fh2-mods\WheresMyBuddy.dll` and the `wheresmybuddy` folder. If you don't use any other FH2 mods, delete `d3dcompiler_47.dll` and the `fh2-mods` folder too. If you've already deleted the mod, you can remove `FH2\Mount\mediaoverride\drivatarglobalcache\DefaultCache.bin` and the `AIPlayers.str` files under `FH2\Mount\mediaoverride\Stripped\StringTables` by hand instead. To get your old avatar back, rename `Gamerpic.before-wheresmybuddy.png` next to `FH2_ExtraOptions.ini` to `Gamerpic.png`.

If you want to revoke the mod's Xbox access as well, remove WheresMyBuddy from the apps list at <https://account.live.com/consent/Manage>.

## How it works

FH2 keeps its Drivatars in `media\DrivatarGlobalCache\DefaultCache.bin`, a list of 125 driver records that only use 25 different gamertags between them. Each record keeps its gamertag as UTF-16 text in a fixed 16-character field. When Azimuth starts, the mod copies that file into Extra Options' `mediaoverride` folder and rewrites the name fields, leaving everything else in every record untouched. Extra Options makes the game load the override instead of the original.

Championships work a little differently. When you start one, the game copies the Drivatars it picked into your save, and every race of that championship loads them from there instead of from the Drivatar list, so a championship started before you installed the mod would still have the old names. Each of those saved Drivatars carries the same id (1 to 25) it has in `DefaultCache.bin`, so when the game loads them, the mod gives each one the name that id has in the Drivatar list right now. It does this in memory after the game reads your save. The save file itself is never edited, and it simply picks up the new names the next time the game saves.

Car meets and some road-trip traffic don't use Drivatars at all. Their drivers are named from a plain string table, `AIPlayers.str`, which sits inside each language's string table zip in `media\Stripped\StringTables` and holds the same 25 gamertags plus one extra, "HRZN Ben". The mod reads it out of every language zip, swaps the names the same way as the Drivatars (HRZN Ben gets the next friend in line), and writes the result to `mediaoverride\Stripped\StringTables\<language>\AIPlayers.str`, so the zips are left alone.

The Xbox side is the standard chain a PC app uses: a Microsoft account sign-in through the identity platform's consumer endpoint (authorization code with PKCE and a `localhost` redirect, or the device code flow as the fallback), then an Xbox Live user token, then an XSTS token, then the Xbox people hub for your friends list. Your own name goes into `player_name` in Azimuth's `data\azimuth.ini`, and your gamerpic comes from the Xbox profile service and is saved as Extra Options' `Gamerpic.png`.

## Build from source

The mod is built with [llvm-mingw](https://github.com/mstorsjo/llvm-mingw/releases) and CMake, on top of the [FH2 Mod Platform](https://github.com/Trinculo54/FH2-Mod-Platform), which is a git submodule in `platform/`. Clone with `--recursive`, or run `git submodule update --init`; the build script does it for you if it's missing. On Linux:

```bash
./scripts/build.sh
```

`dist/` then holds `d3dcompiler_47.dll`, `fh2-mods/WheresMyBuddy.dll` and a `README.txt`, ready to zip up. GitHub Actions does the same on every push and uploads the result as a build artifact. Pushing a tag like `v1.0.0` also creates a draft release with the zip attached, ready to be published. The version lives in `project(... VERSION x.y.z)` in `CMakeLists.txt`. Bump it before tagging, since the tag build refuses to run if the two don't match, and it's what the mod compares against the latest GitHub release.

The release build has the WheresMyBuddy Azure app ID built in. If you fork this, you can use your own: register an app in the [Azure portal](https://portal.azure.com) under App registrations, choose "Personal Microsoft accounts only", add `http://localhost` as a redirect URI under the "Mobile and desktop applications" platform, and turn on "Allow public client flows". Then either build with `WMB_CLIENT_ID=<your app id> ./scripts/build.sh` or put the ID in `client_id` in the ini.

Configuring with `-DWMB_TESTS=ON` also builds four offline tests that run under Wine. `wmb_test` rewrites a copy of the real `DefaultCache.bin`. `championship_test` maps the real game executable and runs the championship fix against Drivatars taken from a save. `profile_test` checks the name and avatar settings. `browser_test` pretends to be a browser to exercise the sign-in redirect.

## Troubleshooting

Everything gets logged to `wheresmybuddy\wheresmybuddy.log`, which starts fresh every launch. A working launch shows `wrote ... with N of 25 Drivatar names replaced`, `AI driver names (car meets, road trips): N swapped`, `championship fix installed`, `signed in to Xbox Live as <your gamertag>`, and `got N people from Xbox Live`. When the game loads a championship that still has old names, you'll also see a line for each Drivatar it renames. The FH2 Mod Platform writes `fh2-mods\platform.log`, which shows whether the mod was loaded at all, and which game text and files it wrote for it. If the game crashes, the platform saves a report in `fh2-mods\crashes` that says which mod, if any, was involved; attach it when you report a problem.

| Symptom | Fix |
|---|---|
| Nothing happens and there's no `wheresmybuddy` folder | Make sure `d3dcompiler_47.dll` is next to `azimuth.exe` and `WheresMyBuddy.dll` is inside `fh2-mods`, then check `fh2-mods\platform.log`. |
| Drivatars still have their old names | Launch the game a second time, since the first launch only fetches your friends. If they're still old, check that Extra Options is installed with `EnableMediaOverrides=1`. The log warns about both. |
| The browser shows a Microsoft error page | Set `browser_sign_in=0` and use the code at microsoft.com/link instead, and open an issue with a screenshot of the error. |
| `Xbox Live authorization failed` | The log says why. Usually the account has no Xbox profile yet, or it's a child account that needs to be added to a Microsoft family. |
| Signed in as the wrong account | Delete `wheresmybuddy\account.dat` and launch again. |
| A friend is missing | Gamertags longer than 15 characters don't fit a Drivatar and are skipped. Also, only the first `max_names` names are used. |

If you're still stuck, open an issue and attach your `wheresmybuddy.log`. It doesn't contain your sign-in, just the steps and your friends' gamertags.

## License

Released under the [GNU General Public License v3.0](LICENSE).

## Disclaimer

Unofficial fan-made mod. Not affiliated with, endorsed by, or connected to Playground Games, Turn 10 Studios, Xbox Game Studios, Microsoft, the Azimuth developers, or the Extra Options author. Xbox, Xbox Live and Forza Horizon are trademarks of Microsoft. Use at your own risk.
