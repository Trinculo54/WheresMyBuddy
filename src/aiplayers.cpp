// AI player names (car meets, road trips, ...).
//
// Besides the Drivatar list, FH2 has a plain string table of AI driver
// names, AIPlayers.str, inside each language's string table zip. It holds
// the same 25 gamertags plus "HRZN Ben", and it's what car meets and some
// road-trip spawns name their drivers from. We hand FH2ML the same swaps as
// the Drivatar list; it writes them for every language into Extra Options'
// mediaoverride folder, merged with any other mod's edits.

#include "common.hpp"
#include "platform_link.hpp"

namespace wmb {

void write_ai_players(const Paths&, const std::map<std::wstring, std::wstring>& rename) {
    const Fh2ml* f = fh2ml();
    if (!f) {
        log("AI driver names (car meets, road trips) skipped: not running under FH2ML");
        return;
    }
    f->S->ClearStrings(f->mod, "AIPlayers.str");
    for (const auto& [from, to] : rename) f->S->ReplaceString(f->mod, "AIPlayers.str", from.c_str(), to.c_str());
    f->S->ApplyStrings(f->mod);
    if (rename.empty())
        log("AI driver names back to the originals");
    else
        log("AI driver names (car meets, road trips): {} swapped", rename.size());
}

} // namespace wmb
