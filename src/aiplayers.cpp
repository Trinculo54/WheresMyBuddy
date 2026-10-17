// AI player names (car meets, road trips, ...).
//
// Besides the Drivatar list, FH2 has a plain string table of AI driver
// names, AIPlayers.str, inside each language's string table zip
// (media\Stripped\StringTables\<lang>.zip). It holds the same 25 gamertags
// plus "HRZN Ben", and it's what car meets and some road-trip spawns name
// their drivers from. We read it out of every language zip, swap the names
// the same way as the Drivatar list, and write it to Extra Options'
// mediaoverride folder (mediaoverride\Stripped\StringTables\<lang>\
// AIPlayers.str), so the zips themselves are never touched.
//
// AIPlayers.str is an "LSB2" string table:
//   "LSB2", u32 0, u32 0x200, u32 header size (0x24), u32 section count (4),
//   u32 section offsets[4] (0 = absent)
//   section 0 (strings): u32 size (incl. itself), u32 count,
//                        (count+1) x { u16 key, u32 offset in UTF-16 units }
//                        (the last one is a 0xFFFF sentinel holding the
//                        total length), then the NUL-terminated UTF-16 strings
//   sections 1-3: string ids, comment, key order; copied unchanged.

#include "common.hpp"

#include <cstring>
#include <fstream>
#include <map>

namespace wmb {

namespace fs = std::filesystem;

namespace {

// ---- raw DEFLATE (RFC 1951) ----------------------------------------------------

struct Inflater {
    const unsigned char* in;
    std::size_t len, pos = 0;
    uint32_t bitbuf = 0;
    int bitcnt = 0;
    std::vector<unsigned char> out;
    bool bad = false;

    int bits(int need) {
        uint32_t v = bitbuf;
        while (bitcnt < need) {
            if (pos >= len) {
                bad = true;
                return 0;
            }
            v |= static_cast<uint32_t>(in[pos++]) << bitcnt;
            bitcnt += 8;
        }
        bitbuf = v >> need;
        bitcnt -= need;
        return static_cast<int>(v & ((1u << need) - 1));
    }

    struct Huffman {
        short count[16]{};
        short symbol[288]{};
    };

    static bool build(Huffman& h, const short* lengths, int n) {
        std::memset(h.count, 0, sizeof(h.count));
        for (int s = 0; s < n; ++s) h.count[lengths[s]]++;
        if (h.count[0] == n) return true; // no codes: only valid if never used
        int left = 1;
        for (int l = 1; l < 16; ++l) {
            left <<= 1;
            left -= h.count[l];
            if (left < 0) return false; // over-subscribed
        }
        short offs[16]{};
        for (int l = 1; l < 15; ++l) offs[l + 1] = offs[l] + h.count[l];
        for (int s = 0; s < n; ++s)
            if (lengths[s]) h.symbol[offs[lengths[s]]++] = static_cast<short>(s);
        return true;
    }

    int decode(const Huffman& h) {
        int code = 0, first = 0, index = 0;
        for (int l = 1; l < 16; ++l) {
            code |= bits(1);
            if (bad) return -1;
            const int count = h.count[l];
            if (code - count < first) return h.symbol[index + (code - first)];
            index += count;
            first += count;
            first <<= 1;
            code <<= 1;
        }
        bad = true;
        return -1;
    }

