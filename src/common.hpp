#pragma once

#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <format>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace wmb {

// ---- logging (wheresmybuddy\wheresmybuddy.log, fresh every launch) --------
void log_open(const std::filesystem::path& file);
void log_write(std::string_view line);
template <class... A> void log(std::format_string<A...> fmt, A&&... args) {
    log_write(std::format(fmt, std::forward<A>(args)...));
}

std::string to_utf8(std::wstring_view w);
std::wstring to_wide(std::string_view s);

// ---- paths ------------------------------------------------------------------
struct Paths {
    std::filesystem::path azimuth;   // folder with azimuth.exe
    std::filesystem::path data;      // <azimuth>\wheresmybuddy
    std::filesystem::path mount;     // <game>\Mount (game:\)
    std::filesystem::path original;  // Mount\media\drivatarglobalcache\DefaultCache.bin
    std::filesystem::path override_; // Mount\mediaoverride\drivatarglobalcache\DefaultCache.bin
};
Paths find_paths(HMODULE self);

// ---- settings (wheresmybuddy\WheresMyBuddy.ini) -----------------------------
struct Settings {
    bool enabled          = true;
    int max_names         = 25;   // how many Drivatar names to replace (1..25)
    bool repeat_to_fill   = false; // reuse your names to fill every slot
    bool favorites_first  = true;
    bool xbox             = true;
    bool browser_sign_in  = true;  // false: code at microsoft.com/link instead
    std::string client_id;        // Azure app (client) ID for Xbox sign-in
    // [profile]: copy your gamertag / gamerpic into the game.
    enum class Apply { Off, Once, Always };
    Apply player_name = Apply::Once;
    Apply gamerpic    = Apply::Once;
    bool check_updates = true; // [updates] check
};
Settings load_settings(const Paths& p);

// ---- Drivatar cache ---------------------------------------------------------
constexpr std::size_t kMaxGamertag = 15;

// Rewrites the override from the original cache, replacing up to
// settings.max_names of the 25 built-in gamertags with `names` (in order).
// Returns the number of distinct built-in names replaced, or -1 on error.
int write_override(const Paths& p, const Settings& s, const std::vector<std::string>& names);

// ---- AI driver names (aiplayers.cpp) ----------------------------------------
// Writes mediaoverride\Stripped\StringTables\<lang>\AIPlayers.str for every
// language, with names swapped per `rename` (old -> new). Empty removes them.
void write_ai_players(const Paths& p, const std::map<std::wstring, std::wstring>& rename);

// ---- names ------------------------------------------------------------------
std::vector<std::string> read_manual_names(const Paths& p);   // friends.txt
std::vector<std::string> read_cached_friends(const Paths& p); // friends-cache.json
void save_cached_friends(const Paths& p, const std::vector<std::string>& friends);

// Manual names first, then Xbox friends; de-duplicated, length-checked.
std::vector<std::string> merge_names(const std::vector<std::string>& manual,
                                     const std::vector<std::string>& xbox);

// ---- Xbox Live --------------------------------------------------------------
// Signs in (device code on first run, cached refresh token after) and returns
// the gamertags of the people you follow, favourites first. Empty + logged
// reason on failure.
struct XboxMe {
    std::string gamertag;     // classic gamertag of whoever signed in
    std::string gamerpic_url; // their gamerpic (only fetched when asked for)
};
bool fetch_xbox_friends(const Paths& p, const Settings& s, std::vector<std::string>& out,
                        XboxMe* me = nullptr, bool want_gamerpic = false);

// ---- your own profile (profile.cpp) ------------------------------------------
// Whether apply_xbox_profile has anything left to do (decides if the
// gamerpic URL is worth fetching).
bool profile_wants_gamerpic(const Paths& p, const Settings& s);
// Sets Azimuth's player_name and Extra Options' Gamerpic.png from your
// Xbox profile, once (or every launch), per settings.
void apply_xbox_profile(const Paths& p, const Settings& s, const XboxMe& me);

// ---- championships (championship.cpp) ---------------------------------------
// Renames the Drivatars saved with a championship (which don't come from the
// override) to match the current Drivatar list. Installs itself once the
// game has been mapped.
void start_championship_fix(const Paths& p);
// Call after rewriting the override so later loads use the new names.
void update_championship_names(const Paths& p);

// ---- HTTP (WinHTTP) ---------------------------------------------------------
struct HttpResult {
    int status = 0;
    std::string body;
    std::string error;
};
HttpResult http_request(const wchar_t* method, const std::wstring& url,
                        const std::vector<std::pair<std::wstring, std::wstring>>& headers,
                        const std::string& body = {});
std::string url_encode(std::string_view s);

} // namespace wmb
