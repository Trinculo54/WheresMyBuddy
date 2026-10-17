<h1 align="center" id="title">WheresMyBuddy</h1>

A mod for **Forza Horizon 2** on PC that puts your real **Xbox friends** in the game. FH2 fills its world with Drivatars named after random players, and the servers that used to hand out real ones are long gone. WheresMyBuddy signs in to Xbox Live, grabs the people you follow, and gives their gamertags to the Drivatars instead, so the cars you race, overtake and get punted by around Europe carry your friends' names again.

It can also set your own in-game name and avatar to your gamertag and gamerpic, which gets you most of the way to a pseudo online mode without any servers at all.

## Features

The game ships with 25 Drivatar names, each used several times across the AI drivers you meet. WheresMyBuddy swaps up to all 25 of them for your friends, with your Xbox favourites first. Anyone you add on Xbox shows up the next time you play, and if Xbox Live can't be reached, the last friends list it saved is used instead. You can also add names by hand in `friends.txt`, which always come before your Xbox friends. That's handy for people who aren't on Xbox, or for testing.

Signing in happens on the normal Microsoft sign-in page in your browser, the same one any app uses for "Sign in with Microsoft". You do it once. After that the mod keeps you signed in on its own, and your sign-in is stored encrypted with Windows' own data protection, so only your Windows account can read it. The mod only asks Microsoft for access to your Xbox profile and friends list, and it talks to nobody but Microsoft and Xbox Live.

After your first sign-in it copies your gamertag into Azimuth's player name and your gamerpic into the custom avatar slot from [Forza Horizon 2 Extra Options](https://github.com/RokkuDayo/forzahorizon2_extraoptions). This happens once, and then it's left alone so you can still change either one yourself. Your old avatar picture is kept as a backup.

The game's own files are never modified. The new Drivatar names go into the override folder that Extra Options provides, so turning the mod off puts the original Drivatars right back.

## Install

You'll need Forza Horizon 2 running on PC through Azimuth, and [Forza Horizon 2 Extra Options](https://github.com/RokkuDayo/forzahorizon2_extraoptions) installed with `EnableMediaOverrides=1` in its ini, which is the default. Extra Options is what lets the game load the modified Drivatar list.

Download the latest zip from the [Releases](../../releases) page, close Azimuth, and extract the zip into your Azimuth folder, the one with `azimuth.exe` in it. You should end up with `d3dcompiler_47.dll` next to `azimuth.exe` and `WheresMyBuddy.dll` inside a `fh2-mods` folder.

Launch the game as normal. Your browser opens on the Microsoft sign-in page, and a small WheresMyBuddy note pops up on top of the game in case the browser ends up behind it. Pick the account you use on Xbox and allow access. The tab tells you when you're done, and the note closes itself.

The game reads its Drivatar list very early, usually before the sign-in finishes, so your friends show up from the second launch onwards. The same goes for your name and avatar. From then on, every launch uses your saved friends straight away and quietly refreshes the list in the background.

### Using it with other FH2 mods

