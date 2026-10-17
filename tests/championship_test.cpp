// Offline test of the championship fix against the real game code.
//   championship_test <fh2.exe> <DefaultCache.bin> <BinarySaveState>
// Maps the exe at its preferred base (like Azimuth), lets the fix find it and
// patch ChampionshipDrivatarsState's vtable, then calls Serialize through the
// patched slot in "saving" mode with a fake archive (which only needs the
// archive's write path, not the game's allocator) on a state holding the
// 11 Drivatars from a real save. Checks every one ends up with the name its
// id has in the override.
#include "common.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>

using namespace wmb;
namespace fs = std::filesystem;
static int fails = 0;
#define CHECK(x) do { bool ok_ = (x); std::printf("%s %s\n", ok_ ? "ok  " : "FAIL", #x); if (!ok_) ++fails; } while (0)

static std::vector<char> slurp(const wchar_t* p) {
    std::ifstream in{fs::path{p}, std::ios::binary};
    return {std::istreambuf_iterator<char>(in), {}};
}

static char* map_image(const std::vector<char>& file) {
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(file.data());
    auto* nt  = reinterpret_cast<const IMAGE_NT_HEADERS64*>(file.data() + dos->e_lfanew);
    auto* base = static_cast<char*>(VirtualAlloc(reinterpret_cast<void*>(nt->OptionalHeader.ImageBase),
                                                 nt->OptionalHeader.SizeOfImage, MEM_RESERVE | MEM_COMMIT,
                                                 PAGE_EXECUTE_READWRITE));
    if (!base) return nullptr;
    std::memcpy(base, file.data(), nt->OptionalHeader.SizeOfHeaders);
    auto* sec = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec)
        if (sec->SizeOfRawData)
            std::memcpy(base + sec->VirtualAddress, file.data() + sec->PointerToRawData,
                        std::min<DWORD>(sec->SizeOfRawData, sec->Misc.VirtualSize));
    // .rdata read-only, like the real mapping
    DWORD old;
    VirtualProtect(base + 0x2D44000, 0x1000, PAGE_READONLY, &old);
    return base;
}

// Fake archive: every virtual returns 0 (so IsLoading() is false); Write()
// at +0x98 records what it was asked to write.
static const void* g_written = nullptr;
static uint32_t g_written_size = 0;
static uintptr_t noop(void*, void*, uint64_t, uint64_t) { return 0; }
static uintptr_t write_fn(void*, const void* data, uint32_t size) { g_written = data; g_written_size = size; return 0; }

static std::wstring name_at(const char* rec) {
    std::wstring n;
    for (int j = 0; j < 30; j += 2) { wchar_t c = (unsigned char)rec[j] | ((unsigned char)rec[j + 1] << 8); if (!c) break; n += c; }
    return n;
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 4) return 2;
    log_open("championship_test.log");
    fs::path root = fs::temp_directory_path() / "wmb_champ_test";
    std::error_code ec; fs::remove_all(root, ec); fs::create_directories(root);
    Paths p; p.azimuth = root; p.data = root;
    p.original = root / "orig.bin"; p.override_ = root / "override.bin";
    fs::copy_file(argv[2], p.original);

    // Override: id N gets "FriendNN".
    auto cache = slurp(argv[2]);
    const size_t R = 0x2658, H = 8, n = (cache.size() - H) / R;
    for (size_t i = 0; i < n; ++i) {
        char* rec = cache.data() + H + i * R; uint32_t id; std::memcpy(&id, rec + 0x2650, 4);
        std::wstring nm = L"Friend" + std::to_wstring(100 + id).substr(1);
        std::memset(rec, 0, 32);
        for (size_t j = 0; j < nm.size(); ++j) { rec[j * 2] = (char)nm[j]; }
    }
    { std::ofstream o{p.override_, std::ios::binary}; o.write(cache.data(), cache.size()); }

    auto exe = slurp(argv[1]);
    char* base = map_image(exe);
    CHECK(base == reinterpret_cast<char*>(0x140000000));
    if (!base) return 1;
    void** slot = reinterpret_cast<void**>(base + 0x2D44F20) + 2;
    void* original = *slot;
    CHECK(original == base + 0xA68870);

    start_championship_fix(p);
    for (int i = 0; i < 50 && *slot == original; ++i) Sleep(100);
    CHECK(*slot != original);

    // State object as the game lays it out: count at +0x1C, vector at +0x20.
    auto save = slurp(argv[3]);
    const int count = 11;
    std::vector<char> recs(save.begin() + 0x54, save.begin() + 0x54 + count * R);
    alignas(16) char state[0x40]{};
    *reinterpret_cast<int*>(state + 0x1C) = count;
    char* vb = recs.data();
    std::memcpy(state + 0x20, &vb, 8);
    char* ve = vb + recs.size();
    std::memcpy(state + 0x28, &ve, 8); std::memcpy(state + 0x30, &ve, 8);

    void* vt[0x80]; for (auto& f : vt) f = (void*)&noop; vt[0x98 / 8] = (void*)&write_fn;
    void* archive[2] = {vt, nullptr};

    std::vector<std::wstring> before;
    for (int i = 0; i < count; ++i) before.push_back(name_at(vb + i * R));
    using Fn = char (*)(void*, void*);
    char r = reinterpret_cast<Fn>(*slot)(state, archive);
    CHECK(r == 1);
    CHECK(g_written == vb && g_written_size == count * R); // the real Serialize ran
    bool all = true;
    for (int i = 0; i < count; ++i) {
        uint32_t id; std::memcpy(&id, vb + i * R + 0x2650, 4);
        std::wstring want = L"Friend" + std::to_wstring(100 + id).substr(1);
        std::printf("  %-15ls -> %ls\n", before[i].c_str(), name_at(vb + i * R).c_str());
        all = all && name_at(vb + i * R) == want;
    }
    CHECK(all);

    // Mod turned off (no override): names go back to the originals.
    fs::remove(p.override_);
    update_championship_names(p);
    reinterpret_cast<Fn>(*slot)(state, archive);
    bool back = true;
    for (int i = 0; i < count; ++i) back = back && name_at(vb + i * R) == before[i];
    CHECK(back);

    // A record that isn't a copy of a built-in Drivatar is left alone.
    std::memset(vb + 0x100, 0x5A, 200);
    fs::copy_file(argv[2], p.override_, fs::copy_options::overwrite_existing);
    { std::ofstream o{p.override_, std::ios::binary}; o.write(cache.data(), cache.size()); }
    update_championship_names(p);
    reinterpret_cast<Fn>(*slot)(state, archive);
    CHECK(name_at(vb) == before[0]);
    CHECK(name_at(vb + R).rfind(L"Friend", 0) == 0);

    fs::remove_all(root, ec);
    std::printf(fails ? "\n%d FAILED\n" : "\nALL PASSED\n", fails);
    return fails;
}
