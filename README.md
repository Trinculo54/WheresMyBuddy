<h1 align="center" id="title">WheresMyBuddy</h1>

A mod for **Forza Horizon 2** on PC that puts your real **Xbox friends** in the game. FH2 fills its world with Drivatars named after random players, and the servers that used to hand out real ones are long gone. WheresMyBuddy signs in to Xbox Live, grabs the people you follow, and gives their gamertags to the Drivatars instead, so the cars you race, overtake and get punted by around Europe carry your friends' names again.

It can also set your own in-game name and avatar to your gamertag and gamerpic, which gets you most of the way to a pseudo online mode without any servers at all.

## Features

The game ships with 25 Drivatar names, each used several times across the AI drivers you meet. WheresMyBuddy swaps up to all 25 of them for your friends, with your Xbox favourites first. If you have more friends than that, they take turns: your favourites are always there, and everyone else rotates through the remaining spots, so a different set shows up each time you play and everyone gets their turn. Anyone you add on Xbox shows up the next time you play, and if Xbox Live can't be reached, the last friends list it saved is used instead. You can also add names by hand in `friends.txt`, which always come before your Xbox friends. That's handy for people who aren't on Xbox, or for testing.

Signing in happens on the normal Microsoft sign-in page in your browser, the same one any app uses for "Sign in with Microsoft". You do it once. After that the mod keeps you signed in on its own, and your sign-in is stored encrypted with Windows' own data protection, so only your Windows account can read it. The mod only asks Microsoft for access to your Xbox profile and friends list, and it talks to nobody but Microsoft and Xbox Live.

After your first sign-in it copies your gamertag into Azimuth's player name and your gamerpic into the custom avatar slot from [Forza Horizon 2 Extra Options](https://github.com/RokkuDayo/forzahorizon2_extraoptions). This happens once, and then it's left alone so you can still change either one yourself. Your old avatar picture is kept as a backup.

Your friends show up everywhere the game names its AI drivers: free roam, championship races (including ones you started before installing the mod), road trips and car meets. The game's own files are never modified. The new names go into the override folder that Extra Options provides, so turning the mod off puts the original Drivatars right back.

## Install

You'll need Forza Horizon 2 running on PC through Azimuth, [FH2ML](https://github.com/Refilewski/FH2ML), the Forza Horizon 2 mod loader, which loads WheresMyBuddy, and [Forza Horizon 2 Extra Options](https://github.com/RokkuDayo/forzahorizon2_extraoptions) installed with `EnableMediaOverrides=1` in its ini, which is the default. Extra Options is what lets the game load the modified Drivatar list and AI driver names.

Download the latest zip from the [Releases](../../releases) page and close Azimuth. Then either install it with the FH2ML Manager (Mods > Install mod, and pick the zip), or extract the zip into your Azimuth folder, the one with `azimuth.exe` in it. Either way you end up with `WheresMyBuddy.dll` and its `mod.json` in `mods\wheresmybuddy`.

Launch the game as normal. Your browser opens on the Microsoft sign-in page, and a small WheresMyBuddy note pops up on top of the game in case the browser ends up behind it. Pick the account you use on Xbox and allow access. The tab tells you when you're done, and the note closes itself.

The game reads its Drivatar list very early, usually before the sign-in finishes, so your friends show up from the second launch onwards. The same goes for your name and avatar. From then on, every launch uses your saved friends straight away and quietly refreshes the list in the background.

### Upgrading from 1.7 or earlier

WheresMyBuddy 1.7 and earlier ran on a different loader, and FH2ML doesn't start its mods while that one is still installed. The FH2ML Manager shows it on its Mods page and removes it for you. On its first launch under FH2ML, WheresMyBuddy copies everything from the old `wheresmybuddy` folder next to `azimuth.exe` (your settings, `friends.txt`, the saved friends list and your sign-in) into `FH2ML\data\wheresmybuddy`, so there's nothing to redo. It only copies, so the old folder stays until you delete it.

### Updates

When a newer WheresMyBuddy is out, FH2ML tells you on the title screen, where "PRESS (A) TO START" gets a note added to it, and on its MODS tab. Update checks are FH2ML's job now, and you choose them in `FH2ML\config\FH2ML.json`: `"updateChecks": false` turns them off for every mod, and `"mods": {"wheresmybuddy": {"updateChecks": false}}` for WheresMyBuddy alone. The `[updates]` section older versions put in `WheresMyBuddy.ini` is removed from it on the first launch (if it said `check=0`, the log reminds you to set this). The version you have is in the first line of the log, in FH2ML's mod list and in the DLL's file properties.

### Using it with other FH2 mods

