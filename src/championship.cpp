// Championship Drivatars.
//
// Free roam takes its Drivatars straight from DefaultCache.bin, which the
// override renames. A championship doesn't: when you start one, the game
// copies the Drivatars it picked into your save (SaveStates::
// ChampionshipDrivatarsState, also used for the Storm Island championships),
// and every race of it, and every later session, reloads them from there.
// So championships started before the mod, or before a friends-list change,
// keep the names they were saved with.
//
// Fix: each of those saved records is a byte-for-byte copy of a DefaultCache
// record, including the Drivatar's id (1..25) near the end. We swap the
// state's Serialize entry in its vtable for a wrapper that runs the original
// and then gives every record the name that Drivatar id currently has in
// the cache file the game is using (the override, or the original when the
// mod is off). The save itself is never touched on disk; it simply picks up
// the new names the next time the game saves the championship.

#include "common.hpp"

#include <atomic>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <thread>

namespace wmb {

namespace fs = std::filesystem;

namespace {

// forza_durango_release_final.exe (Xbox One build, 24 Nov 2015)
constexpr uint32_t kTimestamp   = 0x565501EDu;
constexpr uint32_t kSizeOfImage = 0x03A18000u;
constexpr uint32_t kStateVtable = 0x02D44F20u; // SaveStates::ChampionshipDrivatarsState
constexpr uint32_t kSerialize   = 0x00A68870u; // its vtable[2], Serialize(archive)
constexpr std::size_t kSerializeSlot = 2;

// ChampionshipDrivatarsState: int count at +0x1C, std::vector<Record> at +0x20.
constexpr std::size_t kCountOffset  = 0x1C;
constexpr std::size_t kVectorOffset = 0x20;

// Drivatar record (same layout in DefaultCache.bin and in the save).
constexpr std::size_t kHeader    = 8;
constexpr std::size_t kRecord    = 0x2658;
constexpr std::size_t kNameBytes = 32;
constexpr std::size_t kIdOffset  = 0x2650;
// A saved copy differs from its cache record in a handful of bytes (4 in
// practice); anything far off isn't one of the built-in Drivatars.
constexpr std::size_t kMaxDiff = 64;

using SerializeFn = char (*)(void* self, void* archive);
SerializeFn g_original = nullptr;

std::mutex g_mu;
std::vector<char> g_cache;                    // original DefaultCache.bin
std::multimap<uint32_t, std::size_t> g_by_id; // id -> record index in g_cache
std::map<uint32_t, std::wstring> g_names;     // id -> name it should have now

uint32_t rd32(const char* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

std::wstring name_of(const char* rec) {
    std::wstring n;
    for (std::size_t j = 0; j + 1 < kNameBytes; j += 2) {
        const wchar_t c = static_cast<wchar_t>(static_cast<unsigned char>(rec[j]) |
                                               (static_cast<unsigned char>(rec[j + 1]) << 8));
        if (!c) break;
        n.push_back(c);
    }
    return n;
}

std::vector<char> read_cache(const fs::path& f) {
    std::ifstream in{f, std::ios::binary};
    std::vector<char> d((std::istreambuf_iterator<char>(in)), {});
    if (d.size() < kHeader || rd32(d.data()) != 4) return {};
    const uint32_t count = rd32(d.data() + 4);
    if (!count || d.size() != kHeader + count * kRecord) return {};
    return d;
}

std::map<uint32_t, std::wstring> names_by_id(const std::vector<char>& d) {
    std::map<uint32_t, std::wstring> m;
    const std::size_t count = (d.size() - kHeader) / kRecord;
    for (std::size_t i = 0; i < count; ++i) {
        const char* rec = d.data() + kHeader + i * kRecord;
        m.emplace(rd32(rec + kIdOffset), name_of(rec));
    }
    return m;
}

// Committed and accessible for reading (and writing, if `write`).
bool accessible(const void* p, std::size_t n, bool write) {
    constexpr DWORD kWritable = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    constexpr DWORD kReadable = kWritable | PAGE_READONLY | PAGE_EXECUTE_READ;
    auto a         = reinterpret_cast<uintptr_t>(p);
    const auto end = a + n;
    while (a < end) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(reinterpret_cast<void*>(a), &mbi, sizeof(mbi)) != sizeof(mbi)) return false;
        if (mbi.State != MEM_COMMIT || (mbi.Protect & PAGE_GUARD) ||
            !(mbi.Protect & (write ? kWritable : kReadable)))
            return false;
        a = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    }
    return true;
}

// Is this saved record a copy of built-in Drivatar `id`?
bool is_builtin_copy(const char* rec, uint32_t id) {
    auto [lo, hi] = g_by_id.equal_range(id);
    for (auto it = lo; it != hi; ++it) {
        const char* ref  = g_cache.data() + kHeader + it->second * kRecord;
        std::size_t diff = 0;
        for (std::size_t k = kNameBytes; k < kRecord && diff <= kMaxDiff; ++k) diff += rec[k] != ref[k];
        if (diff <= kMaxDiff) return true;
    }
    return false;
}

void rename_records(void* self) {
    auto* s = static_cast<char*>(self);
    if (!accessible(s, kVectorOffset + 8, false)) return;
    const int count = *reinterpret_cast<int*>(s + kCountOffset);
    char* vec       = *reinterpret_cast<char**>(s + kVectorOffset);
    if (count <= 0 || count > 256 || !vec || !accessible(vec, count * kRecord, true)) return;

    std::scoped_lock lk{g_mu};
    int renamed = 0;
    for (int i = 0; i < count; ++i) {
        char* rec      = vec + i * kRecord;
        const auto id  = rd32(rec + kIdOffset);
        const auto it  = g_names.find(id);
        if (it == g_names.end() || !is_builtin_copy(rec, id)) continue;
        const auto old = name_of(rec);
        if (old == it->second) continue;
        std::memset(rec, 0, kNameBytes);
        for (std::size_t j = 0; j < it->second.size() && j < kMaxGamertag; ++j) {
            rec[j * 2]     = static_cast<char>(it->second[j] & 0xFF);
            rec[j * 2 + 1] = static_cast<char>((it->second[j] >> 8) & 0xFF);
        }
        log("championship Drivatar {:<15} -> {}", to_utf8(old), to_utf8(it->second));
        ++renamed;
    }
    if (renamed) log("renamed {} of {} saved championship Drivatars", renamed, count);
}

char serialize_hook(void* self, void* archive) {
    const char r = g_original(self, archive);
    rename_records(self);
    return r;
}

bool header_matches(const char* base) {
    if (!accessible(base, 0x1000, false)) return false;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 64 || dos->e_lfanew > 0x800) return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    return nt->Signature == IMAGE_NT_SIGNATURE && nt->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 &&
           nt->FileHeader.TimeDateStamp == kTimestamp && nt->OptionalHeader.SizeOfImage == kSizeOfImage;
}

