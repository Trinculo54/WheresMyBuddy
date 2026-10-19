// WheresMyBuddy's entry (mods\wheresmybuddy\WheresMyBuddy.dll), loaded by
// FH2ML as Azimuth starts. Initialize links us to FH2ML (platform_link.cpp)
// and starts our thread, which does the rest:
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
#include "platform_link.hpp"

#include <filesystem>

namespace {

DWORD WINAPI run(LPVOID) {
    using namespace wmb;
    const Fh2ml* f = fh2ml();
    const Paths p  = find_paths(f->azimuth, f->data);
    std::error_code ec;
    std::filesystem::create_directories(p.data, ec);
    log_open(p.data / "wheresmybuddy.log");
    log("WheresMyBuddy {} starting under FH2ML {}; Azimuth folder {}", WMB_VERSION,
        f->S->runtime_version ? f->S->runtime_version : "?", to_utf8(p.azimuth.wstring()));
    log("settings and logs in {}", to_utf8(p.data.wstring()));
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

    const auto manual = read_manual_names(p);
    std::vector<std::string> cached_favs;
    auto cached = read_cached_friends(p, &cached_favs);
    log("{} name(s) from friends.txt, {} saved Xbox friend(s)", manual.size(), cached.size());

    // Rotation advances once per launch; a refresh later this launch reuses
    // the same starting point.
    const std::size_t start = read_rotation(p);
    std::size_t next        = start;
    write_override(p, s, pick_lineup(s, merge_names(manual, cached), manual, cached_favs, start, &next));
    if (next != start) save_rotation(p, next);
    start_championship_fix(p);

    if (!s.enabled || !s.xbox) return 0;

    std::vector<std::string> fresh, fresh_favs;
    XboxMe me;
    const bool got = fetch_xbox_friends(p, s, fresh, &me, profile_wants_gamerpic(p, s), &fresh_favs);
    apply_xbox_profile(p, s, me); // your own gamertag / gamerpic, if signed in
    publish_signed_in_gamertag(me.gamertag);
    if (!got) {
        log("keeping the saved friends list for now");
        return 0;
    }
    if (fresh.empty() && !cached.empty()) {
        log("Xbox Live returned nobody; keeping the saved friends list");
    } else if (fresh != cached || fresh_favs != cached_favs) {
        save_cached_friends(p, fresh, fresh_favs);
        std::size_t unused = 0;
        write_override(p, s, pick_lineup(s, merge_names(manual, fresh), manual, fresh_favs, start, &unused));
        update_championship_names(p);
        log("friends list updated (shows in-game now if FH2 hadn't loaded Drivatars yet, otherwise "
            "next launch)");
    } else {
        log("friends list unchanged");
    }
    return 0;
}

// On FH2ML's worker, as Azimuth starts and before the game is mapped.
int initialize(const FH2ML_ModContext_v4* context, void*) {
    if (!wmb::connect_fh2ml(context)) return 0;
    // Our threads outlive Initialize: keep the DLL loaded for good.
    HMODULE pinned = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                       reinterpret_cast<LPCWSTR>(&run), &pinned);
    HANDLE t = CreateThread(nullptr, 0, run, nullptr, 0, nullptr);
    if (!t) {
        context->services->Log(context->mod, "couldn't start WheresMyBuddy's thread");
        return 0;
    }
    CloseHandle(t);
    return 1;
}

const FH2ML_ModAPI_v4 kDescriptor{
    sizeof(FH2ML_ModAPI_v4),
    FH2ML_API_VERSION_V4,
    nullptr,
    "WheresMyBuddy",
    WMB_VERSION,
    "Trinculo54/WheresMyBuddy",
    "Puts your Xbox friends in the game: the Drivatars you race and meet carry your friends' gamertags instead "
    "of random names. It can also make your in-game name and avatar your gamertag and gamerpic.",
    0,
    initialize,
    nullptr,
    nullptr};

} // namespace

FH2ML_V4_EXPORT const FH2ML_ModAPI_v4* FH2ML_GetModAPI_v4(uint32_t host_version) {
    return host_version == FH2ML_API_VERSION_V4 ? &kDescriptor : nullptr;
}
