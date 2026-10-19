// Paths, settings, the Drivatar cache rewrite and the name lists.

#include "common.hpp"
#include "platform_link.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace wmb {

namespace fs = std::filesystem;
using json   = nlohmann::json;

// ---- paths --------------------------------------------------------------------

Paths find_paths(const fs::path& azimuth, const fs::path& data) {
    Paths p;
    p.azimuth = azimuth;
    p.data    = data;

    // Azimuth keeps the game folder in data\azimuth.ini (UTF-16):
    //   [title-1a96545b] folder=FH2   -> <folder>\Mount is game:\ .
    std::error_code ec;
    const auto ini = (p.azimuth / "data" / "azimuth.ini").wstring();
    wchar_t folder[MAX_PATH]{};
    GetPrivateProfileStringW(L"title-1a96545b", L"folder", L"", folder, MAX_PATH, ini.c_str());
    std::vector<fs::path> candidates;
    if (folder[0]) {
        fs::path f{folder};
        if (f.is_relative()) f = p.azimuth / f;
        candidates.push_back(f / "Mount");
        candidates.push_back(f);
    }
    candidates.push_back(p.azimuth / "FH2" / "Mount");
    candidates.push_back(p.azimuth); // Mezcal: next to the game exe
    for (const auto& m : candidates) {
        if (fs::exists(m / "media" / "drivatarglobalcache" / "DefaultCache.bin", ec)) {
            p.mount = m;
            break;
        }
    }
    if (!p.mount.empty()) {
        p.original  = p.mount / "media" / "drivatarglobalcache" / "DefaultCache.bin";
        p.override_ = p.mount / "mediaoverride" / "drivatarglobalcache" / "DefaultCache.bin";
    }
    return p;
}

// ---- settings -----------------------------------------------------------------

namespace {

constexpr const char* kDefaultIni =
    "; WheresMyBuddy -- your Xbox friends as Forza Horizon 2 Drivatars\r\n"
    "; Changes apply the next time you start Azimuth.\r\n"
    "\r\n"
    "[names]\r\n"
    "; 0 turns the mod off and restores the original Drivatars.\r\n"
    "enabled=1\r\n"
    "\r\n"
    "; How many of the 25 built-in Drivatar names to replace (1-25).\r\n"
    "max_names=25\r\n"
    "\r\n"
    "; If you have fewer names than max_names: 0 keeps the leftover\r\n"
    "; original Drivatars, 1 repeats your names to fill every slot.\r\n"
    "repeat_to_fill=0\r\n"
    "\r\n"
    "; Put your Xbox favourites first (and, with rotate=1, always in).\r\n"
    "favorites_first=1\r\n"
    "\r\n"
    "; If you have more friends than max_names: 1 rotates through all of\r\n"
    "; them, a different set each launch (favourites and friends.txt names\r\n"
    "; always stay in); 0 always uses the same first ones.\r\n"
    "rotate=1\r\n"
    "\r\n"
    "; You can also add names by hand in friends.txt (one per line);\r\n"
    "; those always come first.\r\n"
    "\r\n"
    "[xbox]\r\n"
    "; 1 = sign in to Xbox Live and use the people you follow.\r\n"
    "enabled=1\r\n"
    "\r\n"
    "; 1 = sign in on the normal Microsoft page in your browser.\r\n"
    "; 0 = get a code to enter at microsoft.com/link instead.\r\n"
    "browser_sign_in=1\r\n"
    "\r\n"
    "; Azure app (client) ID used for Microsoft sign-in.\r\n"
    "client_id=" WMB_DEFAULT_CLIENT_ID "\r\n";

#define WMB_REDO_ONCE "; To redo a \"once\", delete FH2ML\\data\\wheresmybuddy\\profile-applied.ini."
constexpr const char* kProfileBlock =
    "\r\n"
    "[profile]\r\n"
    "; Use your Xbox gamertag as your in-game name (Azimuth's player name)\r\n"
    "; and your Xbox gamerpic as your avatar (Extra Options' Gamerpic.png).\r\n"
    ";   once   = set it after your first sign-in, then leave it alone so you\r\n"
    ";            can change it yourself in the Azimuth launcher\r\n"
    ";   always = keep it matched to Xbox every launch\r\n"
    ";   off    = never touch it\r\n"
    WMB_REDO_ONCE "\r\n"
    "player_name=once\r\n"
    "gamerpic=once\r\n";
// What that line said when the ini lived in <Azimuth>\wheresmybuddy.
constexpr std::string_view kOldRedoOnce = "; To redo a \"once\", delete wheresmybuddy\\profile-applied.ini.";

constexpr const char* kDefaultFriendsTxt =
    "# WheresMyBuddy: extra Drivatar names, one per line (max 15 characters).\r\n"
    "# These are used before your Xbox friends. Lines starting with # are ignored.\r\n";

int ini_int(const std::wstring& ini, const wchar_t* sec, const wchar_t* key, int def) {
    return static_cast<int>(GetPrivateProfileIntW(sec, key, def, ini.c_str()));
}

std::string trim(std::string s) {
    auto ws = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!s.empty() && ws(static_cast<unsigned char>(s.back()))) s.pop_back();
    std::size_t i = 0;
    while (i < s.size() && ws(static_cast<unsigned char>(s[i]))) ++i;
    return s.substr(i);
}

