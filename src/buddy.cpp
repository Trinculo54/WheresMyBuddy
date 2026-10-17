// Paths, settings, the Drivatar cache rewrite and the name lists.

#include "common.hpp"

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

Paths find_paths(HMODULE self) {
    Paths p;
    wchar_t buf[MAX_PATH]{};
    GetModuleFileNameW(self, buf, MAX_PATH);
    fs::path dir     = fs::path{buf}.parent_path();
    std::wstring leaf = dir.filename().wstring();
    for (auto& c : leaf) c = static_cast<wchar_t>(towlower(c));
    p.azimuth = leaf == L"fh2-mods" ? dir.parent_path() : dir;
    p.data    = p.azimuth / "wheresmybuddy";

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
    "; Put your Xbox favourites first.\r\n"
    "favorites_first=1\r\n"
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

constexpr const char* kProfileBlock =
    "\r\n"
    "[profile]\r\n"
    "; Use your Xbox gamertag as your in-game name (Azimuth's player name)\r\n"
    "; and your Xbox gamerpic as your avatar (Extra Options' Gamerpic.png).\r\n"
    ";   once   = set it after your first sign-in, then leave it alone so you\r\n"
    ";            can change it yourself in the Azimuth launcher\r\n"
    ";   always = keep it matched to Xbox every launch\r\n"
    ";   off    = never touch it\r\n"
    "; To redo a \"once\", delete wheresmybuddy\\profile-applied.ini.\r\n"
    "player_name=once\r\n"
    "gamerpic=once\r\n";


constexpr const char* kUpdatesBlock =
    "\r\n"
    "[updates]\r\n"
    "; 1 = check GitHub for a newer WheresMyBuddy (at most every 6 hours) and\r\n"
    "; say so on the title screen. 0 = never go online for this.\r\n"
    "check=1\r\n";

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

} // namespace

Settings load_settings(const Paths& p) {
    std::error_code ec;
    fs::create_directories(p.data, ec);
    const auto ini_path = p.data / "WheresMyBuddy.ini";
    if (!fs::exists(ini_path, ec)) {
        std::ofstream{ini_path, std::ios::binary} << kDefaultIni << kProfileBlock << kUpdatesBlock;
    } else {
        // Inis written before [profile] existed get the new section added.
        std::ifstream in{ini_path, std::ios::binary};
        const std::string text((std::istreambuf_iterator<char>(in)), {});
        in.close();
        if (text.find("[profile]") == std::string::npos)
            std::ofstream{ini_path, std::ios::binary | std::ios::app} << kProfileBlock;
        if (text.find("[updates]") == std::string::npos)
            std::ofstream{ini_path, std::ios::binary | std::ios::app} << kUpdatesBlock;
    }
    if (!fs::exists(p.data / "friends.txt", ec))
        std::ofstream{p.data / "friends.txt", std::ios::binary} << kDefaultFriendsTxt;

    const std::wstring ini = ini_path.wstring();
    Settings s;
    s.enabled         = ini_int(ini, L"names", L"enabled", 1) != 0;
    s.max_names       = std::clamp(ini_int(ini, L"names", L"max_names", 25), 1, 25);
    s.repeat_to_fill  = ini_int(ini, L"names", L"repeat_to_fill", 0) != 0;
    s.favorites_first = ini_int(ini, L"names", L"favorites_first", 1) != 0;
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
    s.check_updates = ini_int(ini, L"updates", L"check", 1) != 0;
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
        if (fs::remove(p.override_, ec))
            log("removed the override; the original Drivatars are back");
        write_ai_players(p, {});
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

    if (!replace_file(p.override_, d)) return -1;

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

std::vector<std::string> read_cached_friends(const Paths& p) {
    std::vector<std::string> out;
    std::ifstream in{p.data / "friends-cache.json"};
    if (!in) return out;
    try {
        const auto j = json::parse(in);
        for (const auto& f : j.at("friends")) out.push_back(f.get<std::string>());
    } catch (...) {}
    return out;
}

void save_cached_friends(const Paths& p, const std::vector<std::string>& friends) {
    SYSTEMTIME t;
    GetLocalTime(&t);
    const json j{{"updated", std::format("{:04}-{:02}-{:02} {:02}:{:02}", t.wYear, t.wMonth, t.wDay,
                                         t.wHour, t.wMinute)},
                 {"friends", friends}};
    std::ofstream{p.data / "friends-cache.json", std::ios::trunc} << j.dump(2);
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
