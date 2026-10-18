#include "platform_link.hpp"

#include "wheresmybuddy.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace {
std::atomic<const FH2ModApi*> g_api{nullptr};
std::atomic<FH2Mod*> g_mod{nullptr};
std::atomic<bool> g_gave_up{false};
HANDLE g_ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);

std::mutex g_pub_mu;
std::vector<std::string> g_names;
std::string g_gamertag;

int copy_out(const std::string& s, char* out, int out_size) {
    if (out && out_size > 0) {
        const auto n = std::min<std::size_t>(s.size(), static_cast<std::size_t>(out_size - 1));
        std::memcpy(out, s.data(), n);
        out[n] = '\0';
    }
    return static_cast<int>(s.size());
}

const WheresMyBuddyFriendsV1 g_friends{
    sizeof(WheresMyBuddyFriendsV1),
    [] {
        std::scoped_lock lk{g_pub_mu};
        return static_cast<int>(g_names.size());
    },
    [](int i, char* out, int out_size) {
        std::scoped_lock lk{g_pub_mu};
        if (i < 0 || static_cast<std::size_t>(i) >= g_names.size()) return -1;
        return copy_out(g_names[static_cast<std::size_t>(i)], out, out_size);
    },
    [](char* out, int out_size) {
        std::scoped_lock lk{g_pub_mu};
        if (out && out_size > 0) *out = '\0';
        return g_gamertag.empty() ? 0 : copy_out(g_gamertag, out, out_size);
    },
};
} // namespace

namespace wmb {

const FH2ModApi* wait_for_platform(DWORD ms) {
    // Only the first caller waits; after that, no platform means no platform.
    if (!g_api && !g_gave_up && g_ready && WaitForSingleObject(g_ready, ms) != WAIT_OBJECT_0) g_gave_up = true;
    return g_api;
}

FH2Mod* platform_mod() { return g_mod; }

const FH2ModApi* platform_v2() {
    const FH2ModApi* api = g_api;
    return api && api->api_version >= 2 && g_mod ? api : nullptr;
}

const FH2ModApi* platform_v3() {
    const FH2ModApi* api = g_api;
    return api && api->api_version >= 3 && g_mod ? api : nullptr;
}

void publish_drivatar_names(const std::vector<std::string>& names) {
    std::scoped_lock lk{g_pub_mu};
    g_names = names;
}

void publish_signed_in_gamertag(const std::string& gamertag) {
    std::scoped_lock lk{g_pub_mu};
    g_gamertag = gamertag;
}

} // namespace wmb

namespace {

// ---- the Mods page (Pause > Settings > MODS, platform 2.3.0 / API 4) ----------------------
// The names switches, as On/Off rows. The Drivatars are written when the game starts, so
// every change takes a restart (the game offers one when the player leaves the page).
// A change is saved to WheresMyBuddy.ini straight away.
struct ModsRow {
    const char* key;          // the row's key on the page
    const wchar_t* section;   // where it lives in WheresMyBuddy.ini
    const wchar_t* ini_key;
    int def;
    const char* label;
    const char* description;
};
const ModsRow kModsRows[] = {
    {"names_enabled", L"names", L"enabled", 1, "Friends as Drivatars",
     "Your Xbox friends, and the names in wheresmybuddy\\friends.txt, race as the game's Drivatars. Off puts the original Drivatars back."},
    {"xbox_enabled", L"xbox", L"enabled", 1, "Xbox Live friends",
     "Sign in to Xbox Live and use the people you follow. Off uses only the names in friends.txt."},
    {"favorites_first", L"names", L"favorites_first", 1, "Favourites first",
     "Your Xbox favourites go to the front of the line, and always stay in when friends take turns."},
    {"rotate", L"names", L"rotate", 1, "Friends take turns",
     "With more friends than Drivatars, a different set races each launch; favourites and friends.txt names always stay in. Off always uses the same ones."},
    {"repeat_to_fill", L"names", L"repeat_to_fill", 0, "Fill every Drivatar",
     "With fewer friends than Drivatars, your friends' names fill the rest too. Off keeps the original names there."},
};
std::wstring g_ini;

void mods_row_changed(void* user, const char*, int value) {
    const auto* r = static_cast<const ModsRow*>(user);
    WritePrivateProfileStringW(r->section, r->ini_key, value ? L"1" : L"0", g_ini.c_str());
}

void add_mods_rows(const FH2ModApi* api, FH2Mod* mod) {
    if (api->api_version < 4 || !mod) return;   // an older platform: no Mods page
    for (const ModsRow& r : kModsRows) {
        FH2Setting s{};
        s.size          = sizeof s;
        s.key           = r.key;
        s.label         = r.label;
        s.description   = r.description;
        s.value         = GetPrivateProfileIntW(r.section, r.ini_key, r.def, g_ini.c_str()) != 0;
        s.needs_restart = 1;
        s.on_change     = mods_row_changed;
        s.user          = const_cast<ModsRow*>(&r);
        api->add_setting(mod, &s);
    }
}

} // namespace

FH2MOD_EXPORT int fh2mod_init(const FH2ModApi* api) {
    if (!api || api->api_version < 1) return 1;
    // [updates] check in our ini; read here directly since the rest of the
    // settings load on our own thread.
    const std::wstring ini = std::wstring{api->azimuth_dir} + L"\\wheresmybuddy\\WheresMyBuddy.ini";
    FH2ModInfo info{};
    info.size             = sizeof info;
    info.name             = "WheresMyBuddy";
    info.version          = WMB_VERSION;
    info.github_repo      = "Trinculo54/WheresMyBuddy";
    info.platform_version = FH2MOD_PLATFORM_VERSION;
    info.check_updates    = GetPrivateProfileIntW(L"updates", L"check", 1, ini.c_str()) != 0;
    g_mod = api->register_mod(&info);
    g_api = api;
    if (api->api_version >= 2 && g_mod) api->publish_interface(g_mod, WHERESMYBUDDY_FRIENDS, 1, &g_friends);
    g_ini = ini;
    add_mods_rows(api, g_mod);
    if (g_ready) SetEvent(g_ready);
    return g_mod ? 0 : 1;
}
