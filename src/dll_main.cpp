// WheresMyBuddy plugin entry (fh2-mods\WheresMyBuddy.dll), loaded by the
// FH2 Mod Loader as Azimuth starts.
//
//   1. Right away: rebuild the Drivatar override from friends.txt plus the
//      friends list saved last time. That's a ~1 MB file copy, done long
//      before the game reads DefaultCache.bin.
//   2. Then, in the background: sign in to Xbox Live, fetch the people you
//      follow, save them, and rebuild the override if anything changed.
//      If the game has already read the file by then, the new names show
//      up next launch.
//   3. Your own gamertag/gamerpic become your in-game name and avatar
//      (once, by default; see profile.cpp).

#include "common.hpp"
#include "fh2_updates.hpp"

#include <filesystem>

namespace {

DWORD WINAPI run(LPVOID param) {
    using namespace wmb;
    const Paths p = find_paths(static_cast<HMODULE>(param));
    std::error_code ec;
    std::filesystem::create_directories(p.data, ec);
    log_open(p.data / "wheresmybuddy.log");
    log("WheresMyBuddy {} starting; Azimuth folder {}", WMB_VERSION, to_utf8(p.azimuth.wstring()));
    if (p.mount.empty()) {
        log("couldn't find the FH2 game folder (looked via data\\azimuth.ini and FH2\\Mount); stopping");
        return 0;
    }
    log("game folder {}", to_utf8(p.mount.wstring()));

    // Extra Options provides the mediaoverride folder.
    const auto eo = (p.azimuth / "FH2_ExtraOptions.ini").wstring();
    if (!std::filesystem::exists(eo, ec)) {
        log("WARNING: Forza Horizon 2 Extra Options not found next to azimuth.exe; it's needed to "
            "load the override (mediaoverride) folder");
    } else if (GetPrivateProfileIntW(L"Files", L"EnableMediaOverrides", 1, eo.c_str()) == 0) {
        log("WARNING: EnableMediaOverrides=0 in FH2_ExtraOptions.ini, so the game will ignore the "
            "override; set it to 1");
    }

    const Settings s = load_settings(p);

    // Version / update notice on the title screen (background).
    fh2common::start_update_check({L"WheresMyBuddy", L"WheresMyBuddy.dll", WMB_VERSION, "Trinculo54/WheresMyBuddy",
                                   FH2_LOADER_VERSION, p.azimuth, s.check_updates,
                                   [](const std::string& m) { log("{}", m); }});
    const auto manual = read_manual_names(p);
    auto cached       = read_cached_friends(p);
    log("{} name(s) from friends.txt, {} saved Xbox friend(s)", manual.size(), cached.size());
    write_override(p, s, merge_names(manual, cached));
    start_championship_fix(p);

    if (!s.enabled || !s.xbox) return 0;

    std::vector<std::string> fresh;
    XboxMe me;
    const bool got = fetch_xbox_friends(p, s, fresh, &me, profile_wants_gamerpic(p, s));
    apply_xbox_profile(p, s, me); // your own gamertag / gamerpic, if signed in
    if (!got) {
        log("keeping the saved friends list for now");
        return 0;
    }
    if (fresh.empty() && !cached.empty()) {
        log("Xbox Live returned nobody; keeping the saved friends list");
    } else if (fresh != cached) {
        save_cached_friends(p, fresh);
        write_override(p, s, merge_names(manual, fresh));
        update_championship_names(p);
        log("friends list updated (shows in-game now if FH2 hadn't loaded Drivatars yet, otherwise "
            "next launch)");
    } else {
        log("friends list unchanged");
    }
    return 0;
}

} // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        HMODULE pinned = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                           reinterpret_cast<LPCWSTR>(&run), &pinned);
        if (HANDLE t = CreateThread(nullptr, 0, run, instance, 0, nullptr)) CloseHandle(t);
    }
    return TRUE;
}
