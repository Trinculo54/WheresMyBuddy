// Offline test: player_name once/always/off and the [profile] ini upgrade.
#include "common.hpp"
#include <cstdio>
#include <fstream>
using namespace wmb;
namespace fs = std::filesystem;
static int fails = 0;
#define CHECK(x) do { bool ok_ = (x); std::printf("%s %s\n", ok_ ? "ok  " : "FAIL", #x); if (!ok_) ++fails; } while (0)
static std::wstring name(const fs::path& ini) { wchar_t v[64]{}; GetPrivateProfileStringW(L"launcher", L"player_name", L"", v, 64, ini.wstring().c_str()); return v; }
int main() {
    fs::path root = fs::temp_directory_path() / "wmb_profile_test";
    std::error_code ec; fs::remove_all(root, ec);
    fs::create_directories(root / "data"); fs::create_directories(root / "wheresmybuddy");
    Paths p; p.azimuth = root; p.data = root / "wheresmybuddy";
    // UTF-16 azimuth.ini like the real one
    std::wstring txt = L"\xFEFF[launcher]\r\ngame=1a96545b\r\nplayer_name=Player\r\nsettings_version=1\r\n[title-1a96545b]\r\nfolder=FH2\r\n";
    { std::ofstream o{root / "data" / "azimuth.ini", std::ios::binary}; o.write((const char*)txt.data(), txt.size() * 2); }
    // old-style ini without [profile]
    { std::ofstream o{p.data / "WheresMyBuddy.ini", std::ios::binary}; o << "[xbox]\r\nenabled=1\r\nclient_id=abc\r\n"; }
    Settings s = load_settings(p);
    std::ifstream in{p.data / "WheresMyBuddy.ini"}; std::string ini((std::istreambuf_iterator<char>(in)), {}); in.close();
    CHECK(ini.find("[profile]") != std::string::npos && ini.find("client_id=abc") != std::string::npos);
    CHECK(s.player_name == Settings::Apply::Once && s.gamerpic == Settings::Apply::Once);
    CHECK(!profile_wants_gamerpic(p, s)); // no Extra Options here
    XboxMe me{"Trinculo54", ""};
    apply_xbox_profile(p, s, me);
    CHECK(name(root / "data" / "azimuth.ini") == L"Trinculo54");
    { wchar_t v[64]{}; GetPrivateProfileStringW(L"title-1a96545b", L"folder", L"", v, 64, (root / "data" / "azimuth.ini").wstring().c_str()); CHECK(std::wstring(v) == L"FH2"); }
    // user renames it; "once" must leave it alone
    WritePrivateProfileStringW(L"launcher", L"player_name", L"MyOwnName", (root / "data" / "azimuth.ini").wstring().c_str());
    apply_xbox_profile(p, s, me);
    CHECK(name(root / "data" / "azimuth.ini") == L"MyOwnName");
    s.player_name = Settings::Apply::Always;
    apply_xbox_profile(p, s, me);
    CHECK(name(root / "data" / "azimuth.ini") == L"Trinculo54");
    // still UTF-16 with BOM
    { std::ifstream b{root / "data" / "azimuth.ini", std::ios::binary}; unsigned char h[2]{}; b.read((char*)h, 2); CHECK(h[0] == 0xFF && h[1] == 0xFE); }
    // Extra Options present -> wants gamerpic; empty URL is handled
    { std::ofstream o{root / "FH2_ExtraOptions.ini"}; o << "[Profile]\nCustomGamerpic=1\n"; }
    s.player_name = Settings::Apply::Off;
    CHECK(profile_wants_gamerpic(p, s));
    apply_xbox_profile(p, s, me);
    CHECK(!fs::exists(root / "Gamerpic.png"));
    fs::remove_all(root, ec);
    std::printf(fails ? "\n%d FAILED\n" : "\nALL PASSED\n", fails);
    return fails;
}