    bool codes(const Huffman& lit, const Huffman& dist) {
        static const short lbase[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                        31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
        static const short lext[29]  = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                        2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
        static const short dbase[30] = {1,   2,   3,   4,   5,   7,    9,    13,   17,   25,
                                        33,  49,  65,  97,  129, 193,  257,  385,  513,  769,
                                        1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
        static const short dext[30]  = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
                                        6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
        for (;;) {
            int sym = decode(lit);
            if (sym < 0) return false;
            if (sym < 256) {
                out.push_back(static_cast<unsigned char>(sym));
            } else if (sym == 256) {
                return true;
            } else {
                sym -= 257;
                if (sym >= 29) return false;
                const int n = lbase[sym] + bits(lext[sym]);
                const int ds = decode(dist);
                if (ds < 0 || ds >= 30) return false;
                const std::size_t d = static_cast<std::size_t>(dbase[ds] + bits(dext[ds]));
                if (bad || d > out.size()) return false;
                for (int i = 0; i < n; ++i) out.push_back(out[out.size() - d]);
            }
        }
    }

    bool fixed() {
        static Huffman lit, dist;
        static bool built = false;
        if (!built) {
            short l[288];
            int s = 0;
            for (; s < 144; ++s) l[s] = 8;
            for (; s < 256; ++s) l[s] = 9;
            for (; s < 280; ++s) l[s] = 7;
            for (; s < 288; ++s) l[s] = 8;
            build(lit, l, 288);
            for (s = 0; s < 30; ++s) l[s] = 5;
            build(dist, l, 30);
            built = true;
        }
        return codes(lit, dist);
    }

    bool dynamic() {
        static const short order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
        const int nlen = bits(5) + 257, ndist = bits(5) + 1, ncode = bits(4) + 4;
        if (bad || nlen > 286 || ndist > 30) return false;
        short lengths[320]{};
        for (int i = 0; i < ncode; ++i) lengths[order[i]] = static_cast<short>(bits(3));
        Huffman lencode, lit, dist;
        if (bad || !build(lencode, lengths, 19)) return false;
        int i = 0;
        while (i < nlen + ndist) {
            int sym = decode(lencode);
            if (sym < 0) return false;
            if (sym < 16) {
                lengths[i++] = static_cast<short>(sym);
            } else {
                short v = 0;
                int rep;
                if (sym == 16) {
                    if (i == 0) return false;
                    v   = lengths[i - 1];
                    rep = 3 + bits(2);
                } else if (sym == 17) {
                    rep = 3 + bits(3);
                } else {
                    rep = 11 + bits(7);
                }
                if (bad || i + rep > nlen + ndist) return false;
                while (rep--) lengths[i++] = v;
            }
        }
        if (lengths[256] == 0) return false;
        if (!build(lit, lengths, nlen) || !build(dist, lengths + nlen, ndist)) return false;
        return codes(lit, dist);
    }

    bool run() {
        for (;;) {
            const int last = bits(1), type = bits(2);
            if (bad) return false;
            bool ok;
            if (type == 0) {
                bitbuf = 0;
                bitcnt = 0;
                if (pos + 4 > len) return false;
                const unsigned n = in[pos] | (in[pos + 1] << 8);
                pos += 4;
                if (pos + n > len) return false;
                out.insert(out.end(), in + pos, in + pos + n);
                pos += n;
                ok = true;
            } else if (type == 1) {
                ok = fixed();
            } else if (type == 2) {
                ok = dynamic();
            } else {
                return false;
            }
            if (!ok) return false;
            if (last) return true;
        }
    }
};

// ---- zip ---------------------------------------------------------------------

uint16_t r16(const std::vector<char>& d, std::size_t o) {
    return static_cast<uint16_t>(static_cast<unsigned char>(d[o]) | (static_cast<unsigned char>(d[o + 1]) << 8));
}
uint32_t r32(const std::vector<char>& d, std::size_t o) {
    return static_cast<uint32_t>(r16(d, o)) | (static_cast<uint32_t>(r16(d, o + 2)) << 16);
}

bool ieq(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    return true;
}

// Extracts one entry from a zip, or empty if it isn't there.
std::vector<char> unzip_entry(const fs::path& zip, std::string_view name) {
    std::ifstream in{zip, std::ios::binary};
    std::vector<char> d((std::istreambuf_iterator<char>(in)), {});
    if (d.size() < 22) return {};
    std::size_t eocd = std::string::npos;
    for (std::size_t i = d.size() - 22 + 1; i-- > 0 && d.size() - i < 22 + 65536;)
        if (r32(d, i) == 0x06054b50) {
            eocd = i;
            break;
        }
    if (eocd == std::string::npos) return {};
    const std::size_t entries = r16(d, eocd + 10);
    std::size_t cd            = r32(d, eocd + 16);
    for (std::size_t e = 0; e < entries && cd + 46 <= d.size() && r32(d, cd) == 0x02014b50; ++e) {
        const uint16_t method = r16(d, cd + 10);
        const uint32_t csize = r32(d, cd + 20), usize = r32(d, cd + 24);
        const uint16_t nlen = r16(d, cd + 28), xlen = r16(d, cd + 30), clen = r16(d, cd + 32);
        const uint32_t local = r32(d, cd + 42);
        if (cd + 46 + nlen > d.size()) return {};
        const std::string_view n{d.data() + cd + 46, nlen};
        cd += 46 + nlen + xlen + clen;
        if (!ieq(n, name)) continue;
        if (local + 30 > d.size() || r32(d, local) != 0x04034b50) return {};
        const std::size_t data = local + 30 + r16(d, local + 26) + r16(d, local + 28);
        if (data + csize > d.size()) return {};
        if (method == 0) return {d.begin() + data, d.begin() + data + csize};
        if (method != 8) return {};
        Inflater inf{reinterpret_cast<const unsigned char*>(d.data() + data), csize};
        if (!inf.run() || inf.out.size() != usize) return {};
        return {inf.out.begin(), inf.out.end()};
    }
    return {};
}

// ---- LSB2 --------------------------------------------------------------------

void w16(std::vector<char>& d, std::size_t o, uint32_t v) {
    d[o]     = static_cast<char>(v & 0xFF);
    d[o + 1] = static_cast<char>((v >> 8) & 0xFF);
}
void w32(std::vector<char>& d, std::size_t o, uint32_t v) {
    w16(d, o, v & 0xFFFF);
    w16(d, o + 2, v >> 16);
}

} // namespace

std::vector<char> rename_lsb2(const std::vector<char>& t, const std::map<std::wstring, std::wstring>& rename,
                              int* changed) {
    if (changed) *changed = 0;
    if (t.size() < 0x24 || std::memcmp(t.data(), "LSB2", 4) != 0) return {};
    const uint32_t hdr = r32(t, 0x0C), nsec = r32(t, 0x10);
    if (hdr < 0x14 + 4 * nsec || hdr > t.size() || nsec == 0) return {};
    const uint32_t s0 = r32(t, 0x14);
    if (s0 + 8 > t.size()) return {};
    const uint32_t size0 = r32(t, s0), count = r32(t, s0 + 4);
    const std::size_t table = s0 + 8, strings = table + (count + 1) * 6;
    if (s0 + size0 > t.size() || strings > s0 + size0 || count > 100000) return {};

    // Pull the strings out by their offsets.
    std::vector<uint16_t> keys(count + 1);
    std::vector<std::wstring> text(count);
    for (uint32_t i = 0; i <= count; ++i) keys[i] = r16(t, table + i * 6);
    for (uint32_t i = 0; i < count; ++i) {
        std::size_t o = strings + 2 * static_cast<std::size_t>(r32(t, table + i * 6 + 2));
        std::wstring s;
        while (o + 1 < s0 + size0 && r16(t, o)) {
            s.push_back(static_cast<wchar_t>(r16(t, o)));
            o += 2;
        }
        auto it = rename.find(s);
        if (it != rename.end() && it->second != s) {
            s = it->second;
            if (changed) ++*changed;
        }
        text[i] = s;
    }

    // New section 0.
    std::vector<char> sec(8 + (count + 1) * 6);
    uint32_t off = 0;
    std::vector<char> blob;
    for (uint32_t i = 0; i < count; ++i) {
        w16(sec, 8 + i * 6, keys[i]);
        w32(sec, 8 + i * 6 + 2, off);
        for (wchar_t c : text[i]) {
            blob.push_back(static_cast<char>(c & 0xFF));
            blob.push_back(static_cast<char>((c >> 8) & 0xFF));
        }
        blob.push_back(0);
        blob.push_back(0);
        off += static_cast<uint32_t>(text[i].size() + 1);
    }
    w16(sec, 8 + count * 6, keys[count]);
    w32(sec, 8 + count * 6 + 2, off);
    sec.insert(sec.end(), blob.begin(), blob.end());
    w32(sec, 0, static_cast<uint32_t>(sec.size()));
    w32(sec, 4, count);

    std::vector<char> out(t.begin(), t.begin() + s0);
    out.insert(out.end(), sec.begin(), sec.end());
    out.insert(out.end(), t.begin() + s0 + size0, t.end());
    const auto delta = static_cast<int64_t>(sec.size()) - static_cast<int64_t>(size0);
    for (uint32_t k = 1; k < nsec; ++k) {
        const uint32_t o = r32(t, 0x14 + 4 * k);
        if (o > s0) w32(out, 0x14 + 4 * k, static_cast<uint32_t>(o + delta));
    }
    return out;
}

std::vector<char> unzip_file(const std::filesystem::path& zip, std::string_view name) {
    return unzip_entry(zip, name);
}

void write_ai_players(const Paths& p, const std::map<std::wstring, std::wstring>& rename) {
    std::error_code ec;
    const fs::path tables   = p.mount / "media" / "Stripped" / "StringTables";
    const fs::path override = p.mount / "mediaoverride" / "Stripped" / "StringTables";
    if (!fs::exists(tables, ec)) return;
    int written = 0, langs = 0;
    for (const auto& e : fs::directory_iterator(tables, ec)) {
        if (!e.is_regular_file(ec) || !ieq(e.path().extension().string(), ".zip")) continue;
        const auto lang   = e.path().stem();
        const auto target = override / lang / "AIPlayers.str";
        if (rename.empty()) {
            if (fs::remove(target, ec)) ++written;
            continue;
        }
        const auto orig = unzip_entry(e.path(), "AIPlayers.str");
        if (orig.empty()) continue;
        ++langs;
        int changed     = 0;
        const auto data = rename_lsb2(orig, rename, &changed);
        if (data.empty()) {
            log("AIPlayers.str in {} isn't in the expected format", to_utf8(e.path().filename().wstring()));
            continue;
        }
        fs::create_directories(target.parent_path(), ec);
        std::ofstream out{target, std::ios::binary | std::ios::trunc};
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
        if (out) ++written;
    }
    if (rename.empty()) {
        if (written) log("removed the AI driver name overrides");
    } else {
        log("wrote AI driver names (car meets, road trips) for {} of {} languages", written, langs);
    }
}

} // namespace wmb
