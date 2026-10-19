// Your own Xbox profile in the game:
//   gamertag -> Azimuth's player name   (data\azimuth.ini, [launcher] player_name)
//   gamerpic -> Extra Options' avatar   (Gamerpic.png next to FH2_ExtraOptions.ini)
//
// By default each is done once, after the first sign-in, and then left
// alone so you can still change them yourself. What's been done is kept in
// FH2ML\data\wheresmybuddy\profile-applied.ini; delete it to redo.

#include "common.hpp"

#include <fstream>

namespace wmb {

namespace fs = std::filesystem;

namespace {

constexpr std::size_t kMaxGamerpicBytes = 16u * 1024 * 1024; // Extra Options' limit

std::wstring state_file(const Paths& p) { return (p.data / "profile-applied.ini").wstring(); }

std::wstring state_get(const Paths& p, const wchar_t* key) {
    wchar_t v[2048]{};
    GetPrivateProfileStringW(L"applied", key, L"", v, 2048, state_file(p).c_str());
    return v;
}

void state_set(const Paths& p, const wchar_t* key, const std::wstring& value) {
    WritePrivateProfileStringW(L"applied", key, value.c_str(), state_file(p).c_str());
}

bool due(Settings::Apply mode, const std::wstring& done) {
    switch (mode) {
        case Settings::Apply::Off: return false;
        case Settings::Apply::Always: return true;
        case Settings::Apply::Once: return done.empty();
    }
    return false;
}

// Extra Options reads Gamerpic.png from the folder its ini is in: next to
// azimuth.exe (winmm proxy), data\bin (older Azimuth installs) or, under
// Mezcal, next to the game exe.
std::vector<fs::path> extra_options_dirs(const Paths& p) {
    std::vector<fs::path> dirs;
    std::error_code ec;
    for (const auto& d : {p.azimuth, p.azimuth / "data" / "bin", p.mount})
        if (!d.empty() && fs::exists(d / "FH2_ExtraOptions.ini", ec)) dirs.push_back(d);
    return dirs;
}

bool is_png(const std::string& b) { return b.size() > 8 && b.compare(0, 8, "\x89PNG\r\n\x1a\n") == 0; }

std::string with_param(std::string url, const std::string& kv) {
    url += url.find('?') == std::string::npos ? '?' : '&';
    return url + kv;
}

std::string download_gamerpic(std::string url) {
    if (url.find("format=") == std::string::npos) url = with_param(url, "format=png");
    // Ask for a big square version first; the plain URL is the fallback.
    for (const auto& u : {with_param(url, "w=1080&h=1080"), url}) {
        const auto r = http_request(L"GET", to_wide(u), {});
        if (r.status == 200 && is_png(r.body) && r.body.size() <= kMaxGamerpicBytes) return r.body;
        log("gamerpic download: {} {} bytes{}{}", r.status, r.body.size(),
            r.status == 200 && !is_png(r.body) ? " (not a PNG)" : "", r.error.empty() ? "" : " " + r.error);
    }
    return {};
}

bool write_file(const fs::path& target, const std::string& data) {
    const fs::path tmp = target.wstring() + L".tmp";
    {
        std::ofstream out{tmp, std::ios::binary | std::ios::trunc};
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
        if (!out) return false;
    }
    return MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

void apply_name(const Paths& p, const Settings& s, const std::string& gamertag) {
    if (!due(s.player_name, state_get(p, L"player_name"))) return;
    const auto ini = (p.azimuth / "data" / "azimuth.ini").wstring();
    std::error_code ec;
    if (!fs::exists(ini, ec)) {
        log("in-game name: data\\azimuth.ini not found, so it wasn't changed");
        return;
    }
    const std::wstring gt = to_wide(gamertag);
    wchar_t cur[256]{};
    GetPrivateProfileStringW(L"launcher", L"player_name", L"", cur, 256, ini.c_str());
    if (gt == cur) {
        log("in-game name is already {}", gamertag);
    } else if (WritePrivateProfileStringW(L"launcher", L"player_name", gt.c_str(), ini.c_str())) {
        log("in-game name set to {} (was \"{}\"); it shows from the next launch, and you can still "
            "change it in the Azimuth launcher",
            gamertag, to_utf8(cur));
    } else {
        log("in-game name: couldn't write data\\azimuth.ini (error {})", GetLastError());
        return;
    }
    state_set(p, L"player_name", gt);
}

void apply_gamerpic(const Paths& p, const Settings& s, const std::string& url) {
    const std::wstring done = state_get(p, L"gamerpic");
    if (!due(s.gamerpic, done)) return;
    const auto dirs = extra_options_dirs(p);
    if (dirs.empty()) {
        log("gamerpic: Forza Horizon 2 Extra Options isn't installed, so there's no avatar to set");
        return;
    }
    if (url.empty()) {
        log("gamerpic: couldn't get your Xbox gamerpic this time");
        return;
    }
    std::error_code ec;
    if (s.gamerpic == Settings::Apply::Always && done == to_wide(url)) {
        bool all_there = true;
        for (const auto& d : dirs) all_there = all_there && fs::exists(d / "Gamerpic.png", ec);
        if (all_there) return; // same picture as last time
    }

    const std::string png = download_gamerpic(url);
    if (png.empty()) {
        log("gamerpic: download failed; will try again next launch");
        return;
    }
    for (const auto& d : dirs) {
        const auto target = d / "Gamerpic.png";
        const auto backup = d / "Gamerpic.before-wheresmybuddy.png";
        if (fs::exists(target, ec) && !fs::exists(backup, ec)) fs::copy_file(target, backup, ec);
        if (!write_file(target, png)) {
            log("gamerpic: couldn't write {}", to_utf8(target.wstring()));
            return;
        }
        const auto eo = (d / "FH2_ExtraOptions.ini").wstring();
        if (GetPrivateProfileIntW(L"Profile", L"CustomGamerpic", 1, eo.c_str()) == 0)
            log("gamerpic: CustomGamerpic=0 in {}, so the game won't use it; set it to 1",
                to_utf8(eo));
        log("gamerpic: wrote your Xbox gamerpic ({} KB) to {} (your old one is saved as "
            "Gamerpic.before-wheresmybuddy.png)",
            png.size() / 1024, to_utf8(target.wstring()));
    }
    state_set(p, L"gamerpic", to_wide(url));
}

} // namespace

bool profile_wants_gamerpic(const Paths& p, const Settings& s) {
    return s.gamerpic != Settings::Apply::Off && !extra_options_dirs(p).empty() &&
           (s.gamerpic == Settings::Apply::Always || state_get(p, L"gamerpic").empty());
}

void apply_xbox_profile(const Paths& p, const Settings& s, const XboxMe& me) {
    if (me.gamertag.empty()) return;
    apply_name(p, s, me.gamertag);
    apply_gamerpic(p, s, me.gamerpic_url);
}

} // namespace wmb