// Azimuth maps the game into its own process; find it by its PE header.
char* find_game() {
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    auto addr       = reinterpret_cast<uintptr_t>(si.lpMinimumApplicationAddress);
    const auto last = reinterpret_cast<uintptr_t>(si.lpMaximumApplicationAddress);
    MEMORY_BASIC_INFORMATION mbi{};
    while (addr < last && VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi)) == sizeof(mbi)) {
        const auto next = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        if (mbi.State == MEM_COMMIT && mbi.BaseAddress == mbi.AllocationBase && mbi.RegionSize >= 0x1000 &&
            !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) && header_matches(static_cast<char*>(mbi.BaseAddress)))
            return static_cast<char*>(mbi.BaseAddress);
        if (next <= addr) break;
        addr = next;
    }
    return nullptr;
}

bool patch_vtable(char* base) {
    auto* slot           = reinterpret_cast<void**>(base + kStateVtable) + kSerializeSlot;
    void* const expected = base + kSerialize;
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(slot, &mbi, sizeof(mbi)) != sizeof(mbi) || mbi.State != MEM_COMMIT) return false;
    if (*slot == reinterpret_cast<void*>(&serialize_hook)) return true;
    if (*slot != expected) {
        log("championship fix: unexpected game code, not installed");
        return false;
    }
    g_original = reinterpret_cast<SerializeFn>(expected);
    DWORD old  = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
    InterlockedExchangePointer(slot, reinterpret_cast<void*>(&serialize_hook));
    DWORD ignored = 0;
    VirtualProtect(slot, sizeof(void*), old, &ignored);
    return true;
}

} // namespace

void update_championship_names(const Paths& p) {
    std::error_code ec;
    auto current = fs::exists(p.override_, ec) ? read_cache(p.override_) : std::vector<char>{};
    std::scoped_lock lk{g_mu};
    if (g_cache.empty()) {
        g_cache = read_cache(p.original);
        g_by_id.clear();
        const std::size_t count = g_cache.empty() ? 0 : (g_cache.size() - kHeader) / kRecord;
        for (std::size_t i = 0; i < count; ++i)
            g_by_id.emplace(rd32(g_cache.data() + kHeader + i * kRecord + kIdOffset), i);
    }
    g_names = names_by_id(current.empty() ? g_cache : current);
}

void start_championship_fix(const Paths& p) {
    update_championship_names(p);
    {
        std::scoped_lock lk{g_mu};
        if (g_cache.empty()) {
            log("championship fix: couldn't read the original DefaultCache.bin, not installed");
            return;
        }
    }
    std::thread([] {
        // Azimuth maps the game when you hit Play, and the save (with the
        // championship) loads a few seconds later, so poll until it's there.
        for (;;) {
            if (char* base = find_game()) {
                if (patch_vtable(base))
                    log("championship fix installed (game at {})", static_cast<void*>(base));
                return;
            }
            Sleep(100);
        }
    }).detach();
}

} // namespace wmb
