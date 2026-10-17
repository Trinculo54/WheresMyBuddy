// Offline test of the shared update-notice code.
//   updates_test <loader dll with version info> <EN.zip> [more zips]
#include "fh2_strings.hpp"
#include "fh2_updates.hpp"

#include <windows.h>

#include <cstdio>
#include <fstream>

using namespace fh2common;
namespace fs = std::filesystem;
static int fails = 0;
#define CHECK(x) do { bool ok_ = (x); std::printf("%s %s\n", ok_ ? "ok  " : "FAIL", #x); if (!ok_) ++fails; } while (0)

static std::wstring title_text(const fs::path& mount, const char* lang) {
    std::ifstream in{mount / "mediaoverride" / "Stripped" / "StringTables" / lang / "Main.str", std::ios::binary};
    std::vector<char> d((std::istreambuf_iterator<char>(in)), {});
    if (d.empty()) return L"<none>";
    std::wstring found;
    edit_lsb2(d, [&](uint16_t k, const std::wstring& t) {
        if (k == 0x18F0) found = t;
        return t;
    });
    return found;
}

int main(int argc, char** argv) {
    if (argc < 3) return 2;
    CHECK(compare_versions("1.0.0", "1.0.1") < 0);
    CHECK(compare_versions("v1.10.0", "1.9.9") > 0);
    CHECK(compare_versions("V1.1.0", "1.1.0") == 0);
    CHECK(compare_versions("1.1", "1.1.0") == 0);

    fs::path root = fs::temp_directory_path() / "wmb_updates_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    const fs::path mount = root / "FH2" / "Mount";
    fs::create_directories(root / "fh2-mods");
    fs::create_directories(mount / "media" / "Stripped" / "StringTables");
    for (int i = 2; i < argc; ++i)
        fs::copy_file(argv[i], mount / "media" / "Stripped" / "StringTables" / fs::path{argv[i]}.filename());
    CHECK(find_game_mount(root) == mount);

    // Loader version: none (the first loader) vs. a versioned one.
    CHECK(installed_loader_version(root) == "1.0.0");
    fs::copy_file(argv[1], root / "d3dcompiler_47.dll");
    const auto lv = installed_loader_version(root);
    std::printf("  installed loader version: %s\n", lv.c_str());
    CHECK(lv == FH2_LOADER_VERSION);

    const auto lang = fs::path{argv[2]}.stem().string();
    const std::wstring orig = title_text(mount, lang.c_str());
    CHECK(orig == L"<none>");

    // Two mods with notices -> both shown, sorted by name.
    { std::ofstream{root / "fh2-mods" / "A.dll"} << "x"; }
    { std::ofstream{root / "fh2-mods" / "B.dll"} << "x"; }
    fs::create_directories(root / "fh2-mods" / "notices");
    { std::ofstream{root / "fh2-mods" / "notices" / "Zed.txt"} << "B.dll\nZed 2.0.0 is available\n"; }
    { std::ofstream{root / "fh2-mods" / "notices" / "Alpha.txt"} << "A.dll\nAlpha 1.5.0 is available\n"; }
    refresh_title_notices(root);
    const auto t = title_text(mount, lang.c_str());
    std::printf("  title: %ls\n", t.c_str());
    CHECK(t == L"\n\n\nTO START\nAlpha 1.5.0 is available\nZed 2.0.0 is available");

    // A removed mod's notice is ignored.
    fs::remove(root / "fh2-mods" / "B.dll");
    refresh_title_notices(root);
    CHECK(title_text(mount, lang.c_str()) == L"\n\nTO START\nAlpha 1.5.0 is available");

    // No notices left -> override removed.
    fs::remove(root / "fh2-mods" / "notices" / "Alpha.txt");
    refresh_title_notices(root);
    CHECK(title_text(mount, lang.c_str()) == L"<none>");

    // A mod whose bundled loader is newer than the installed one says so
    // (offline: no GitHub check).
    fs::remove(root / "d3dcompiler_47.dll");
    std::string logged;
    start_update_check({L"WheresMyBuddy", L"A.dll", "1.1.0", "Trinculo54/WheresMyBuddy", FH2_LOADER_VERSION, root,
                        false, [&](const std::string& m) { logged += m + "\n"; }});
    for (int i = 0; i < 50 && title_text(mount, lang.c_str()) == L"<none>"; ++i) Sleep(100);
    std::printf("  log: %s  title: %ls\n", logged.c_str(), title_text(mount, lang.c_str()).c_str());
    CHECK(title_text(mount, lang.c_str()).find(L"\n\nTO START\nOld FH2 Mod Loader: reinstall WheresMyBuddy") !=
          std::wstring::npos);

    // Every other string is untouched.
    {
        auto a = unzip_file(argv[2], "Main.str");
        std::ifstream in{mount / "mediaoverride" / "Stripped" / "StringTables" / lang / "Main.str", std::ios::binary};
        std::vector<char> b((std::istreambuf_iterator<char>(in)), {});
        std::vector<std::wstring> sa, sb;
        edit_lsb2(a, [&](uint16_t k, const std::wstring& s) { if (k != 0x18F0) sa.push_back(s); return s; });
        edit_lsb2(b, [&](uint16_t k, const std::wstring& s) { if (k != 0x18F0) sb.push_back(s); return s; });
        CHECK(!sa.empty() && sa == sb);
    }

    fs::remove_all(root, ec);
    std::printf(fails ? "\n%d FAILED\n" : "\nALL PASSED\n", fails);
    return fails;
}