std::wstring lower(std::wstring s) {
    for (auto& c : s) c = static_cast<wchar_t>(towlower(c));
    return s;
}

// Inis from before FH2ML (brought over from <Azimuth>\wheresmybuddy): [updates] goes,
// since FH2ML decides update checks now (FH2ML\config\FH2ML.json), and the [profile]
// comment names the new folder. `text` is the ini as read; it's rewritten if needed.
void upgrade_ini(const fs::path& ini_path, const std::string& text) {
    const std::wstring ini = ini_path.wstring();
    const bool had_updates_check_off = GetPrivateProfileIntW(L"updates", L"check", 1, ini.c_str()) == 0;
    std::string out;
    bool in_updates = false, dropped = false, renamed = false;
    for (std::size_t pos = 0; pos < text.size();) {
        std::size_t eol = text.find('\n', pos);
        eol              = eol == std::string::npos ? text.size() : eol + 1;
        std::string line = text.substr(pos, eol - pos);
        pos              = eol;
        std::string t    = trim(line);
        if (!t.empty() && t[0] == '[') {
            for (auto& c : t) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            in_updates = t == "[updates]";
        }
        if (in_updates) {
            dropped = true;
            continue;
        }
        if (trim(line) == kOldRedoOnce) {
            const std::size_t end = line.find_last_not_of("\r\n"); // keep its line ending
            line    = WMB_REDO_ONCE + line.substr(end == std::string::npos ? 0 : end + 1);
            renamed = true;
        }
        out += line;
    }
    if (!dropped && !renamed) return;
    // [updates] was the last section: don't leave blank lines at the end.
    while (out.size() >= 4 && out.ends_with("\r\n\r\n")) out.resize(out.size() - 2);
    std::ofstream{ini_path, std::ios::binary | std::ios::trunc} << out;
    if (dropped) {
        log("removed [updates] from WheresMyBuddy.ini: FH2ML checks for updates now, as set in "
            "FH2ML\\config\\FH2ML.json");
        if (had_updates_check_off)
            log("you had check=0 there; to keep WheresMyBuddy from checking, add \"mods\": {{\"wheresmybuddy\": "
                "{{\"updateChecks\": false}}}} to FH2ML\\config\\FH2ML.json");
    }
}

} // namespace

