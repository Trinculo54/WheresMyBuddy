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

#include "common.hpp"
#include "fh2_strings.hpp"

namespace wmb {

void write_ai_players(const Paths& p, const std::map<std::wstring, std::wstring>& rename) {
    if (p.mount.empty()) return;
    if (rename.empty()) {
        if (fh2common::override_string_table(p.mount, "AIPlayers.str", nullptr, true))
            log("removed the AI driver name overrides");
        return;
    }
    const int n = fh2common::override_string_table(p.mount, "AIPlayers.str",
                                                   [&](uint16_t, const std::wstring& t) {
                                                       auto it = rename.find(t);
                                                       return it == rename.end() ? t : it->second;
                                                   });
    log("wrote AI driver names (car meets, road trips) for {} languages", n);
}

} // namespace wmb
