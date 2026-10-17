// Offline test: rewrite a copy of the real DefaultCache.bin and check it.
//   wmb_test.exe <DefaultCache.bin>
#include "common.hpp"
#include <cstdio>
#include <set>
#include <fstream>

using namespace wmb;
namespace fs = std::filesystem;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %d: %s\n", __LINE__, #c); ++fails; } else std::printf("ok   %s\n", #c); } while (0)

static std::vector<std::wstring> names_in(const fs::path& f) {
    std::ifstream in{f, std::ios::binary};
    std::vector<char> d((std::istreambuf_iterator<char>(in)), {});
    std::vector<std::wstring> out;
    for (size_t i = 0; i < 125; ++i) {
        const char* p = d.data() + 8 + i * 0x2658;
        std::wstring w;
        for (size_t j = 0; j < 32; j += 2) { wchar_t c = (unsigned char)p[j] | ((unsigned char)p[j+1] << 8); if (!c) break; w += c; }
        out.push_back(w);
    }
    return out;
}

static std::string read_all(const fs::path& f) {
    std::ifstream in{f, std::ios::binary};
    return {std::istreambuf_iterator<char>(in), {}};
}

int main(int argc, char** argv) {
    wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
    fs::path root = fs::path{tmp} / L"wmb_test";
    fs::remove_all(root);
    Paths p;
    p.azimuth = root; p.data = root / "wheresmybuddy"; p.mount = root / "Mount";
    p.original = p.mount / "media" / "drivatarglobalcache" / "DefaultCache.bin";
    p.override_ = p.mount / "mediaoverride" / "drivatarglobalcache" / "DefaultCache.bin";
    fs::create_directories(p.original.parent_path());
    fs::copy_file(argv[1], p.original);
    log_open(root / "test.log");

    Settings s = load_settings(p);
    CHECK(fs::exists(p.data / "WheresMyBuddy.ini") && fs::exists(p.data / "friends.txt"));
    CHECK(s.enabled && s.max_names == 25 && !s.repeat_to_fill);
    const auto orig = names_in(p.original);

    // manual first, dedupe case-insensitively, skip >15 chars
    auto merged = merge_names({"Trinculo54", "BuddyOne"}, {"buddyone", "Xbox Pal", "ThisNameIsWayTooLong", "Ünïcødé"});
    CHECK(merged.size() == 4 && merged[0] == "Trinculo54" && merged[1] == "BuddyOne" && merged[2] == "Xbox Pal");

    // 4 names, keep leftovers
    CHECK(write_override(p, s, merged) == 4);
    auto n = names_in(p.override_);
    CHECK(n[0] == L"Trinculo54" && n[25] == L"Trinculo54" && n[124] == orig[124]);
    CHECK(n[1] == L"BuddyOne" && n[2] == L"Xbox Pal" && n[3] == L"\u00dcn\u00efc\u00f8d\u00e9");
    CHECK(n[4] == orig[4]);  // 5th distinct name untouched
    // nothing but names changed
    {
        CHECK(names_in(p.original) == orig);  // game file untouched
        std::ifstream a{p.original, std::ios::binary}, b{p.override_, std::ios::binary};
        std::vector<char> da((std::istreambuf_iterator<char>(a)), {}), db((std::istreambuf_iterator<char>(b)), {});
        size_t outside = 0;
        for (size_t i = 0; i < da.size(); ++i) {
            bool in_name = i >= 8 && ((i - 8) % 0x2658) < 32;
            if (!in_name && da[i] != db[i]) ++outside;
        }
        CHECK(da.size() == db.size() && outside == 0);
    }

    // repeat to fill
    s.repeat_to_fill = true;
    CHECK(write_override(p, s, merged) == 25);
    n = names_in(p.override_);
    CHECK(n[4] == L"Trinculo54" && n[24] == L"Trinculo54" && n[124] == L"Trinculo54");

    // max_names
    s.repeat_to_fill = false; s.max_names = 2;
    CHECK(write_override(p, s, merged) == 2);
    n = names_in(p.override_);
    CHECK(n[1] == L"BuddyOne" && n[2] == orig[2]);

    // friends cache round trip
    save_cached_friends(p, {"A", "B c"}, {"A"});
    std::vector<std::string> favs;
    auto c = read_cached_friends(p, &favs);
    CHECK(c.size() == 2 && c[1] == "B c" && favs.size() == 1 && favs[0] == "A");

    // rotation: 1 friends.txt name + 3 favourites + 28 others through 25 slots
    {
        Settings r = s;
        r.max_names = 25; r.rotate = true; r.favorites_first = true;
        std::vector<std::string> xbox, fv{"Fav1", "Fav2", "Fav3"};
        for (auto& f : fv) xbox.push_back(f);
        for (int i = 0; i < 28; ++i) xbox.push_back("Pal" + std::to_string(i));
        const std::vector<std::string> man{"Hand"};
        const auto merged = merge_names(man, xbox);
        std::size_t next = 0;
        auto a = pick_lineup(r, merged, man, fv, 0, &next);
        CHECK(a.size() == 25 && a[0] == "Hand" && a[3] == "Fav3" && a[4] == "Pal0" && a[24] == "Pal20" && next == 21);
        auto b = pick_lineup(r, merged, man, fv, next, &next);
        CHECK(b.size() == 25 && b[3] == "Fav3" && b[4] == "Pal21" && b[10] == "Pal27" && b[11] == "Pal0" && next == 14);
        // over a few launches everyone gets a turn
        std::set<std::string> seen;
        std::size_t at = 0;
        for (int launch = 0; launch < 2; ++launch)
            for (auto& n : pick_lineup(r, merged, man, fv, at, &at)) seen.insert(n);
        CHECK(seen.size() == merged.size());
        // off: always the first 25 (write_override takes the first max_names)
        r.rotate = false;
        CHECK(pick_lineup(r, merged, man, fv, 5, &next) == merged && next == 5);
        // fewer friends than slots: nothing to rotate
        r.rotate = true;
        std::vector<std::string> few{"x", "y"};
        CHECK(pick_lineup(r, few, {}, {}, 3, &next) == few);
        // favourites alone fill the slots
        r.max_names = 2;
        auto f2 = pick_lineup(r, merged, man, fv, 0, &next);
        CHECK(f2.size() == 2 && f2[0] == "Hand" && f2[1] == "Fav1");
    }

    // friends.txt with comments/blank lines
    { std::ofstream f{p.data / "friends.txt"}; f << "\xEF\xBB\xBF# hi\n  Alice  \n\n#x\nBob\n"; }
    auto m = read_manual_names(p);
    CHECK(m.size() == 2 && m[0] == "Alice" && m[1] == "Bob");

    // no names / disabled -> override removed
    CHECK(write_override(p, s, {}) == 0 && !fs::exists(p.override_));
    // ...and the game's own file was never touched
    {
        std::ifstream a{p.original, std::ios::binary}, b{argv[1], std::ios::binary};
        std::vector<char> da((std::istreambuf_iterator<char>(a)), {}), db((std::istreambuf_iterator<char>(b)), {});
        CHECK(da == db);
    }

    std::printf("\n%s (%d failures)\n", fails ? "FAILED" : "ALL PASSED", fails);
    return fails;
}