FH2ML loads every mod in the `mods` folder and handles the jobs they have in common, like changing the game's text and files and hooking game code, so mods don't trip over each other. Extra Options loads through `winmm.dll` and isn't affected. The Drivatar file goes through FH2ML's file overrides, so removing WheresMyBuddy puts the original Drivatars back on its own. Other mods can ask WheresMyBuddy which names are on the Drivatars and who's signed in, through the `WheresMyBuddy.Friends` interface in [`include/wheresmybuddy.h`](include/wheresmybuddy.h). The championship fix below is an FH2ML hook: it goes in only after Extra Options has finished patching the game, it's listed in FH2ML's crash reports, and other mods can hook the same game code alongside it.

## Settings

Everything lives in `FH2ML\data\wheresmybuddy` in your Azimuth folder, which FH2ML creates the first time WheresMyBuddy loads and keeps when you update or reinstall the mod. `WheresMyBuddy.ini` has the options, and changes apply the next time you start Azimuth.

Under `[names]`, `enabled` turns the whole mod on or off, and setting it to 0 restores the original Drivatars on the next launch. `max_names` is how many of the 25 Drivatar names get replaced. If you have fewer friends than that, `repeat_to_fill` decides whether the leftover slots keep their original names or reuse your friends'. `favorites_first` puts your Xbox favourites at the front of the line. `rotate` decides what happens when you have more friends than `max_names`. Set to 1, which is the default, your favourites and the names in `friends.txt` always stay in, and the rest take turns, a different set each launch. Set to 0, it always uses the same first ones. Because championship racers follow the Drivatar list, a championship in progress picks up that launch's names too.

Under `[xbox]`, `enabled` set to 0 skips Xbox Live entirely and only uses `friends.txt`. `browser_sign_in` set to 0 swaps the browser sign-in for a code you enter at microsoft.com/link, in case the browser route doesn't work on your machine. `client_id` is the Azure app the sign-in goes through. Leave it alone unless you've registered your own, as described below.

Under `[profile]`, `player_name` and `gamerpic` can each be `once`, `always` or `off`. `once` sets it after your first sign-in and then never touches it again, `always` keeps it in sync with Xbox on every launch, and `off` leaves it alone. To run a `once` again, delete `profile-applied.ini`.

The main switches are also in the game, under Pause > Settings > MODS: friends as Drivatars (`[names] enabled`), Xbox Live friends (`[xbox] enabled`), favourites first, friends taking turns (`rotate`) and filling every Drivatar (`repeat_to_fill`). Changing them there saves them to the ini. Since the Drivatars are written when the game starts, the game offers to restart when you leave the page.

`friends.txt`, in the same folder, takes one name per line. Gamertags are limited to 15 characters, the same as on the Xbox One, and longer ones are skipped.

## Uninstall

Set `enabled=0` under `[names]` and launch the game once to put the original Drivatars back, then uninstall WheresMyBuddy in the FH2ML Manager (Mods > Uninstall), or delete the `mods\wheresmybuddy` folder. Your settings stay in `FH2ML\data\wheresmybuddy`; delete that folder too if you won't be back. If you've already deleted the mod and FH2ML with it, you can remove `FH2\Mount\mediaoverride\drivatarglobalcache\DefaultCache.bin` and the `AIPlayers.str` files under `FH2\Mount\mediaoverride\Stripped\StringTables` by hand instead. To get your old avatar back, rename `Gamerpic.before-wheresmybuddy.png` next to `FH2_ExtraOptions.ini` to `Gamerpic.png`.

If you want to revoke the mod's Xbox access as well, remove WheresMyBuddy from the apps list at <https://account.live.com/consent/Manage>.

## How it works

FH2 keeps its Drivatars in `media\DrivatarGlobalCache\DefaultCache.bin`, a list of 125 driver records that only use 25 different gamertags between them. Each record keeps its gamertag as UTF-16 text in a fixed 16-character field. When Azimuth starts, the mod copies that file into Extra Options' `mediaoverride` folder and rewrites the name fields, leaving everything else in every record untouched. Extra Options makes the game load the override instead of the original.

Championships work a little differently. When you start one, the game copies the Drivatars it picked into your save, and every race of that championship loads them from there instead of from the Drivatar list, so a championship started before you installed the mod would still have the old names. Each of those saved Drivatars carries the same id (1 to 25) it has in `DefaultCache.bin`, so when the game loads them, the mod gives each one the name that id has in the Drivatar list right now. It does this in memory after the game reads your save. The save file itself is never edited, and it simply picks up the new names the next time the game saves.

Car meets and some road-trip traffic don't use Drivatars at all. Their drivers are named from a plain string table, `AIPlayers.str`, which sits inside each language's string table zip in `media\Stripped\StringTables` and holds the same 25 gamertags plus one extra, "HRZN Ben". The mod reads it out of every language zip, swaps the names the same way as the Drivatars (HRZN Ben gets the next friend in line), and writes the result to `mediaoverride\Stripped\StringTables\<language>\AIPlayers.str`, so the zips are left alone.

