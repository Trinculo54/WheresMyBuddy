#include "platform_link.hpp"

#include "wheresmybuddy.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {
wmb::Fh2ml g_fh2ml;
std::atomic<const wmb::Fh2ml*> g_connected{nullptr};

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

// Once: copies the platform-era files (a file, or a folder's contents) into the data
// folder. Files already there win. A marker file remembers that it was done.
// Copy, don't move: the old folder stays where it was until the player removes it.
void import_from_platform(const FH2ML_ModContext_v4* c,
                          std::initializer_list<std::pair<const wchar_t*, const wchar_t*>> items) {
    std::error_code e;
    const fs::path data = c->data_directory, marker = data / L"imported-from-fh2-mods.txt";
    if (fs::exists(marker, e)) return;
    for (auto [from, to] : items) {
        const fs::path src = fs::path(c->services->azimuth_directory) / from, dst = data / to;
        if (!fs::exists(src, e)) continue;
        fs::create_directories(fs::is_directory(src, e) ? dst : dst.parent_path(), e);
        fs::copy(src, dst, fs::copy_options::recursive | fs::copy_options::skip_existing, e);
    }
    std::ofstream(marker) << "Copied from the FH2 Mod Platform's folders on the first launch under FH2ML.\n";
}

// ---- the Mods page (Pause > Settings > MODS) -----------------------------------------------
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
     "Your Xbox friends, and the names in FH2ML\\data\\wheresmybuddy\\friends.txt, race as the game's Drivatars. Off puts the original Drivatars back."},
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

void add_mods_rows(const FH2ML_Services_v4* S, FH2ML_Mod* mod) {
    for (const ModsRow& r : kModsRows) {
        FH2ML_Setting s{};
        s.size          = sizeof s;
        s.key           = r.key;
        s.label         = r.label;
        s.description   = r.description;
        s.value         = GetPrivateProfileIntW(r.section, r.ini_key, r.def, g_ini.c_str()) != 0;
        s.needs_restart = 1;
        s.on_change     = mods_row_changed;
        s.user          = const_cast<ModsRow*>(&r);
        S->AddSetting(mod, &s);
    }
}

} // namespace

namespace wmb {

bool connect_fh2ml(const FH2ML_ModContext_v4* c) {
    if (!c || !c->services || !c->mod || !c->data_directory || !c->services->azimuth_directory) return false;
    g_fh2ml.S       = c->services;
    g_fh2ml.mod     = c->mod;
    g_fh2ml.azimuth = c->services->azimuth_directory;
    g_fh2ml.data    = c->data_directory;
    // Before FH2ML, everything lived in <Azimuth>\wheresmybuddy: bring it all over
    // (settings, friends.txt, the saved friends list, the sign-in) before reading any of it.
    import_from_platform(c, {{L"wheresmybuddy", L"."}});
    g_ini = g_fh2ml.data + L"\\WheresMyBuddy.ini";
    g_fh2ml.S->PublishInterface(g_fh2ml.mod, WHERESMYBUDDY_FRIENDS, 1, &g_friends);
    add_mods_rows(g_fh2ml.S, g_fh2ml.mod);
    g_connected.store(&g_fh2ml, std::memory_order_release);
    return true;
}

const Fh2ml* fh2ml() { return g_connected.load(std::memory_order_acquire); }

void publish_drivatar_names(const std::vector<std::string>& names) {
    std::scoped_lock lk{g_pub_mu};
    g_names = names;
}

void publish_signed_in_gamertag(const std::string& gamertag) {
    std::scoped_lock lk{g_pub_mu};
    g_gamertag = gamertag;
}

} // namespace wmb