`d3dcompiler_47.dll` is the FH2 Mod Loader. All it does is load every mod in the `fh2-mods` folder, and every mod that uses it ships the same file, so you can install them in any order and overwrite it freely. [FH2 Universal Radio](https://github.com/Trinculo54/FH2-Universal-Radio) uses it from version 1.1.0. Older versions of the radio came with their own `d3dcompiler_47.dll`, so update the radio as well if you use both. Extra Options loads through `winmm.dll` and isn't affected.

## Settings

Everything lives in the `wheresmybuddy` folder next to `azimuth.exe`, which is created the first time you launch. `WheresMyBuddy.ini` has the options, and changes apply the next time you start Azimuth.

Under `[names]`, `enabled` turns the whole mod on or off, and setting it to 0 restores the original Drivatars on the next launch. `max_names` is how many of the 25 Drivatar names get replaced. If you have fewer friends than that, `repeat_to_fill` decides whether the leftover slots keep their original names or reuse your friends'. `favorites_first` puts your Xbox favourites at the front of the line.

Under `[xbox]`, `enabled` set to 0 skips Xbox Live entirely and only uses `friends.txt`. `browser_sign_in` set to 0 swaps the browser sign-in for a code you enter at microsoft.com/link, in case the browser route doesn't work on your machine. `client_id` is the Azure app the sign-in goes through. Leave it alone unless you've registered your own, as described below.

Under `[profile]`, `player_name` and `gamerpic` can each be `once`, `always` or `off`. `once` sets it after your first sign-in and then never touches it again, `always` keeps it in sync with Xbox on every launch, and `off` leaves it alone. To run a `once` again, delete `profile-applied.ini`.

`friends.txt` takes one name per line. Gamertags are limited to 15 characters, the same as on the Xbox One, and longer ones are skipped.

## Uninstall

Set `enabled=0` under `[names]` and launch the game once to put the original Drivatars back, then delete `fh2-mods\WheresMyBuddy.dll` and the `wheresmybuddy` folder. If you don't use any other FH2 mods, delete `d3dcompiler_47.dll` and the `fh2-mods` folder too. If you've already deleted the mod, you can remove `FH2\Mount\mediaoverride\drivatarglobalcache\DefaultCache.bin` by hand instead. To get your old avatar back, rename `Gamerpic.before-wheresmybuddy.png` next to `FH2_ExtraOptions.ini` to `Gamerpic.png`.

If you want to revoke the mod's Xbox access as well, remove WheresMyBuddy from the apps list at <https://account.live.com/consent/Manage>.

## How it works

FH2 keeps its Drivatars in `media\DrivatarGlobalCache\DefaultCache.bin`, a list of 125 driver records that only use 25 different gamertags between them. Each record keeps its gamertag as UTF-16 text in a fixed 16-character field. When Azimuth starts, the mod copies that file into Extra Options' `mediaoverride` folder and rewrites the name fields, leaving everything else in every record untouched. Extra Options makes the game load the override instead of the original.

The Xbox side is the standard chain a PC app uses: a Microsoft account sign-in through the identity platform's consumer endpoint (authorization code with PKCE and a `localhost` redirect, or the device code flow as the fallback), then an Xbox Live user token, then an XSTS token, then the Xbox people hub for your friends list. Your own name goes into `player_name` in Azimuth's `data\azimuth.ini`, and your gamerpic comes from the Xbox profile service and is saved as Extra Options' `Gamerpic.png`.

## Build from source

The mod is built with [llvm-mingw](https://github.com/mstorsjo/llvm-mingw/releases) and CMake. On Linux:

```bash
./scripts/build.sh
```

`dist/` then holds `d3dcompiler_47.dll`, `fh2-mods/WheresMyBuddy.dll` and a `README.txt`, ready to zip up. GitHub Actions does the same on every push and uploads the result as a build artifact. Pushing a tag like `v1.0.0` also creates a draft release with the zip attached, ready to be published.

The release build has the WheresMyBuddy Azure app ID built in. If you fork this, you can use your own: register an app in the [Azure portal](https://portal.azure.com) under App registrations, choose "Personal Microsoft accounts only", add `http://localhost` as a redirect URI under the "Mobile and desktop applications" platform, and turn on "Allow public client flows". Then either build with `WMB_CLIENT_ID=<your app id> ./scripts/build.sh` or put the ID in `client_id` in the ini.

Configuring with `-DWMB_TESTS=ON` also builds three offline tests that run under Wine. `wmb_test` rewrites a copy of the real `DefaultCache.bin`, which you pass in as an argument. `profile_test` checks the name and avatar settings. `browser_test` pretends to be a browser to exercise the sign-in redirect.

## Troubleshooting

Everything gets logged to `wheresmybuddy\wheresmybuddy.log`, which starts fresh every launch. A working launch shows `wrote ... with N of 25 Drivatar names replaced`, `signed in to Xbox Live as <your gamertag>`, and `got N people from Xbox Live`. The FH2 Mod Loader writes `fh2-mods\loader.log`, which shows whether the mod was loaded at all.

| Symptom | Fix |
|---|---|
| Nothing happens and there's no `wheresmybuddy` folder | Make sure `d3dcompiler_47.dll` is next to `azimuth.exe` and `WheresMyBuddy.dll` is inside `fh2-mods`, then check `fh2-mods\loader.log`. |
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
