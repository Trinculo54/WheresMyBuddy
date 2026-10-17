// Offline test: rewrite a copy of the real DefaultCache.bin and check it.
//   wmb_test.exe <DefaultCache.bin>
#include "common.hpp"
#include <cstdio>
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
    save_cached_friends(p, {"A", "B c"});
    auto c = read_cached_friends(p);
    CHECK(c.size() == 2 && c[1] == "B c");

    // friends.txt with comments/blank lines
    { std::ofstream f{p.data / "friends.txt"}; f << "\xEF\xBB\xBF# hi\n  Alice  \n\n#x\nBob\n"; }
    auto m = read_manual_names(p);
    CHECK(m.size() == 2 && m[0] == "Alice" && m[1] == "Bob");

    // no names / disabled -> override removed
    CHECK(write_override(p, s, {}) == 0 && !fs::exists(p.override_));

    std::printf("\n%s (%d failures)\n", fails ? "FAILED" : "ALL PASSED", fails);
    return fails;
}