The Xbox side is the standard chain a PC app uses: a Microsoft account sign-in through the identity platform's consumer endpoint (authorization code with PKCE and a `localhost` redirect, or the device code flow as the fallback), then an Xbox Live user token, then an XSTS token, then the Xbox people hub for your friends list. Your own name goes into `player_name` in Azimuth's `data\azimuth.ini`, and your gamerpic comes from the Xbox profile service and is saved as Extra Options' `Gamerpic.png`.

## Build from source

The mod is built with [llvm-mingw](https://github.com/mstorsjo/llvm-mingw/releases) and CMake, against FH2ML's v4 mod API: [`FH2ML_v4.h`](third_party/fh2ml/FH2ML_v4.h), vendored from [FH2ML](https://github.com/Refilewski/FH2ML) under the MIT licence next to it. On Linux:

```bash
./scripts/build.sh
```

`dist/` then holds `mods/wheresmybuddy/` with `mod.json`, `WheresMyBuddy.dll`, `README.txt` and `LICENSE.txt`, ready to zip up: the zip extracts straight into the Azimuth folder, and the FH2ML Manager installs it as it is. GitHub Actions does the same on every push and uploads the result as a build artifact. Pushing a tag like `v1.0.0` also creates a draft release with the zip attached, ready to be published. The version lives in `project(... VERSION x.y.z)` in `CMakeLists.txt`, and `mod.json` is generated from [`cmake/mod.json.in`](cmake/mod.json.in) with it, so the two always match. Bump it before tagging, since the tag build refuses to run if the tag and the version don't match, and it's what FH2ML compares against the latest GitHub release.

The release build has the WheresMyBuddy Azure app ID built in. If you fork this, you can use your own: register an app in the [Azure portal](https://portal.azure.com) under App registrations, choose "Personal Microsoft accounts only", add `http://localhost` as a redirect URI under the "Mobile and desktop applications" platform, and turn on "Allow public client flows". Then either build with `WMB_CLIENT_ID=<your app id> ./scripts/build.sh` or put the ID in `client_id` in the ini.

Configuring with `-DWMB_TESTS=ON` also builds four offline tests that run under Wine. `wmb_test` rewrites a copy of the real `DefaultCache.bin`. `championship_test` maps the real game executable and runs the championship fix against Drivatars taken from a save (or copies of the built-in ones, with `-` instead of a save), on its own or, with `fh2ml` at the end, through a stand-in for FH2ML's services, which also checks the Mods-page rows and the one-time copy of the old `wheresmybuddy` folder. `profile_test` checks the name and avatar settings. `browser_test` pretends to be a browser to exercise the sign-in redirect.

## Troubleshooting

Everything gets logged to `FH2ML\data\wheresmybuddy\wheresmybuddy.log`, which starts fresh every launch. A working launch shows `wrote ... with N of 25 Drivatar names replaced`, `AI driver names (car meets, road trips): N swapped`, `championship fix installed as an FH2ML hook` (and `FH2ML.log` then has a `wheresmybuddy: hooked pointer game+0x2D44F30` line), `signed in to Xbox Live as <your gamertag>`, and `got N people from Xbox Live`. When the game loads a championship that still has old names, you'll also see a line for each Drivatar it renames. FH2ML writes `FH2ML\logs\FH2ML.log`, which shows whether the mod was loaded at all (`wheresmybuddy: lifecycle v4 initialized`), and which game text and files it wrote for it. If the game crashes, FH2ML saves a report in `FH2ML\logs\crashes` that says which mod, if any, was involved; attach it when you report a problem.

| Symptom | Fix |
|---|---|
| Nothing happens and there's no `FH2ML\data\wheresmybuddy` folder | Make sure FH2ML is installed and `WheresMyBuddy.dll` and `mod.json` are inside `mods\wheresmybuddy`, then check `FH2ML\logs\FH2ML.log`. |
| Drivatars still have their old names | Launch the game a second time, since the first launch only fetches your friends. If they're still old, check that Extra Options is installed with `EnableMediaOverrides=1`. The log warns about both. |
| The browser shows a Microsoft error page | Set `browser_sign_in=0` and use the code at microsoft.com/link instead, and open an issue with a screenshot of the error. |
| `Xbox Live authorization failed` | The log says why. Usually the account has no Xbox profile yet, or it's a child account that needs to be added to a Microsoft family. |
| Signed in as the wrong account | Delete `FH2ML\data\wheresmybuddy\account.dat` and launch again. |
| A friend is missing | Gamertags longer than 15 characters don't fit a Drivatar and are skipped. Also, only the first `max_names` names are used. |

If you're still stuck, open an issue and attach your `wheresmybuddy.log`. It doesn't contain your sign-in, just the steps and your friends' gamertags.

## License

Released under the [GNU General Public License v3.0](LICENSE).

## Disclaimer

Unofficial fan-made mod. Not affiliated with, endorsed by, or connected to Playground Games, Turn 10 Studios, Xbox Game Studios, Microsoft, the Azimuth developers, or the Extra Options author. Xbox, Xbox Live and Forza Horizon are trademarks of Microsoft. Use at your own risk.