Settings load_settings(const Paths& p) {
    std::error_code ec;
    fs::create_directories(p.data, ec);
    const auto ini_path = p.data / "WheresMyBuddy.ini";
    if (!fs::exists(ini_path, ec)) {
        std::ofstream{ini_path, std::ios::binary} << kDefaultIni << kProfileBlock;
    } else {
        std::ifstream in{ini_path, std::ios::binary};
        const std::string text((std::istreambuf_iterator<char>(in)), {});
        in.close();
        upgrade_ini(ini_path, text);
        // Inis written before [profile] existed get the new section added.
        if (text.find("[profile]") == std::string::npos)
            std::ofstream{ini_path, std::ios::binary | std::ios::app} << kProfileBlock;
    }
    if (!fs::exists(p.data / "friends.txt", ec))
        std::ofstream{p.data / "friends.txt", std::ios::binary} << kDefaultFriendsTxt;

    const std::wstring ini = ini_path.wstring();
    Settings s;
    s.enabled         = ini_int(ini, L"names", L"enabled", 1) != 0;
    s.max_names       = std::clamp(ini_int(ini, L"names", L"max_names", 25), 1, 25);
    s.repeat_to_fill  = ini_int(ini, L"names", L"repeat_to_fill", 0) != 0;
    s.favorites_first = ini_int(ini, L"names", L"favorites_first", 1) != 0;
    s.rotate          = ini_int(ini, L"names", L"rotate", 1) != 0;
    {
        // Inis from before rotation existed: add the key so it's discoverable.
        wchar_t v[8]{};
        if (!GetPrivateProfileStringW(L"names", L"rotate", L"", v, 8, ini.c_str()))
            WritePrivateProfileStringW(L"names", L"rotate", L"1", ini.c_str());
    }
    s.xbox            = ini_int(ini, L"xbox", L"enabled", 1) != 0;
    s.browser_sign_in = ini_int(ini, L"xbox", L"browser_sign_in", 1) != 0;
    wchar_t id[128]{};
    GetPrivateProfileStringW(L"xbox", L"client_id", L"", id, 128, ini.c_str());
    s.client_id = trim(to_utf8(id));
    auto apply = [&](const wchar_t* key) {
        wchar_t v[32]{};
        GetPrivateProfileStringW(L"profile", key, L"once", v, 32, ini.c_str());
        const std::wstring w = lower(to_wide(trim(to_utf8(v))));
        if (w == L"off" || w == L"0" || w == L"never") return Settings::Apply::Off;
        if (w == L"always" || w == L"2") return Settings::Apply::Always;
        return Settings::Apply::Once;
    };
    s.player_name = apply(L"player_name");
    s.gamerpic    = apply(L"gamerpic");
    // Blank in the ini (e.g. written by an older build) -> the built-in ID.
    if (s.client_id.empty()) s.client_id = WMB_DEFAULT_CLIENT_ID;
    return s;
}

// ---- Drivatar cache -----------------------------------------------------------
//
// media\drivatarglobalcache\DefaultCache.bin (Xbox One FH2):
//   u32 version (4), u32 count (125), then `count` records of 0x2658 bytes.
//   Each record starts with the gamertag as UTF-16, NUL-terminated, in a
//   fixed buffer (15 chars max, like classic gamertags). The 125 records
//   use 25 distinct gamertags, five Drivatars each.

namespace {
constexpr std::size_t kHeader     = 8;
constexpr std::size_t kRecord     = 0x2658;
constexpr std::size_t kNameBytes  = 32; // 16 UTF-16 units incl. NUL
constexpr uint32_t kVersion       = 4;

std::wstring record_name(const std::vector<char>& d, std::size_t i) {
    const char* p = d.data() + kHeader + i * kRecord;
    std::wstring w;
    for (std::size_t j = 0; j + 1 < kNameBytes; j += 2) {
        const wchar_t c = static_cast<wchar_t>(static_cast<unsigned char>(p[j]) |
                                               (static_cast<unsigned char>(p[j + 1]) << 8));
        if (!c) break;
        w.push_back(c);
    }
    return w;
}
} // namespace

namespace {

// Write beside the target, then swap it in, so the game never sees a
// half-written file.
bool replace_file(const fs::path& target, const std::vector<char>& d) {
    std::error_code ec;
    fs::create_directories(target.parent_path(), ec);
    const auto tmp = target.wstring() + L".tmp";
    {
        std::ofstream out{fs::path{tmp}, std::ios::binary | std::ios::trunc};
        out.write(d.data(), static_cast<std::streamsize>(d.size()));
        if (!out) {
            log("couldn't write {}", to_utf8(tmp));
            return false;
        }
    }
    // Game files often come read-only; that would block the swap.
    const DWORD attrs = GetFileAttributesW(target.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY))
        SetFileAttributesW(target.c_str(), attrs & ~FILE_ATTRIBUTE_READONLY);
    if (!MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        log("couldn't replace {} (error {})", to_utf8(target.wstring()), GetLastError());
        DeleteFileW(tmp.c_str());
        return false;
    }
    return true;
}

constexpr const char* kCacheMediaPath = "drivatarglobalcache/DefaultCache.bin";

// The override goes through FH2ML's file overrides (FH2ML then knows the file
// is ours, and cleans it up if WheresMyBuddy is removed). Without FH2ML (the
// offline tests), straight to mediaoverride.
bool store_override(const Paths& p, const std::vector<char>& d) {
    const Fh2ml* f = fh2ml();
    if (!f) return replace_file(p.override_, d);
    // Before a loader managed it, WheresMyBuddy wrote this file itself.
    // Remove that copy once, so FH2ML doesn't keep it as a backup. (The
    // marker comes over with the old folder: FH2ML took over the records of
    // the loader that wrote it since, so that copy is known to it.)
    std::error_code ec;
    const auto marker = p.data / "override-by-platform";
    if (!fs::exists(marker, ec)) {
        SetFileAttributesW(p.override_.c_str(), FILE_ATTRIBUTE_NORMAL);
        fs::remove(p.override_, ec);
        std::ofstream{marker} << "DefaultCache.bin in mediaoverride is written through FH2ML's file overrides\n";
    }
    return f->S->SetFile(f->mod, kCacheMediaPath, d.data(), d.size()) == 1;
}

void remove_override(const Paths& p) {
    std::error_code ec;
    bool removed = false;
    if (const Fh2ml* f = fh2ml()) {
        removed = fs::exists(p.override_, ec);
        f->S->ClearFile(f->mod, kCacheMediaPath);
    }
    SetFileAttributesW(p.override_.c_str(), FILE_ATTRIBUTE_NORMAL);
    if (fs::remove(p.override_, ec)) removed = true;
    if (removed) log("removed the override; the original Drivatars are back");
}

} // namespace

int write_override(const Paths& p, const Settings& s, const std::vector<std::string>& names) {
    std::error_code ec;
    if (p.original.empty()) {
        log("couldn't find FH2's media\\drivatarglobalcache\\DefaultCache.bin");
        return -1;
    }

    // Off, or nothing to use: remove our override and put the game's own
    // file back, so it uses the original Drivatars.
    if (!s.enabled || names.empty()) {
        remove_override(p);
        write_ai_players(p, {});
        publish_drivatar_names({});
        return 0;
    }

    std::vector<char> d;
    {
        std::ifstream in{p.original, std::ios::binary};
        d.assign(std::istreambuf_iterator<char>(in), {});
    }
    uint32_t version = 0, count = 0;
    if (d.size() >= kHeader) {
        std::memcpy(&version, d.data(), 4);
        std::memcpy(&count, d.data() + 4, 4);
    }
    if (version != kVersion || !count || d.size() != kHeader + count * kRecord) {
        log("DefaultCache.bin isn't in the expected format (version {}, {} records, {} bytes)",
            version, count, d.size());
        return -1;
    }

    // Distinct built-in names, in file order.
    std::vector<std::wstring> slots;
    for (std::size_t i = 0; i < count; ++i) {
        auto n = record_name(d, i);
        if (std::find(slots.begin(), slots.end(), n) == slots.end()) slots.push_back(n);
    }

    // slot -> new name
    const std::size_t want = std::min<std::size_t>(static_cast<std::size_t>(s.max_names), slots.size());
    std::vector<std::wstring> repl(slots.size());
    std::size_t used = 0;
    for (std::size_t i = 0; i < want; ++i) {
        if (i < names.size()) repl[i] = to_wide(names[i]);
        else if (s.repeat_to_fill) repl[i] = to_wide(names[i % names.size()]);
        else break;
        ++used;
    }

    for (std::size_t i = 0; i < count; ++i) {
        const auto old = record_name(d, i);
        const auto it  = std::find(slots.begin(), slots.end(), old);
        const auto& nw = repl[static_cast<std::size_t>(it - slots.begin())];
        if (nw.empty()) continue;
        char* dst = d.data() + kHeader + i * kRecord;
        std::memset(dst, 0, kNameBytes);
        for (std::size_t j = 0; j < nw.size() && j < kMaxGamertag; ++j) {
            dst[j * 2]     = static_cast<char>(nw[j] & 0xFF);
            dst[j * 2 + 1] = static_cast<char>((nw[j] >> 8) & 0xFF);
        }
    }

    if (!store_override(p, d)) return -1;
    {
        std::vector<std::string> placed;
        for (const auto& r : repl)
            if (!r.empty() && std::find(placed.begin(), placed.end(), to_utf8(r)) == placed.end())
                placed.push_back(to_utf8(r));
        publish_drivatar_names(placed);
    }

    // The same names for the AI driver string table (car meets etc.);
    // "HRZN Ben" is only in there, so he gets the next name in line.
    std::map<std::wstring, std::wstring> rename;
    for (std::size_t i = 0; i < slots.size(); ++i)
        if (!repl[i].empty()) rename[slots[i]] = repl[i];
    if (used < names.size()) rename[L"HRZN Ben"] = to_wide(names[used]);
    write_ai_players(p, rename);

    log("wrote {} with {} of {} Drivatar names replaced:", to_utf8(p.override_.wstring()), used,
        slots.size());
    for (std::size_t i = 0; i < slots.size(); ++i)
        if (!repl[i].empty()) log("  {:<15} -> {}", to_utf8(slots[i]), to_utf8(repl[i]));
    return static_cast<int>(used);
}

// ---- names --------------------------------------------------------------------

std::vector<std::string> read_manual_names(const Paths& p) {
    std::vector<std::string> out;
    std::ifstream in{p.data / "friends.txt"};
    std::string line;
    while (std::getline(in, line)) {
        if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF) line.erase(0, 3); // BOM
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        out.push_back(line);
    }
    return out;
}

std::vector<std::string> read_cached_friends(const Paths& p, std::vector<std::string>* favorites) {
    std::vector<std::string> out;
    if (favorites) favorites->clear();
    std::ifstream in{p.data / "friends-cache.json"};
    if (!in) return out;
    try {
        const auto j = json::parse(in);
        for (const auto& f : j.at("friends")) out.push_back(f.get<std::string>());
        if (favorites && j.contains("favorites"))
            for (const auto& f : j.at("favorites")) favorites->push_back(f.get<std::string>());
    } catch (...) {}
    return out;
}

void save_cached_friends(const Paths& p, const std::vector<std::string>& friends,
                         const std::vector<std::string>& favorites) {
    SYSTEMTIME t;
    GetLocalTime(&t);
    const json j{{"updated", std::format("{:04}-{:02}-{:02} {:02}:{:02}", t.wYear, t.wMonth, t.wDay,
                                         t.wHour, t.wMinute)},
                 {"friends", friends},
                 {"favorites", favorites}};
    std::ofstream{p.data / "friends-cache.json", std::ios::trunc} << j.dump(2);
}

std::size_t read_rotation(const Paths& p) {
    return GetPrivateProfileIntW(L"rotation", L"next", 0, (p.data / "rotation.ini").wstring().c_str());
}

void save_rotation(const Paths& p, std::size_t next) {
    WritePrivateProfileStringW(L"rotation", L"next", std::to_wstring(next).c_str(),
                               (p.data / "rotation.ini").wstring().c_str());
}

std::vector<std::string> pick_lineup(const Settings& s, const std::vector<std::string>& merged,
                                     const std::vector<std::string>& manual,
                                     const std::vector<std::string>& favorites, std::size_t start,
                                     std::size_t* next) {
    const std::size_t slots = static_cast<std::size_t>(std::clamp(s.max_names, 1, 25));
    if (next) *next = start;
    if (!s.rotate || merged.size() <= slots) return merged;

    // Who always stays in: friends.txt names, and favourites if they go first.
    std::unordered_set<std::wstring> keep;
    for (const auto& n : manual) keep.insert(lower(to_wide(n)));
    if (s.favorites_first)
        for (const auto& n : favorites) keep.insert(lower(to_wide(n)));
    std::vector<std::string> out, pool;
    for (const auto& n : merged) (keep.count(lower(to_wide(n))) ? out : pool).push_back(n);
    if (out.size() >= slots) {
        log("{} favourites / friends.txt names already fill all {} Drivatar names, so there's nothing to "
            "rotate",
            out.size(), slots);
        out.resize(slots);
        return out;
    }
    const std::size_t free = slots - out.size();
    start %= pool.size();
    for (std::size_t i = 0; i < free; ++i) out.push_back(pool[(start + i) % pool.size()]);
    if (next) *next = (start + free) % pool.size();
    log("rotating {} friends through {} Drivatar names: {} always in, {} of the other {} this launch", merged.size(),
        slots, slots - free, free, pool.size());
    return out;
}

std::vector<std::string> merge_names(const std::vector<std::string>& manual,
                                     const std::vector<std::string>& xbox) {
    std::vector<std::string> out;
    std::unordered_set<std::wstring> seen;
    auto add = [&](const std::string& n) {
        const std::wstring w = to_wide(n);
        if (w.empty()) return;
        if (w.size() > kMaxGamertag) {
            log("skipping \"{}\": longer than {} characters", n, kMaxGamertag);
            return;
        }
        if (seen.insert(lower(w)).second) out.push_back(n);
    };
    for (const auto& n : manual) add(n);
    for (const auto& n : xbox) add(n);
    return out;
}

} // namespace wmb
